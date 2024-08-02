#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "tight.h"


/* error message format */
#define ERR_MSG(err)	TIGHT_NAME ": " err ".\n"

/* tight error */
#define terror(err)			tprint(ERR_MSG(err))
#define terrorf(fmt, ...)	tprintf(ERR_MSG(fmt), __VA_ARGS__)


/* cleanup and exit */
#define tdie(ts) \
	{ tight_free(ts); if (t_outfile) free(t_outfile); exit(EXIT_FAILURE); }


typedef unsigned char uchar;


/* command line context */
typedef struct CLI_context {
	tight_State *ts; /* state for errors */
	const char *infile; /* input file */
	const char *outfile; /* output file */
	uchar huffman; /* use huffman coding */
	uchar lzw; /* use lzw */
	uchar decode; /* decode */
} CLIctx;


/* output filename */
static char *t_outfile = NULL;


/* character frequencies (for huffman) */
static size_t t_frequencies[256];



/* memory allocator */
static void *trealloc(void *block, void *ud, size_t os, size_t ns) {
	(void)ud; /* unused */
	(void)os; /* unused */
	if (ns == 0) {
		free(block);
		return NULL;
	}
	return realloc(block, ns);
}


/* debug messages writer */
static void tprint(const char *msg) {
	fputs(msg, stderr);
	fflush(stderr);
}


/* write formatted debug messages */
static void tprintf(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}


/* print usage */
static void usage(void) {
	tprint(
		"usage: tight [-dhl] [INFILE] [OUTFILE]\n"
		"\n"
		"\t-d  decode INFILE into OUTFILE\n"
		"\t-h  use huffman encoding\n"
		"\t-l  use lzw encoding\n"
	);
}


/* print copyright */
static void copyright(void) {
	tprint(TIGHT_COPYRIGHT "\n");
}


/* print version */
static void version(void) {
	tprint("version: " TIGHT_RELEASE);
}


/* 
 * Make output file from 'infile' + '.tit' suffix.
 * Resulting filename is stored in global 't_outfile'.
 */
static void makeoutfile(tight_State *ts, const char *infile) {
	size_t len = strlen(infile);
	const char *outfile = trealloc(NULL, NULL, 0, len + sizeof(".tit"));
	if (outfile == NULL) {
		terror("out of memory, can't allocate storage for output filename");
		tight_free(ts);
		exit(ENOMEM);
	}
	memcpy(t_outfile, infile, len);
	memcpy(&t_outfile[len], ".tit", sizeof(".tit"));
}


/* 'parseargs' returns */
#define argsok		0
#define argserr		1
#define argsexit	2

/* parse cli args */
static int parseargs(CLIctx *ctx, int argc, const char **argv) {
#define jmpifhaveopt(arg,i,l)		if (arg[++i] != '\0') goto l

	size_t i;
	int nomoreopts;

	nomoreopts = 0;
	while (argc-- > 0) {
		const char *arg = *argv++;
		switch (arg[0]) {
		case '-':
			if (nomoreopts) 
				goto filearg;
			i = 1;
readmore:
			switch(arg[i]) {
			case '-':  /* '--' */
				nomoreopts = 1;
				break;
			case 'c': /* use huffman coding */
				ctx->huffman = 1;
				jmpifhaveopt(arg, i, readmore);
				break;
			case 'l': /* use lzw */
				ctx->lzw = 1;
				jmpifhaveopt(arg, i, readmore);
				break;
			case 'd': /* decode */
				ctx->decode = 1;
				jmpifhaveopt(arg, i, readmore);
				break;
			case 'C': /* display copyright */
				copyright();
				return argsexit;
				break;
			case 'h': /* usage/help */
				usage();
				return argsexit;
			case 'v':
				version();
				return argsexit;
			default:
				terrorf("unknown option '-%c'", arg[i]);
				return argserr;
			}
			break;
		default:
filearg:
			if (ctx->infile && ctx->outfile) {
				terror("already have input and output file arguments");
				return argserr;
			} else if (ctx->infile) {
				ctx->outfile = arg;
			} else {
				ctx->infile = arg;
			}
			break;
		}
	}
	if (!ctx->infile) { /* missing input file ? */
		terror("missing input file");
		return argserr;
	} else if (!ctx->outfile) { /* missing output file ? */
		makeoutfile(ctx->ts, ctx->infile);
		ctx->outfile = t_outfile;
	}
	return argsok;

#undef jmpifhaveopt
}


