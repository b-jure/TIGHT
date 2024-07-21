#include "pfcalloc.h"
#include "pfcdebug.h"


/* realloc */
void *pfcA_realloc(pfc_State *ps, void *block, size_t osize, size_t nsize) {
	block = ps->frealloc(block, ps->ud, osize, nsize);
	if (pfc_unlikely(block == NULL))
		pfcD_memerror(ps);
	return block;
}


/* malloc */
void *pfcA_malloc(pfc_State *ps, size_t size) {
	void *block = ps->frealloc(NULL, ps->ud, 0, size);
	if (pfc_unlikely(block == NULL))
		pfcD_memerror(ps);
	return block;
}


/* free */
void pfcA_free(pfc_State *ps, void *block, size_t osize) {
	ps->frealloc(block, ps->ud, osize, 0);
}



/* minimum initial size for arrays */
#define MINARRAYSIZE	8


/* grow array if needed */
void *pfcA_growarray_(pfc_State *ps, void *block, size_t *sizep, size_t limit,
					  size_t nelems, int elemsize)
{
	size_t size = *sizep;
	if (pfc_likely(nelems + 1 <= size))
		return block;
	if (pfc_unlikely(size << 1 > limit)) {
		if (pfc_unlikely(size >= limit))
			pfcD_error(ps, "array size limit reached %S", limit);
		size = limit;
		pfc_assert(size >= MINARRAYSIZE);
	} else {
		size <<= 1;
		if (size < MINARRAYSIZE)
			size = MINARRAYSIZE;
	}
	block = pfcA_realloc(ps, block, *sizep * elemsize, size * elemsize);
	*sizep = size;
	return block;
}


/* ensure array has space for at least 'nelems' + 'ensure' elements */
void *pfcA_ensurearray_(pfc_State *pfc, void *block, size_t *sizep, 
						size_t limit, size_t nelems, size_t ensure,
						size_t elemsize)
{
	size_t size = *sizep;

	if (pfc_unlikely(limit - nelems < ensure)) /* would overflow? */
		pfcD_error(pfc, "array size limit reached %S", limit);
	ensure += nelems;
	if (ensure <= size)
		return block;
	if (pfc_unlikely(size >= (limit >> 1))) {
		if (pfc_unlikely(size >= limit))
			pfcD_error(pfc, "array size limit reached %S", limit);
		size = limit;
		pfc_assert(size >= MINARRAYSIZE);
	} else {
		if (size < MINARRAYSIZE)
			size = MINARRAYSIZE;
		while (size < ensure)
			size <<= 1;
	}
	block = pfcA_realloc(pfc, block, *sizep * elemsize, size * elemsize);
	*sizep = size;
	return block;
}
