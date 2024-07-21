#include <string.h>

#include "pfc.h"
#include "pfcinternal.h"
#include "pfcstate.h"


/* create state */
PFC_API pfc_State *pfc_new(pfc_fRealloc frealloc, void *userdata) {
	pfc_State *ps = (pfc_State *)frealloc(NULL, userdata, 0, SIZEOFSTATE);
	if (pfc_unlikely(ps == NULL))
		return NULL;
	ps->frealloc = frealloc;
	ps->ud = userdata;
	ps->fmsg = NULL;
	ps->fpanic = NULL;
	ps->ncodes = 0;
	memset(ps->codes, 0, sizeof(ps->codes));
	return ps;
}


/* set error writer */
PFC_API pfc_fError pfc_seterror(pfc_State *ps, pfc_fError fmsg) {
	pfc_fError old = ps->fmsg;
	ps->fmsg = fmsg;
	return old;
}


/* set panic handler */
PFC_API pfc_fPanic pfc_setpanic(pfc_State *ps, pfc_fPanic fpanic) {
	pfc_fPanic old = ps->fpanic;
	ps->fpanic = fpanic;
	return old;
}


/* delete state */
PFC_API void pfc_free(pfc_State *ps) {
	ps->frealloc(ps, ps->ud, SIZEOFSTATE, 0);
}
