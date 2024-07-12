#ifndef PFCSTATE_H
#define PFCSTATE_H

#include "pfc.h"
#include "pfctree.h"


#define SIZEOFSTATE		sizeof(pfc_State)


typedef struct pfc_State {
	pfc_MessageFn fmsg; /* debug message writer */
	pfc_PanicFn fpanic; /* panic handler */
	pfc_ReallocFn frealloc; /* memory allocator */
	void *ud; /* userdata for 'frealloc' */
	pfc_ReaderFn freader; /* file reader */
	void *rud; /* userdata for 'freader' */
	Tree *cleanuplist; /* list of all allocated trees */
	const Tree **forest; /* tree stack */
	size_t ntrees; /* amount of trees in 'forest' */
	size_t sizef; /* size of 'forest' */
} pfc_State;


void pfcS_delete(pfc_State *ps);
pfc_State *pfcS_new(pfc_ReallocFn frealloc, void *userdata, pfc_MessageFn fmsg,
					pfc_PanicFn fpanic);

void pfcS_shrinkforest(pfc_State *ps);
void pfcS_sortforest(pfc_State *ps);
void pcfS_growforest(pfc_State *ps, const Tree *t);
void pcfS_cutforest(pfc_State *ps);

#endif
