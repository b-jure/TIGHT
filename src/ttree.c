/*****************************************
 * Copyright (C) 2024 Jure B.
 * Refer to 'tight.h' for license details.
 *****************************************/

#include "ttree.h"
#include "talloc.h"
#include "tdebug.h"


/* make new leaf tree */
TreeData *tightT_newleaf(tight_State *ts, size_t freq, ushrt c) {
	TreeData *tl = tightA_malloc(ts, sizeof(*tl));
	tl->left = tl->right = NULL;
	tl->freq = freq; 
	tl->c = c;
	return tl;
}


/* make new parent tree */
TreeData *tightT_newparent(tight_State *ts, TreeData *t1, TreeData *t2, ushrt idx) {
	TreeData *tp = tightA_malloc(ts, sizeof(*tp));
	tp->left = t1;
	tp->right = t2;
	if (t_unlikely(t1->freq > SIZE_MAX - t2->freq)) { /* overflow ? */
		tightA_free(ts, tp, sizeof(*tp));
		tightD_limiterror(ts, "frequency", SIZE_MAX);
	}
	tp->freq = t1->freq + t2->freq;
	tp->c = idx;
	return tp;
}


/* free parent and its children */
void tightT_freeparent(tight_State *ts, TreeData *tdroot) {
	if (tdroot->left)
		tightT_freeparent(ts, tdroot->left);
	if (tdroot->right)
		tightT_freeparent(ts, tdroot->right);
	tightA_free(ts, tdroot, sizeof(*tdroot));
}
