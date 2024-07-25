#ifndef TIGHTALLOC_H
#define TIGHTALLOC_H

#include "tstate.h"


#define tightA_growvec(ts,b,s,l,n) \
	((b) = tightA_growarray_(ts, b, &(s), l, n, sizeof(*b)))

#define tightA_ensurevec(th,b,sz,l,n,e) \
	((b) = tightA_ensurearray_(th, b, &(sz), l, n, e, sizeof(*b)))

#define tightA_freevec(ts,b,s)	tightA_free(ts, b, (s) * sizeof(*(b)))


void *tightA_realloc(tight_State *ts, void *block, size_t osize, size_t nsize);
void *tightA_malloc(tight_State *ts, size_t size);
void tightA_free(tight_State *ts, void *block, size_t osize);
void *tightA_growarray_(tight_State *ts, void *block, uint *sizep, uint limit,
					  uint nelems, uint elemsize);
void *tightA_ensurearray_(tight_State *tight, void *block, uint *sizep, uint limit,
						uint nelems, uint ensure, uint elemsize);

#endif
