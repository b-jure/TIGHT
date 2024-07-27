#ifndef TIGHTSTATE_H
#define TIGHTSTATE_H

/* 
 * TODO(jure): add additional information to 'TIGHT' header.
 *				- Field/s describing which compression algorithms were
 *				  used to compress data.
 *				- Field describing OS (portability reasons)
 * 				- Checksum algorithm, describing which algorithm was used to
 * 				  generate 'checksum'.
 * 				- Size of 'checksum', depending on the algorithm used the size
 * 				  of checksum can vary in size, or maybe pick maximum size
 *				  and pad rest with 0's.
 */

#include "tight.h"
#include "tinternal.h"
#include "tmd5.h"
#include "ttree.h"


/* size of 'tight_State' */
#define SIZEOFSTATE		sizeof(tight_State)


/* 'magic' as hex literal (padded with 0's) */
#define TIGHTMAGIC		0x0000005448474954


/* offset in header for which MD5 digest is valid */
#define TIGHTbindataoffset		offsetof(TIGHT, bindata)


/* maximum bits in huffman code */
#define MAXCODE			16


/* check 'encodeeof' */
#define EOFBIAS			6


/* in how many bits 'eof' is encoded */
#define EOFBITS			3



/* MAGIC global, defined in 'tstate.c' */
extern const byte MAGIC[8];


/* 
 * TIGHT header for compressed files.
 * This structure is not used internally and is
 * only defined to calculate offsets and provide
 * layout of header for clarity purposes.
 */
typedef struct TIGHT_header {
	byte magic[8]; /* prefix for 'tight' compressed files */
	byte *bindata; /* encoded Huffman tree */
	byte checksum[16]; /* MD5 digest of 'bindata' */
} TIGHT;


/* Huffman code */
typedef struct HuffCode {
	int nbits; /* how many bits in 'code' */
	int code; /* huffman code */
} HuffCode;


/* state */
struct tight_State {
	tight_fRealloc frealloc; /* memory allocator */
	void *ud; /* userdata for 'frealloc' */
	tight_fError fmsg; /* debug message writer */
	tight_fPanic fpanic; /* panic handler */
	TreeData *hufftree; /* huffman tree */
	HuffCode codes[TIGHTBYTES]; /* huffman codes */
	int rfd; /* file descriptor open for reading */
	int wfd; /* file descriptor open for writing */
};

#endif