/* auxiliary function to 'open' syscall */
static int openfile(const char *file, int mode, int perms) {
	int fd = open(file, mode, perms);
	if (fd < 0)
		terrorf("error while opening file '%s': %s", file, strerror(errno));
	return fd;
}


typedef struct tfile_buffer {
	tight_State *ts; /* for panic */
	uchar buf[TIGHT_RBUFFSIZE];
	uchar *p; /* current position in 'buf' */
	ssize_t n; /* how many chars left in 'buf' */
	int fd; /* file descriptor */
	int wfd; /* for close in case of errors */
} tfilebuff;


/* get character */
#define tgetchar(fb)		((fb)->n-- > 0 ? *(fb)->p++ : fillfilebuf(fb))


static void initfb(tight_State *ts, tfilebuff *fb, int fd, int wfd) {
	fb->ts = ts;
	fb->p = fb->buf;
	fb->n = 0;
	fb->fd = fd;
	fb->wfd = wfd;
}


/* file 'tfilebuff' */
static int fillfilebuf(tfilebuff *fb) {
	fb->n = read(fb->fd, fb->buf, sizeof(fb->buf));
	if (fb->n < 0) {
		terrorf("read error (input file): %s", strerror(errno));
		close(fb->fd); close(fb->wfd);
		tdie(fb->ts);
	}
	if (fb->n == 0)
		return EOF;
	fb->p = fb->buf;
	fb->n--;
	return *fb->p++;
}


/* get character frequencies from file descriptor 'rfd' */
static void getfrequencies(tight_State *ts, int rfd, int wfd) {
	tfilebuff fb; /* file buffer */
	int c;

	initfb(ts, &fb, rfd, wfd);
	memset(t_frequencies, 0, 256 * sizeof(size_t));;
	while ((c = tgetchar(&fb)) != EOF)
		t_frequencies[c]++;
	if (lseek(rfd, 0, SEEK_SET) < 0) {
		terrorf("seek error input file: %s", strerror(errno));
		close(rfd); close(wfd);
		tdie(ts);
	}
}


/* get encoding/decoding mode */
static inline int getmode(CLIctx *ctx) {
	int mode = (ctx->huffman * TIGHT_HUFFMAN) | (ctx->lzw * TIGHT_RLE);
	/* TODO(jure): implement LZW */
	if (!mode) mode = TIGHT_DEFAULT;
	return mode;
}



/* cleanup with status 'c' */
#define tdefer(c) \
	{ status = (c); goto cleanup; }


/* tight */
int main(int argc, const char **argv) {
	CLIctx ctx;
	int status = TIGHT_OK;
	tight_State *ts = tight_new(trealloc, NULL);
	if (ts == NULL) {
		terror("couldn't allocate state");
		exit(EXIT_FAILURE);
	}
	memset(&ctx, 0, sizeof(ctx));
	ctx.ts = ts;
	int res = parseargs(&ctx, --argc, ++argv);
	if (res == argserr)
		status = EXIT_FAILURE;
	if (status == EXIT_FAILURE || res == argsexit)
		goto cleanup;
	int rfd = openfile(ctx.infile, O_RDONLY, 0);
	int wfd = openfile(ctx.outfile, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	tight_setfiles(ts, rfd, wfd);
	if (ctx.decode) { 
		status = tight_decompress(ts);
	} else { 
		size_t *freqs = NULL;
		int mode = getmode(&ctx);
		if (mode & TIGHT_HUFFMAN) {
			getfrequencies(ts, rfd, wfd);
			freqs = t_frequencies;
		}
		status = tight_compress(ts, mode, freqs);
	}
	if (status != TIGHT_OK) /* error ? */
		terrorf("%s", tight_geterror(ts));
	close(rfd); close(wfd);
cleanup:
	tight_free(ts);
	exit(status);
}
