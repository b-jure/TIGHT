#ifndef PFCALLOC_H
#define PFCALLOC_H

#include "pfcstate.h"


#define pfcA_growvec(ps,b,s,l,n) \
	((b) = pfcA_growarray_(ps, b, &(s), l, n, sizeof(*b)))

#define pfcA_ensurevec(th,b,sz,l,n,e) \
	((b) = pfcA_ensurearray_(th, b, &(sz), l, n, e, sizeof(*b)))

#define pfcA_freevec(ps,b,s)	pfcA_free(ps, b, (s) * sizeof(*(b)))


void *pfcA_realloc(pfc_State *ps, void *block, size_t osize, size_t nsize);
void *pfcA_malloc(pfc_State *ps, size_t size);
void pfcA_free(pfc_State *ps, void *block, size_t osize);
void *pfcA_growarray_(pfc_State *ps, void *block, size_t *sizep, size_t limit,
					  size_t nelems, int elemsize);
void *pfcA_ensurearray_(pfc_State *pfc, void *block, size_t *sizep, 
						size_t limit, size_t nelems, size_t ensure,
						size_t elemsize);

#endif
