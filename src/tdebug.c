#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "tdebug.h"
#include "tbuffer.h"


static void errorfmt(tight_State *ts, const char *fmt, va_list ap) {
	Buffer buf;
	const char *end;

	t_assert(ts->fmsg);
	tightB_init(ts, &buf);
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
			tightB_push(ts, &buf, c);
			break;
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
	tightB_addstring(ts, &buf, fmt, strlen(fmt) + 1); /* include null term */
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


/* TIGHT header decode error */
t_noret tightD_headererr(tight_State *ts, const char *extra) {
	extra = (extra ? extra : "");
	tightD_error_(ts, MSGFMT("malformed header (input file)%s"), extra);
}


/* functions for tracing encoder/decoder execution */
#if defined(TIGHT_TRACE)

/* debug code */
void tightD_printbits(int code, int nbits) {
	char buf[64];

	buf[nbits] = '\0';
	for (int i = 0; nbits-- > 0; i++)
		buf[i] = '0' + ((code & (1 << nbits)) > 0);
	t_tracef("%s", buf);
}


/* recursive auxiliary function to 'tightD_printtree' */
static void printtree(const TreeData *t) {
	const TreeData *stack[TIGHTCODES];
	int len = 0;

	t_trace("---Tree---\n");
	memset(stack, 0, sizeof(stack));
	stack[len++] = t;
	stack[len++] = NULL;
	while (len > 0) {
		const TreeData *curr = stack[0];
		memmove(stack, stack + 1, --len * sizeof(stack[0]));
		if (curr == NULL) { /* newline? */
			t_trace("\n");
			if (len > 0)
				stack[len++] = NULL;
		} else if (curr->left) {
			t_assert(curr->right);
			stack[len++] = curr->left;
			stack[len++] = curr->right;
			t_tracef("P(%d)", curr->c);
		} else {
			t_assert(curr->left == NULL);
			t_tracef("L(%d)", curr->c);
		}
	}
	t_assert(len == 0);
}


/* debug encoding tree */
void tightD_printtree(const TreeData *root) {
	printtree(root);
}


/* debug checksum */
void tightD_printchecksum(byte *checksum, size_t size) {
	for (uint i = 0; i < size; i++) {
		t_tracef("%02X", checksum[i]);
		if ((i + 1) % 2 == 0) t_trace(" ");
	}
	t_trace("\n");
}

#endif /* TIGHT_TRACE */
