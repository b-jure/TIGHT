#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "pfcdebug.h"
#include "pfcbuffer.h"
#include "pfcstate.h"


static void errorfmt(pfc_State *ps, const char *fmt, va_list ap) {
	Buffer buf;
	const char *end;

	if (!ps->fmsg) return;
	pfcB_init(ps, &buf);
	pfcB_addstring(ps, &buf, "pfc: ", sizeof("pfc: ") - 1); /* prefix */
	while ((end = strchr(fmt, '%')) != NULL) {
		pfcB_addstring(ps, &buf, fmt, end - fmt);
		switch (end[1]) {
		case 's': {
			const char *s = va_arg(ap, const char *);
			pfcB_addstring(ps, &buf, s, strlen(s));
			break;
		}
		case 'd': {
			int i = va_arg(ap, int);
			pfcB_addint(ps, &buf, i);
			break;
		}
		case 'S': {
			size_t sz = va_arg(ap, size_t);
			pfcB_addsizet(ps, &buf, sz);
			break;
		}
		case 'c': {
			unsigned char c = va_arg(ap, int);
			pfcB_push(ps, &buf, '%');
		}
		case '%': {
			pfcB_push(ps, &buf, '%');
			break;
		}
		default:
			pfcB_free(ps, &buf);
			pfcD_error(ps, "unkown format specifier %%%c", end[1]);
		}
		fmt = end + 2; /* move to end + skip format specifier */
	}
	pfcB_addstring(ps, &buf, fmt, strlen(fmt));
	pfcB_addstring(ps, &buf, ".\n", sizeof(".\n"));
	ps->fmsg(buf.str);
	pfcB_free(ps, &buf);
}


/* generic formatted error */
pfc_noret pfcD_error(pfc_State *ps, const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	errorfmt(ps, fmt, ap);
	va_end(ap);
	if (ps->fpanic)
		ps->fpanic(ps);
	abort();
}


/* oom error */
pfc_noret pfcD_memerror(pfc_State *ps) {
	pfcD_error(ps, "out of memory");
}
