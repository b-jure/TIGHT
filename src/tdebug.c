#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "pfcdebug.h"
#include "pfcbuffer.h"
#include "pfcstate.h"


static void errorfmt(tight_State *ts, const char *fmt, va_list ap) {
	Buffer buf;
	const char *end;

	t_assert(ts->fmsg);
	tightB_init(ts, &buf);
	tightB_addstring(ts, &buf, "tight: ", sizeof("tight: ") - 1); /* prefix */
	while ((end = strchr(fmt, '%')) != NULL) {
		tightB_addstring(ts, &buf, fmt, end - fmt);
		switch (end[1]) {
		case 's': {
			const char *s = va_arg(ap, const char *);
			tightB_addstring(ts, &buf, s, strlen(s));
			break;
		}
		case 'd': {
			int i = va_arg(ap, int);
			tightB_addint(ts, &buf, i);
			break;
		}
		case 'u': {
			uint sz = va_arg(ap, uint);
			tightB_adduint(ts, &buf, sz);
			break;
		}
		case 'c': {
			unsigned char c = va_arg(ap, int);
			tightB_push(ts, &buf, '%');
		}
		case '%': {
			tightB_push(ts, &buf, '%');
			break;
		}
		default:
			tightB_free(ts, &buf);
			tightD_errorf(ts, "unkown format specifier %%%c", end[1]);
		}
		fmt = end + 2; /* move to end + skip format specifier */
	}
	tightB_addstring(ts, &buf, fmt, strlen(fmt));
	tightB_addstring(ts, &buf, ".\n", sizeof(".\n"));
	ts->fmsg(buf.str);
	tightB_free(ts, &buf);
}


/* generic formatted error */
t_noret tightD_error_(tight_State *ts, const char *fmt, ...) {
	va_list ap;

	if (ts->fmsg) {
		va_start(ap, fmt);
		errorfmt(ts, fmt, ap);
		va_end(ap);
	}
	if (ts->fpanic)
		ts->fpanic(ts);
	abort();
}


/* oom error */
t_noret tightD_memerror(tight_State *ts) {
	tightD_error_(ts, MSGFMT("out of memory"));
}


/* print warning */
void tightD_warn_(tight_State *ts, const char *wfmt, ...) {
	t_assert(ts->fmsg != NULL);
	va_list ap;
	va_start(ap, wfmt);
	errorfmt(ts, wfmt, ap);
	va_end(ap);
}
