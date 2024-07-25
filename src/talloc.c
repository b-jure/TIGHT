#include "talloc.h"
#include "tdebug.h"


/* realloc */
void *tightA_realloc(tight_State *ts, void *block, size_t osize, size_t nsize) {
	block = ts->frealloc(block, ts->ud, osize, nsize);
	if (t_unlikely(block == NULL))
		tightD_memerror(ts);
	return block;
}


/* malloc */
void *tightA_malloc(tight_State *ts, size_t size) {
	void *block = ts->frealloc(NULL, ts->ud, 0, size);
	if (t_unlikely(block == NULL))
		tightD_memerror(ts);
	return block;
}


/* free */
void tightA_free(tight_State *ts, void *block, size_t osize) {
	ts->frealloc(block, ts->ud, osize, 0);
}



/* minimum initial size for arrays */
#define MINARRAYSIZE	8


/* grow array if needed */
void *tightA_growarray_(tight_State *ts, void *block, uint *sizep, uint limit,
					  uint nelems, uint elemsize)
{
	uint size = *sizep;
	if (t_likely(nelems + 1 <= size))
		return block;
	if (t_unlikely(size << 1 > limit)) {
		if (t_unlikely(size >= limit))
			tightD_errorf(ts, "array size limit reached %u", limit);
		size = limit;
		t_assert(size >= MINARRAYSIZE);
	} else {
		size <<= 1;
		if (size < MINARRAYSIZE)
			size = MINARRAYSIZE;
	}
	block = tightA_realloc(ts, block, *sizep * elemsize, size * elemsize);
	*sizep = size;
	return block;
}


/* ensure array has space for at least 'nelems' + 'ensure' elements */
void *tightA_ensurearray_(tight_State *tight, void *block, uint *sizep, uint limit,
						uint nelems, uint ensure, uint elemsize)
{
	uint size = *sizep;

	if (t_unlikely(limit - nelems < ensure)) /* would overflow? */
		tightD_errorf(tight, "array size limit reached %u", limit);
	ensure += nelems;
	if (ensure <= size)
		return block;
	if (t_unlikely(size >= (limit >> 1))) {
		if (t_unlikely(size >= limit))
			tightD_errorf(tight, "array size limit reached %u", limit);
		size = limit;
		t_assert(size >= MINARRAYSIZE);
	} else {
		if (size < MINARRAYSIZE)
			size = MINARRAYSIZE;
		while (size < ensure)
			size <<= 1;
	}
	block = tightA_realloc(tight, block, *sizep * elemsize, size * elemsize);
	*sizep = size;
	return block;
}
