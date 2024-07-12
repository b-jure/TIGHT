#ifndef PFCALLOC_H
#define PFCALLOC_H

#include "pfcstate.h"


#define pfcA_malloc(ps,s)		pfcA_realloc(ps, NULL, 0, s)
#define pfcA_free(ps,b,os)		pfcA_realloc(ps, b, os, 0)

#define pfcA_growvec(ps,b,s,l,n) \
	((b) = pfcA_growarray_(ps, b, &(s), l, n, sizeof(*b)))

#define pfcA_freevec(ps,b,s)	pfcA_free(ps, b, (s) * sizeof(*(b)))


void *pfcA_realloc(pfc_State *ps, void *block, size_t osize, size_t nsize);
void *pfcA_growarray_(pfc_State *ps, void *block, size_t *sizep, size_t limit,
					  size_t nelems, int elemsize);

#endif
