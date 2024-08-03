/*****************************************
 * Copyright (C) 2024 Jure B.
 * Refer to 'tight.h' for license details.
 *****************************************/

#define _POSIX_C_SOURCE		199309L

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

#include "tight.h"


/* message format */
#define MSGFMT(msg)			TIGHT_NAME ": " msg ".\n"


/* tight error */
#define terror(err)			tprint(stderr, MSGFMT(err))
#define terrorf(fmt, ...)	tprintf(stderr, MSGFMT(fmt), __VA_ARGS__)


/* 'open' error */
#define openerror(filename)	\
	terrorf("error while opening '%s': %s", filename, strerror(errno))


/* cleanup, do 'pre' and exit */
#define tdie_(pre, ts) \
	{ pre; tight_free(ts); if (t_outfile) free(t_outfile); exit(EXIT_FAILURE); }


/* die with error message */
#define tdie(ts, err)			tdie_(tprint(stderr, MSGFMT(err)), ts)

/* die with formatted error message */
#define tdief(ts, fmt, ...)		tdie_(tprintf(stderr, MSGFMT(fmt), __VA_ARGS__), ts)


/* get absolute value */
#define tabs(x)			((x) < 0 ? -(x) : (x))



typedef unsigned char uchar;


/* command line context */
typedef struct CLI_context {
	tight_State *ts; /* state for errors */
	const char *infile; /* input file */
	const char *outfile; /* output file */
	uchar huffman; /* use huffman coding */
	uchar rle; /* use rle */
	uchar decompress; /* decompress */
	uchar time; /* time the execution */
	uchar verbose; /* verbose output */
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
static void tprint(FILE *fp, const char *msg) {
	fputs(msg, fp);
	fflush(fp);
}


/* write formatted debug messages */
static void tprintf(FILE *fp, const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(fp, fmt, ap);
	fflush(fp);
	va_end(ap);
}


/* print usage */
static void usage(void) {
	tprint(stdout,
		"usage: tight [-dhl] [INFILE] [OUTFILE]\n"
		"              -C  show copyright\n"
		"              -V  enable verbose output\n"
		"              -v  show version information\n"
		"              -h  show usage\n"
		"              -t  time the execution\n"
		"              -d  decompress INFILE into OUTFILE\n"
		"              -c  use huffman compression\n"
		"              -l  (NOT IMPLEMENTED) use run-length-encoding when compressing\n"
	);
}


/* print copyright */
static void copyright(void) {
	tprint(stdout, TIGHT_COPYRIGHT "\n");
}


/* print version */
static void version(void) {
	tprint(stdout, "tight version: " TIGHT_RELEASE "\n");
}


/* 
 * Make output file from 'infile' + '.tit' suffix.
 * Resulting filename is stored in global 't_outfile'.
 */
static void makeoutfile(tight_State *ts, const char *infile) {
	size_t len = strlen(infile);
	if (NAME_MAX - sizeof(".tit") < len) /* name size limit reached ? */
		len = NAME_MAX - sizeof(".tit");
	t_outfile = trealloc(NULL, NULL, 0, len + sizeof(".tit"));
	if (t_outfile == NULL)
		tdie(ts, "out of memory, can't allocate storage for output filename");
	memcpy(t_outfile, infile, len);
	memcpy(&t_outfile[len], ".tit", sizeof(".tit"));
}


/* ASSERT: !ctx->decompress; schizo entry: trying to make out, get it ???;
 * like kissing the file, HAHAHAAHGAHGHAGHA the glorious dungeater */
static int trymakeoutfile(CLIctx *ctx) {
	const char *p;
	if ((p = strrchr(ctx->infile, '.')) != NULL && !strcmp(++p, "tit")) {
		terrorf("file '%s' has '.tit' extension -- skipping", ctx->infile);
		return 1; /* argserr */
	}
	makeoutfile(ctx->ts, ctx->infile);
	ctx->outfile = t_outfile;
	return 0; /* argsok */
}



/* parse cli args */
static int parseargs(CLIctx *ctx, int argc, const char **argv) {
#define jmpifhaveopt(arg,i,l)		if (arg[++i] != '\0') goto l
#define argsok		0
#define argserr		1
#define argsexit	2

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
			case 'l': /* use rle */
				ctx->rle = 1;
				jmpifhaveopt(arg, i, readmore);
				break;
			case 'd': /* decode */
				ctx->decompress = 1;
				jmpifhaveopt(arg, i, readmore);
				break;
			case 't': /* decode */
				ctx->time = 1;
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
			case 'V':
				ctx->verbose = 1;
				jmpifhaveopt(arg, i, readmore);
				break;
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
		usage(); /* give a hint */
		return argserr;
	} else if (!ctx->outfile) { /* missing output file ? */
		if (ctx->decompress) {
			terror("missing decompression output file");
			return argserr;
		} else {
			return trymakeoutfile(ctx);
		}
	}
	return argsok;

#undef jmpifhaveopt
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
		close(fb->fd); close(fb->wfd);
		tdief(fb->ts, "read error (input file): %s", strerror(errno));
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
		close(rfd); close(wfd);
		tdief(ts, "seek error input file: %s", strerror(errno));
	}
}


