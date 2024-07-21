#include <stdio.h>
#include <memory.h>

#include "pfcbuffer.h"
#include "pfcalloc.h"


#define ensurebuf(ps,b,n) \
	pfcA_ensurevec(ps, (b)->str, (b)->size, SIZE_MAX, (b)->len, n)

#define addlen2buff(b,l)		((b)->len += (l))



/* for number to string conversion */
#define MAXNUMDIGITS		44


/* initial size for buffers */
#define MINBUFSIZE			64



/* initialize buffer */
void pfcB_init(pfc_State *ps, Buffer *buf) {
	buf->size = 0;
	buf->len = 0;
	buf->str = NULL;
	ensurebuf(ps, buf, MINBUFSIZE);
}


/* conversion for int */
void pfcB_addint(pfc_State *ps, Buffer *buf, int i) {
	char temp[MAXNUMDIGITS];
	int cnt = snprintf(temp, sizeof(temp), "%d", i);
	ensurebuf(ps, buf, cnt);
	memcpy(&buf->str[buf->len], temp, cnt);
	addlen2buff(buf, cnt);
}


/* conversion for size_t  */
void pfcB_addsizet(pfc_State *ps, Buffer *buf, size_t n) {
	char temp[MAXNUMDIGITS];
	int cnt = snprintf(temp, sizeof(temp), "%lu", n);
	ensurebuf(ps, buf, cnt);
	memcpy(&buf->str[buf->len], temp, cnt);
	addlen2buff(buf, cnt);
}


/* conversion for strings */
void pfcB_addstring(pfc_State *ps, Buffer *buf, const char *str, size_t len) {
	ensurebuf(ps, buf, len);
	memcpy(&buf->str[buf->len], str, len);
	addlen2buff(buf, len);
}


/* push char */
void pfcB_push(pfc_State *ps, Buffer *buf, unsigned char c) {
	ensurebuf(ps, buf, 1);
	buf->str[buf->len++] = c;
}


/* free buffer memory */
void pfcB_free(pfc_State *ps, Buffer *buf) {
	if (buf->size > 0) {
		pfc_assert(buf->str != NULL);
		pfcA_freevec(ps, buf->str, buf->size);
	}
	buf->str = NULL; buf->size = buf->len = 0;
}


/* duplicate strings */
char *pfcB_strdup(pfc_State *ps, const char *str) {
	pfc_assert(str != NULL);
	size_t len = strlen(str);
	char *new = pfcA_malloc(ps, len + 1);
	memcpy(new, str, len);
	new[len] = '\0';
	return new;
}
