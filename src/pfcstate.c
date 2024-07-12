#include "pfcstate.h"
#include "pfcalloc.h"
#include "pfctree.h"
#include "pfcdebug.h"

#include <string.h>



/* create state */
pfc_State *pfcS_new(pfc_ReallocFn frealloc, void *userdata, pfc_MessageFn fmsg,
					pfc_PanicFn fpanic) 
{
	pfc_State *ps;

	ps = (pfc_State *)frealloc(NULL, userdata, 0, SIZEOFSTATE);
	if (pfc_unlikely(ps == NULL))
		return NULL;
	ps->frealloc = frealloc;
	ps->ud = userdata;
	ps->fmsg = fmsg;
	ps->fpanic = fpanic;
	ps->cleanuplist = NULL;
	ps->forest = NULL;
	ps->ntrees = 0;
	ps->sizef = 0;
	ps->emergency = 0;
	return ps;
}


/* delete state */
void pfcS_delete(pfc_State *ps) {
	pfcD_freealltrees(ps);
	pfcA_freevec(ps, ps->forest, ps->sizef);
	ps->frealloc(ps, ps->ud, SIZEOFSTATE, 0);
}


/* 
 * Shrink 'forest' if size is more than 3 times
 * larger than the current use.
 * If 'forest' is being shrunk due to emergency (allocation failed),
 * then the limit is ignored and 'forest' will be shrunk as much
 * as possible.
 */
void pfcS_shrinkforest(pfc_State *ps) {
	size_t nsize;
	size_t limit;

	if (pfc_unlikely(ps->emergency && ps->sizef > ps->ntrees)) {
		nsize = ps->ntrees;
		goto realloc;
	} else {
		limit = (ps->ntrees >= SIZE_MAX / 3 ? SIZE_MAX : ps->ntrees * 3);
		if (ps->ntrees <= SIZE_MAX && (ps->sizef > limit)) {
			nsize = ps->ntrees << 1;
realloc:
			pfcA_realloc(ps, ps->forest, ps->sizef, nsize);
		}
	}
}


/* perform swap */
static inline void swaptree(const Tree **x, const Tree **y) {
	const Tree **temp;

	temp = x;
	*x = *y;
	*y = *temp;
}


/* 
 * This implementation choses pivot to be the middle
 * element in the list and the sorting is reversed. 
 * After each partitioning operation list is sorted as: 
 * ```
 * [pweight, [weights >= pweight], [pweight > weights]
 * ```
 * After that the pivot is moved into its place, so the final
 * order after partitioning operation is:
 * ```
 * [[weights >= pweight], pweight, [pweight > weights]]
 * ```
 */
static void pfcqsort(const Tree **v, size_t start, size_t end) {
	size_t i;
	size_t last;

	if (start < end) return;
    swaptree(&v[start], &v[(start + end) >> 1]); /* move pivot to start */
    last = start;
    for(i = last + 1; i <= end; i++)
        if (v[i]->branches > v[start]->branches)
            swaptree(&v[++last], &v[i]);
    swaptree(&v[start], &v[last]);
	pfcqsort(v, start, last - 1);
	pfcqsort(v, last + 1, end);
}


/* 
 * Sort 'forest' trees from highest weight
 * value to lowest.
 */
void pfcS_sortforest(pfc_State *ps) {
	pfcqsort(ps->forest, 0, ps->ntrees - 1);
}


/* return index where to insert tree in sorted list of trees */
static size_t getsortedindex(const Tree **p, size_t l, size_t h, const Tree *t) {
	size_t mid;

	while (l < h) {
		mid = (l + h) >> 1;
		if (p[mid]->branches > t->branches) l = mid;
		else h = mid;
	}
	return mid;
}


/* 
 * Inserts tree into the 'forest' making sure 'forest'
 * remains sorted.
 * Note: 'forest' must already be sorted!
 */
void pcfS_growforest(pfc_State *ps, const Tree *t) {
	size_t i;

	pfcA_growvec(ps, ps->forest, ps->sizef, SIZE_MAX, ps->ntrees);
	i = getsortedindex(ps->forest, 0, ps->ntrees, t);
	memmove(&ps->forest[i + 1], &ps->forest[i], ps->ntrees - i);
	ps->forest[i] = t;
	ps->ntrees++;
}


/*
 * Cuts the entire 'forest' by removing trees
 * from it and constructing the optimal encoding
 * tree.
 */
void pcfS_cutforest(pfc_State *ps) {
	const Tree *t1;
	const Tree *t2;

	pfcS_sortforest(ps);
	while (ps->ntrees > 1) {
		t1 = ps->forest[--ps->ntrees];
		t2 = ps->forest[--ps->ntrees];
		pcfS_growforest(ps, pfcT_newroot(ps, t1, t2));
	}
	pfc_assert(ps->ntrees == 1);
	pfcS_shrinkforest(ps);
}