/* get encoding/decoding mode */
static inline int getmode(CLIctx *ctx) {
	int mode = (ctx->huffman * TIGHT_HUFFMAN) | (ctx->rle * TIGHT_RLE);
	/* TODO(jure): implement LZW */
	if (!mode) 
		mode = TIGHT_DEFAULT;
	return mode;
}


/* print how much wall-clock time it took to count */
static inline void printmonoclock(struct timespec *end, struct timespec *start) {
	double elapsed = end->tv_sec - start->tv_sec;
	elapsed += (end->tv_nsec - start->tv_nsec) / 1000000000.0;
	tprintf(stdout, MSGFMT("took ~ %g [sec]"), elapsed);
}


/* get monotonic clock time */
static inline int tgettime(struct timespec *ts) {
	return clock_gettime(CLOCK_MONOTONIC, ts);
}


/* print size change (verbose) */
static inline void printsizechange(off_t insize, off_t outsize) {
	off_t size = insize - outsize;
	if (insize == 0) insize = 1;
	unsigned long percent = ((double)tabs(size) / (double)insize) * 100.0;
	tprintf(stdout, MSGFMT("size from %ld to %ld [%c%lu%%]"),
			insize, outsize, (size < 0 ? '+' : '-'), percent);
}


/* cleanup with status 'c' */
#define tdefer(c) \
	{ status = (c); goto cleanup; }


/* tight */
int main(int argc, const char **argv) {
	CLIctx ctx;
	int status = TIGHT_OK;
	struct timespec start, end;
	struct stat st;
	int rfd = -1, wfd = -1;

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

	int mode = TIGHT_NONE;
	if (!ctx.decompress) {
		mode = getmode(&ctx);
		if (mode == TIGHT_RLE) {
			terror("run-length-encoding not implemented");
			tdefer(EXIT_FAILURE);
		}
	}

	rfd = open(ctx.infile, O_RDONLY, 0);
	if (rfd < 0) {
		openerror(ctx.infile);
		tdefer(errno);
	}
	wfd = open(ctx.outfile, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	if (wfd < 0) {
		openerror(ctx.outfile);
		tdefer(errno);
	}
	tight_setfiles(ts, rfd, wfd);


	if (ctx.verbose && stat(ctx.infile, &st) < 0) {
		terrorf("stat '%s': %s", ctx.infile, strerror(errno));
		tdefer(errno);
	}

	if (ctx.time && tgettime(&start) < 0) {
		terrorf("clock_gettime: %s", strerror(errno));
		tdefer(errno);
	}

	if (ctx.verbose) {
		tprintf(stdout, MSGFMT("%s '%s' into '%s'.."), 
				ctx.decompress ? "decompressing" : "compressing",
				ctx.infile, ctx.outfile);
	}

	if (ctx.decompress) { /* decompress ? */
		status = tight_decompress(ts);
	} else { /* compress */
		size_t *freqs = NULL;
		if (mode & TIGHT_HUFFMAN) {
			getfrequencies(ts, rfd, wfd);
			freqs = t_frequencies;
		}
		status = tight_compress(ts, mode, freqs);
	}
	if (status != TIGHT_OK) { /* tightlib error ? */
		terrorf("%s", tight_geterror(ts));
		tdefer(status);
	}

	if (ctx.time) {
		if (tgettime(&end) < 0) {
			terrorf("clock_gettime: %s", strerror(errno));
			tdefer(errno);
		}
		printmonoclock(&end, &start);
	}

	if (ctx.verbose) {
		off_t insize = st.st_size;
		if (stat(ctx.outfile, &st) < 0) {
			terrorf("stat '%s': %s", ctx.outfile, strerror(errno));
			tdefer(errno);
		}
		off_t outsize = st.st_size;
		printsizechange(insize, outsize);
	}

cleanup:
	if (rfd > 0) 
		close(rfd);
	if (wfd > 0) 
		close(wfd);
	if (t_outfile != NULL)
		trealloc(t_outfile, NULL, strlen(t_outfile) + 1, 0);
	tight_free(ts);
	exit(status);
}
