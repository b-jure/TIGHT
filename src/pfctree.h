#ifndef PFCTREE_H
#define PFCTREE_H

#include "pfc.h"


/* list for memory cleanup */
#define CleanupList		struct Tree *next

typedef struct Tree {
	CleanupList;
	const struct Tree *left;
	const struct Tree *right;
	short b; /* byte value */
	p_byte branches; /* weight */
} Tree;


Tree *pfcT_newleaf(pfc_State *ps, p_byte b, p_byte weight);
Tree *pfcT_newroot(pfc_State *ps, const Tree *t1, const Tree *t2);

#endif
