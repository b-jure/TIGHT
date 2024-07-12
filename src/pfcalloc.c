#include "pfcalloc.h"
#include "pfcdebug.h"


static void *tryagain(pfc_State *ps, void *block, size_t osize, size_t nsize) {
	pfcS_shrinkforest(ps);
	return ps->frealloc(block, ps->ud, osize, nsize);
}


void *pfcA_realloc(pfc_State *ps, void *block, size_t osize, size_t nsize) {
	block = ps->frealloc(block, ps->ud, osize, nsize);
	if (pfc_unlikely(block == NULL)) {
		block = tryagain(ps, block, osize, nsize);
		if (pfc_unlikely(block == NULL)) {
			pfcD_error(ps, "out of memory");
			pfc_assert(0);
		}
	}
	return block;
}


#define MINARRAYSIZE	8

void *pfcA_growarray_(pfc_State *ps, void *block, size_t *sizep, size_t limit,
					  size_t nelems, int elemsize)
{
	size_t size;

	size = *sizep;
	if (pfc_likely(nelems + 1 <= size))
		return block;
	if (pfc_unlikely(size << 1 > limit)) {
		if (pfc_unlikely(size >= limit)) {
			pfcD_error(ps, "array size limit reached");
			pfc_assert(0);
		} else {
			size = limit;
		}
	} else {
		size <<= 1;
	}
	if (size < MINARRAYSIZE)
		size = MINARRAYSIZE;
	block = pfcA_realloc(ps, block, *sizep, size);
	*sizep = size;
	return block;
}
