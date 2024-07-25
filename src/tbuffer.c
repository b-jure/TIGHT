#include <stdio.h>
#include <memory.h>
#include <unistd.h>

#include "tbuffer.h"
#include "talloc.h"
#include "tdebug.h"


#define ensurebuf(ts,b,n) \
	tightA_ensurevec(ts, (b)->str, (b)->size, SIZE_MAX, (b)->len, n)

#define addlen2buff(b,l)		((b)->len += (l))



/* for number to string conversion */
#define MAXNUMDIGITS		44


/* initial size for buffers */
#define MINBUFSIZE			64



/* initialize buffer */
void tightB_init(tight_State *ts, Buffer *buf) {
	buf->size = 0;
	buf->len = 0;
	buf->str = NULL;
	ensurebuf(ts, buf, MINBUFSIZE);
}


/* conversion for int */
void tightB_addint(tight_State *ts, Buffer *buf, int i) {
	char temp[MAXNUMDIGITS];
	int cnt = snprintf(temp, sizeof(temp), "%d", i);
	ensurebuf(ts, buf, cnt);
	memcpy(&buf->str[buf->len], temp, cnt);
	addlen2buff(buf, cnt);
}


/* conversion for size_t  */
void tightB_adduint(tight_State *ts, Buffer *buf, uint u) {
	char temp[MAXNUMDIGITS];
	int cnt = snprintf(temp, sizeof(temp), "%u", u);
	ensurebuf(ts, buf, cnt);
	memcpy(&buf->str[buf->len], temp, cnt);
	addlen2buff(buf, cnt);
}


/* conversion for strings */
void tightB_addstring(tight_State *ts, Buffer *buf, const char *str, size_t len) {
	ensurebuf(ts, buf, len);
	memcpy(&buf->str[buf->len], str, len);
	addlen2buff(buf, len);
}


/* push char */
void tightB_push(tight_State *ts, Buffer *buf, unsigned char c) {
	ensurebuf(ts, buf, 1);
	buf->str[buf->len++] = c;
}


/* free buffer memory */
void tightB_free(tight_State *ts, Buffer *buf) {
	if (buf->size > 0) {
		t_assert(buf->str != NULL);
		tightA_freevec(ts, buf->str, buf->size);
	}
	buf->str = NULL; buf->size = buf->len = 0;
}


/* duplicate strings */
char *tightB_strdup(tight_State *ts, const char *str) {
	t_assert(str != NULL);
	size_t len = strlen(str);
	char *new = tightA_malloc(ts, len + 1);
	memcpy(new, str, len);
	new[len] = '\0';
	return new;
}


/* 
 * Fill buffer up to 'n' if 'n' is specified otherwise use
 * 'buf' size; 'n' is clamped in case 'n' > sizeof(buf); 
 * additionally if 'n' is provided it will be updated with
 * the number of bytes read. 
 */
int tightB_brfill(BuffReader *br, long *n) {
	tight_State *ts = br->ts;
	long size = (n ? *n : sizeof(br->buf));

	t_assert(sizeof(br->buf) <= UINT_MAX);
	t_assert(br->n == 0 && br->current != NULL);
	size = (size <= sizeof(br->buf) ? size : sizeof(br->buf));
	br->n = read(ts->rfd, br->buf, size);
	if (t_unlikely(br->n < 0))
		tightD_errnoerr(ts, "read error while reading '%s'", br->filepath);
	if (n) *n = br->n;
	if (br->n == 0) { /* EOF ? */
		br->current = NULL;
		return TIGHTEOF;
	} else {
		br->current = br->buf;
		return *br->current++;
	}
}


/* get buffered character */
int tightB_brgetc(BuffReader *br) {
	if (br->n > 0)
		return *br->current++;
	else if (br->current == NULL)
		return TIGHTEOF;
	return tightB_brfill(br, NULL);
}


