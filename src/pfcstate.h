#ifndef PFCSTATE_H
#define PFCSTATE_H

#include "pfc.h"
#include "pfcinternal.h"
#include "pfctree.h"


/* size of 'pfc_State' */
#define SIZEOFSTATE		sizeof(pfc_State)


/* 'magic' in 'PFCHeader' */
#define PFC_MAGIC		0x706663


/* maximum 'nbits' in huffman 'code' */
#define MAXCODEBITS		15



/* pfc compressed files header */
typedef struct PFCHeader {
	int magic; /* prefix for 'pfc' compressed files */
	size_t freq[PFC_BYTES];
	int checksum; /* checksum of header */
} PFCHeader;


/* huffman code */
typedef struct HuffCode {
	int nbits; /* how many bits in 'code' */
	int code; /* huffman code */
} HuffCode;


/* state */
struct pfc_State {
	pfc_fError fmsg; /* debug message writer */
	pfc_fPanic fpanic; /* panic handler */
	pfc_fRealloc frealloc; /* memory allocator */
	void *ud; /* userdata for 'frealloc' */
	int ncodes; /* number of elements in 'codes' */
	HuffCode codes[PFC_BYTES]; /* huffman codes */
};


void pfcS_gencodes(pfc_State *ps, size_t *freqs);

#endif
