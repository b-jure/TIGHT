#ifndef TIGHTALLOC_H
#define TIGHTALLOC_H

#include "tight.h"
#include "tinternal.h"

#define tightA_growvec(ts,b,s,l,n,w) \
	((b) = tightA_growarray_(ts, b, &(s), l, n, sizeof(*b), w))

#define tightA_ensurevec(th,b,sz,l,n,e,w) \
	((b) = tightA_ensurearray_(th, b, &(sz), l, n, e, sizeof(*b), w))

#define tightA_freevec(ts,b,s)	tightA_free(ts, b, (s) * sizeof(*(b)))



/* update TempMem */
#define updatetm(tm,p,s)		((tm)->mem = (p), (tm)->size = (s))


/*
 * Used when allocating in order to properly
 * cleanup temporary memory in case of memory errors.
 * This kind of memory is anchored to 'tight_State'.
 */
typedef struct TempMem {
	struct TempMem *next; /* list */
	size_t size; /* size of 'mem' */
	void *mem; /* memory block */
} TempMem;


TIGHT_FUNC TempMem *tightA_newtempmem(tight_State *ts);
TIGHT_FUNC void tightA_freetempmem(tight_State *ts, TempMem *tm);

TIGHT_FUNC void *tightA_malloc(tight_State *ts, size_t size);
TIGHT_FUNC void tightA_free(tight_State *ts, void *block, size_t osize);
TIGHT_FUNC void *tightA_realloc(tight_State *ts, void *block, size_t osize,
								size_t nsize);

TIGHT_FUNC void *tightA_growarray_(tight_State *ts, void *block, uint *sizep,
								   uint limit, uint nelems, uint elemsize, 
								   const char *what);

TIGHT_FUNC void *tightA_ensurearray_(tight_State *tight, void *block, uint *sizep,
									 uint limit, uint nelems, uint ensure, 
									 uint elemsize, const char *what);

#endif
