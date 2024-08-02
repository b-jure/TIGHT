#include "talloc.h"
#include "tdebug.h"



/* create new temporary memory */
TempMem *tightA_newtempmem(tight_State *ts) {
	TempMem *tm = tightA_malloc(ts, sizeof(*tm));
	tm->size = 0; 
	tm->mem = NULL;
	tm->next = ts->temp;
	ts->temp = tm;
	return tm;
}


/* free temporary memory */
void tightA_freetempmem(tight_State *ts, TempMem *tm) {
	t_assert(tm != NULL);
	if (t_likely(tm->mem)) {
		t_assert(tm->size > 0);
		tightA_free(ts, tm->mem, tm->size);
	}
	tightA_free(ts, tm, sizeof(*tm));
}


/* realloc */
void *tightA_realloc(tight_State *ts, void *block, size_t osize, size_t nsize) {
	block = ts->frealloc(block, ts->ud, osize, nsize);
	if (t_unlikely(block == NULL))
		tightS_throw(ts, TIGHT_ERRMEM);
	return block;
}


/* malloc */
void *tightA_malloc(tight_State *ts, size_t size) {
	void *block = ts->frealloc(NULL, ts->ud, 0, size);
	if (t_unlikely(block == NULL))
		tightS_throw(ts, TIGHT_ERRMEM);
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
					  uint nelems, uint elemsize, const char *what)
{
	uint size = *sizep;
	if (t_likely(nelems + 1 <= size))
		return block;
	if (t_unlikely(size << 1 > limit)) {
		if (t_unlikely(size >= limit))
			tightD_limiterror(ts, what, limit);
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
void *tightA_ensurearray_(tight_State *ts, void *block, uint *sizep, uint limit,
						uint nelems, uint ensure, uint elemsize, const char *what)
{
	uint size = *sizep;

	if (t_unlikely(limit - nelems < ensure)) /* would overflow? */
		goto limerr;
	ensure += nelems;
	if (ensure <= size)
		return block;
	if (t_unlikely(size >= (limit >> 1))) {
		if (t_unlikely(size >= limit))
			goto limerr;
		size = limit;
		t_assert(size >= MINARRAYSIZE);
	} else {
		if (size < MINARRAYSIZE)
			size = MINARRAYSIZE;
		while (size < ensure)
			size <<= 1;
	}
	block = tightA_realloc(ts, block, *sizep * elemsize, size * elemsize);
	*sizep = size;
	return block;
limerr:
	tightD_limiterror(ts, what, limit); 
	return NULL; /* UNREACHED */
}
