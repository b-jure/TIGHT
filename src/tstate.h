#ifndef TIGHTSTATE_H
#define TIGHTSTATE_H

/* 
 * TODO(jure): add additional information to 'TIGHT' header.
 *				- Field/s describing which compression algorithms were
 *				  used to compress data.
 *				- Field describing OS (portability reasons)
 */

#include <setjmp.h>
#include <stdarg.h>

#include "talloc.h"
#include "tight.h"
#include "tinternal.h"
#include "tmd5.h"
#include "ttree.h"


/* size of 'tight_State' */
#define SIZEOFSTATE			sizeof(tight_State)


/* offset in header for which MD5 digest is valid */
#define TIGHTbindataoffset		((off_t)offsetof(TIGHT, bindata))


/* maximum bits in huffman code */
#define MAXCODE			16

/* check 'encodeeof' */
#define EOFBIAS			6

/* in how many bits 'eof' is encoded */
#define EOFBITS			3



/* MAGIC global, defined in 'tstate.c' */
extern const byte MAGIC[8];


/* internal header (actual memory representation) */
typedef struct TIGHT_header {
	byte magic[8]; /* prefix for 'tight' compressed files */
	byte version[3]; /* version string (x.y.z) */
	byte os; /* operating system */
	byte mode; /* compression mode */
	byte bindata; /* binary data start, true if present */
	byte checksum[16]; /* checksum of 'bindata' */
} TIGHT;



/* protected function, for error recovery */
typedef void (*fProtected)(tight_State *ts, void *ud);

/* error recovery */
typedef struct Tightjmpbuf {
	jmp_buf buf;
	va_list ap; /* cleanup */
	byte haveap; /* true if 'ap' is valid */
} Tightjmpbuf;



/* Huffman code */
typedef struct HuffCode {
	int nbits; /* how many bits in 'code' */
	int code; /* huffman code */
} HuffCode;


/* state */
struct tight_State {
	tight_fRealloc frealloc; /* memory allocator */
	void *ud; /* userdata for 'frealloc' */
	char *error; /* error string */
	TempMem *temp; /* temporary memory to clean */
	TreeData *hufftree; /* huffman tree */
	HuffCode codes[TIGHTBYTES]; /* huffman codes */
	Tightjmpbuf *errjmp; /* for error recovery */
	int rfd; /* file descriptor open for reading */
	int wfd; /* file descriptor open for writing */
	volatile int status; /* status code */
};


TIGHT_FUNC t_noret tightS_throw(tight_State *ts, int err);
TIGHT_FUNC void tightS_gencodes(tight_State *ts, const size_t *freqs);
TIGHT_FUNC void tightS_poptemp(tight_State *ts);
TIGHT_FUNC int tightS_protectedcall(tight_State *ts, void *ud, fProtected fn);

#endif
