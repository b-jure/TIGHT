#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#include "tdebug.h"
#include "tbuffer.h"
#include "talloc.h"
#include "tstate.h"


static void seterrorfmt(tight_State *ts, const char *fmt, va_list ap) {
	Buffer buf;
	const char *end;

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
		case 'z': {
			size_t size = va_arg(ap, size_t);
			tightB_addsize(ts, &buf, size);
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
		default: /* UNREACHED */
			t_assert(0 && "unreachable");
			abort();
		}
		fmt = end + 2; /* move to end + skip format specifier */
	}
	tightB_addstring(ts, &buf, fmt, strlen(fmt) + 1); /* include null term */
	tightB_shrink(ts, &buf);
	if (ts->error != NULL) /* have previous error ? */
		tightA_free(ts, ts->error, strlen(ts->error) + 1);
	ts->error = tightB_string(&buf);
	tightS_poptemp(ts);
}


/* generic formatted error */
static void errormsg(tight_State *ts, const char *fmt, ...) {
	va_start(ts->errjmp->ap, fmt);
	ts->errjmp->haveap = 1;
	seterrorfmt(ts, fmt, ts->errjmp->ap); /* 'va_end' in here */
	va_end(ts->errjmp->ap);
	ts->errjmp->haveap = 0;
}


/* compression error */
t_noret tightD_compresserror(tight_State *ts, const char *desc) {
	t_assert(desc != NULL);
	errormsg(ts, "compression error (%s)", desc);
	tightS_throw(ts, TIGHT_ERRCOMP);
}


/* decompression error */
t_noret tightD_decompresserror(tight_State *ts, const char *desc) {
	t_assert(desc != NULL);
	errormsg(ts, "decompression error (%s)", desc);
	tightS_throw(ts, TIGHT_ERRDECOMP);
}


/* malformed TIGHT header error */
t_noret tightD_headererror(tight_State *ts, const char *extra) {
	extra = (extra ? extra : "");
	errormsg(ts, "malformed header%s", extra);
	tightS_throw(ts, TIGHT_ERRHEAD);
}


/* libc error */
t_noret tightD_errnoerror(tight_State *ts, const char *fn) {
	t_assert(fn != NULL);
	t_assert(errno != 0);
	errormsg(ts, "%s: %s", fn, strerror(errno));
	tightS_throw(ts, TIGHT_ERRNO);
}


/* size limit error */
t_noret tightD_limiterror(tight_State *ts, const char *what, size_t limit) {
	t_assert(what != NULL);
	errormsg(ts, "%s limit error, limit is %z", what, limit);
	tightS_throw(ts, TIGHT_ERRLIMIT);
}


/* mismatched 'major' version error */
t_noret tightD_versionerror(tight_State *ts, byte major) {
	errormsg(ts, "mismatched major version number, expect " TIGHT_VERSION_MAJOR
				 " got %c", major);
	tightS_throw(ts, TIGHT_ERRVER);
}


TIGHT_API const char *tight_geterror(tight_State *ts) {
	if (t_unlikely(ts->status == TIGHT_ERRMEM))
		return TIGHT_MEMERROR;
	return ts->error;
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
