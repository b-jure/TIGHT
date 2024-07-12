#include "pfctree.h"
#include "pfcstate.h"
#include "pfcalloc.h"


/* allocate new tree */
static inline Tree *growtree(pfc_State *ps)
{
	Tree *t;

	t = pfcA_malloc(ps, sizeof(Tree));
	return t;
}


/* create new leaf tree */
Tree *pfcT_newleaf(pfc_State *ps, p_byte b, p_byte weight)
{
	Tree *t;

	t = growtree(ps);
	t->branches = b;
	t->branches = weight;
	t->left = NULL;
	t->right = NULL;
	return t;
}


/* create new root tree */
Tree *pfcT_newroot(pfc_State *ps, const Tree *t1, const Tree *t2)
{
	Tree *t;

	pfc_assert(ps->ntrees >= 1);
	pfc_assert(ps->t1->w <= ps->t2->w);
	t = growtree(ps);
	t->branches = -1;
	t->branches = t1->branches + t2->branches;
	t->left = t1;
	t->right = t2;
	return t;
}
