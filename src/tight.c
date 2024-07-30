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


/* command line context */
typedef struct CLI_context {
	const char *infile; /* input file */
	const char *outfile; /* output file */
	unsigned char huffman; /* use huffman coding */
	unsigned char lzw; /* use lzw */
	unsigned char decode; /* decode */
} CLIctx;


/* tight error */
#define terror(err)			tprint(ERR_MSG(err))
#define terrorf(fmt, ...)	tprintf(ERR_MSG(fmt), __VA_ARGS__)


/* memory allocator */
static void *trealloc(void *block, void *ud, size_t os, size_t ns) {
	(void)ud;
	(void)os;
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


/* panic handler */
static void tpanic(tight_State *ts) {
	tight_free(ts);
	exit(EXIT_FAILURE);
}


/* print usage */
static void usage(void) {
	tprint(
		TIGHT_COPYRIGHT "\n"
		"usage: tight [INFILE] [OUTFILE]\n"
	);
}


/* 'parseargs' return */
#define argsok		0
#define argserr		1
#define argshelp	2

/* check if we have more opts */
#define jmpifhaveopt(arg,i,l)		if (arg[++i] != '\0') goto l

/* parse cli args */
static int parseargs(CLIctx *ctx, int argc, const char **argv) {
	size_t i;
	int infile, outfile, nomoreopts;

	infile = outfile = nomoreopts = 0;
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
			case 'h': /* usage/help */
				usage();
				return argshelp;
			default:
				terrorf("unknown option '-%c'", arg[i]);
				return argserr;
			}
			break;
		default:
filearg:
			if (infile && outfile) {
				terror("already have input and output file arguments");
				return argserr;
			} else if (infile) {
				ctx->outfile = arg;
				outfile = 1;
			} else {
				ctx->infile = arg;
				infile = 1;
			}
			break;
		}
	}
	if (!infile || !outfile) { /* missing required arguments ? */
		terrorf("missing %s file", infile ? "output" : "input");
		return argserr;
	}
	return argsok;
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
	unsigned char buf[TIGHT_RBUFFSIZE];
	unsigned char *p; /* current position in 'buf' */
	ssize_t n; /* how many chars left in 'buf' */
	int fd; /* file descriptor */
} tfilebuff;


/* get character */
#define tgetchar(fb)		((fb)->n-- > 0 ? *(fb)->p++ : fillfilebuf(fb))


static void initfb(tight_State *ts, tfilebuff *fb, int fd) {
	fb->ts = ts;
	fb->p = fb->buf;
	fb->n = 0;
	fb->fd = fd;
}


/* file 'tfilebuff' */
static int fillfilebuf(tfilebuff *fb) {
	fb->n = read(fb->fd, fb->buf, sizeof(fb->buf));
	if (fb->n < 0) 
		tpanic(fb->ts);
	if (fb->n == 0)
		return EOF;
	fb->p = fb->buf;
	fb->n--;
	return *fb->p++;
}


/* get character frequencies from file descriptor 'fd' */
static void getfrequencies(tight_State *ts, int fd, size_t *freqs) {
	tfilebuff fb; /* file buffer */
	int c;

	initfb(ts, &fb, fd);
	memset(freqs, 0, 256 * sizeof(size_t));;
	while ((c = tgetchar(&fb)) != EOF)
		freqs[c]++;
}


/* get encoding/decoding mode */
static inline int getmode(CLIctx *ctx) {
	int mode = (ctx->huffman * MODEHUFF) | (ctx->lzw * MODELZW);
	if (!mode) {
		/* TODO(jure): implement LZW and set to 'MODEALL' */
		mode = MODEHUFF;
	}
	return mode;
}


/* tight */
int main(int argc, const char **argv) {
	size_t freqs[256]; /* frequencies for huffman */
	CLIctx ctx;
	int mode, status;

	status = EXIT_SUCCESS;
	tight_State *ts = tight_new(trealloc, NULL);
	if (ts == NULL) {
		terror("couldn't allocate state");
		exit(EXIT_FAILURE);
	}
	memset(&ctx, 0, sizeof(ctx));
	int res = parseargs(&ctx, --argc, ++argv);
	if (res == argserr)
		status = EXIT_FAILURE;
	if (status == EXIT_FAILURE || res == argshelp)
		goto cleanup;
	tight_seterror(ts, tprint);
	tight_setpanic(ts, tpanic);
	int rfd = openfile(ctx.infile, O_RDONLY, 0);
	int wfd = openfile(ctx.outfile, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	tight_setfiles(ts, rfd, wfd);
	if (ctx.decode) { /* decode */
		tight_decode(ts);
	} else { /* encode */
		mode = getmode(&ctx);
		if (mode & MODEHUFF) {
			getfrequencies(ts, rfd, freqs);
			if (lseek(rfd, 0, SEEK_SET) < 0)
				terrorf("seek error input file: %s", strerror(errno));
			tight_encode(ts, mode, freqs);
		} else {
			tight_encode(ts, mode, NULL);
		}
	}
	close(rfd); close(wfd);
cleanup:
	tight_free(ts);
	exit(status);
}
