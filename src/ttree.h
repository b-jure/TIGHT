#ifndef TIGHTTREE_H
#define TIGHTTREE_H

#include "tight.h"
#include "tinternal.h"


/* header encoding and decoding structure */
typedef struct TreeData {
	struct TreeData *left;
	struct TreeData *right;
	size_t freq; /* frequency */
	ushrt c; /* symbol */
} TreeData;


TreeData *tightT_newleaf(tight_State *ts, size_t freq, ushrt c);
TreeData *tightT_newparent(tight_State *ts, TreeData *t1, TreeData *t2, ushrt idx);
void tightT_freeparent(tight_State *ts, TreeData *tdroot);

#endif
