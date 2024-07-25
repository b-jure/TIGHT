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


/* print errno error */
#define tightD_errnoerr(ts,efmt,...) \
	tightD_error_(ts, MSGFMT(efmt ": %s"), __VA_ARGS__, strerror(errno))


/* print warning */
#define tightD_warn(ts,wmsg)			tightD_warn_(ts, MSGFMT(wmsg))
#define tightD_warnf(ts,wfmt,...)		tightD_warn_(ts, MSGFMT(wfmt), __VA_ARGS__)



t_noret tightD_error_(tight_State *ts, const char *efmt, ...);
t_noret tightD_memerror(tight_State *ts);
void tightD_warn_(tight_State *ts, const char *wfmt, ...);

#endif
