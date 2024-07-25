#ifndef TIGHTSTATE_H
#define TIGHTSTATE_H

#include "tight.h"
#include "tinternal.h"
#include "tmd5.h"
#include "ttree.h"


/* size of 'tight_State' */
#define SIZEOFSTATE		sizeof(tight_State)


/* file format magic (padded with 0's) */
#define TIGHTMAGIC		0x0000005448474954


/* offset in header for which md5 checksum is valid */
#define TIGHTbindataoffset		offsetof(TIGHT, bindata)



/* MAGIC global */
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
	word checksum[4]; /* MD5 checksum of 'bindata' */
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
	int ncodes; /* number of elements in 'codes' */
	int rfd; /* file descriptor open for reading */
	int wfd; /* file descriptor open for writing */
};

#endif
