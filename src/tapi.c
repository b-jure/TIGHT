#include <string.h>
#include <unistd.h>

#include "pfc.h"
#include "pfcinternal.h"
#include "pfcstate.h"


/* create state */
TIGHT_API tight_State *tight_new(tight_fRealloc frealloc, void *userdata) {
	tight_State *ts = (tight_State *)frealloc(NULL, userdata, 0, SIZEOFSTATE);
	if (t_unlikely(ts == NULL))
		return NULL;
	ts->frealloc = frealloc;
	ts->ud = userdata;
	ts->fmsg = NULL;
	ts->fpanic = NULL;
	ts->hufftree = NULL;
	memset(ts->codes, 0, sizeof(ts->codes));
	ts->ncodes = 0;
	ts->rfd = ts->wfd = -1;
	return ts;
}


/* set error writer */
TIGHT_API tight_fError tight_seterror(tight_State *ts, tight_fError fmsg) {
	tight_fError old = ts->fmsg;
	ts->fmsg = fmsg;
	return old;
}


/* set panic handler */
TIGHT_API tight_fPanic tight_setpanic(tight_State *ts, tight_fPanic fpanic) {
	tight_fPanic old = ts->fpanic;
	ts->fpanic = fpanic;
	return old;
}


/* delete state */
TIGHT_API void tight_free(tight_State *ts) {
	if (ts->hufftree) tightT_freeparent(ts, ts->hufftree);
	if (ts->rfd != -1) close(ts->rfd);
	if (ts->wfd != -1) close(ts->wfd);
	ts->frealloc(ts, ts->ud, SIZEOFSTATE, 0);
}
