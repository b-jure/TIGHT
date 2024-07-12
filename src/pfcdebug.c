#include "pfcdebug.h"
#include "pfcstate.h"
#include "pfcalloc.h"


void pfcD_freealltrees(pfc_State *ps)
{
	Tree *t;

	for (t = ps->cleanuplist; t != NULL; t = t->next)
		pfcA_free(ps, t, sizeof(Tree));
}


pfc_noret pfcD_error(pfc_State *ps, const char *err)
{
	pfc_assert(ps != NULL);
	pfc_assert(err != NULL);
	ps->fmsg(ps, err);
	pfcD_freealltrees(ps);
	ps->fpanic(ps);
}
