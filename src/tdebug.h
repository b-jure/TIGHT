#ifndef TIGHTDEBUG_H
#define TIGHTDEBUG_H

#include <errno.h>

#include "tight.h"
#include "tinternal.h"


/* debug message format */
#define MSGFMT(msg)		TIGHT_NAME ": " msg ".\n"


/* invoke and print error */
#define tightD_error(ts,emsg)			tightD_error_(ts, MSGFMT(emsg))
#define tightD_errorf(ts,efmt,...)		tightD_error_(ts, MSGFMT(efmt), __VA_ARGS__)


/* print formatted errno error */
#define tightD_errnoerrf(ts,efmt,...) \
	tightD_error_(ts, MSGFMT(efmt ": %s"), __VA_ARGS__, strerror(errno))

/* print errno error */
#define tightD_errnoerr(ts,err) \
	tightD_error_(ts, MSGFMT(err ": %s"), strerror(errno))


/* print warning */
#define tightD_warn(ts,wmsg)			tightD_warn_(ts, MSGFMT(wmsg))
#define tightD_warnf(ts,wfmt,...)		tightD_warn_(ts, MSGFMT(wfmt), __VA_ARGS__)


TIGHT_FUNC t_noret tightD_error_(tight_State *ts, const char *efmt, ...);
TIGHT_FUNC void tightD_warn_(tight_State *ts, const char *wfmt, ...);

TIGHT_FUNC t_noret tightD_memerror(tight_State *ts);
TIGHT_FUNC t_noret tightD_headererr(tight_State *ts, const char *extra);

#endif
