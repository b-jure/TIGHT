#include <stdio.h>
#include <memory.h>
#include <unistd.h>

#include "talloc.h"
#include "tbuffer.h"
#include "tdebug.h"
#include "tmd5.h"


#define ensurebuf(ts,b,n) \
	tightA_ensurevec(ts, (b)->str, (b)->size, SIZE_MAX, (b)->len, n)

#define addlen2buff(b,l)		((b)->len += (l))



/* for number to string conversion */
#define MAXNUMDIGITS		44


/* initial size for buffers */
#define MINBUFSIZE			64



/*--------------------------------------------------------------------------
 * Buffer
 *-------------------------------------------------------------------------- */

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



/*--------------------------------------------------------------------------
 * BuffReader
 *-------------------------------------------------------------------------- */

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
		tightD_errnoerr(ts, "read error while reading input file");
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


/* 
 * Read (up to 8) bits; only used while decoding header; should
 * never be used while decoding actual compressed data.
 */
byte tightB_readnbits(BuffReader *br, int n) {
	int res;

	t_assert(0 <= n && n <= 8);
	if (br->validbits < n) { /* read more bits ?  */
		t_assert(br->validbits + n <= TMPBsize);
		res = tightB_brgetc(br);
		if (t_unlikely(res == TIGHTEOF))
			tightD_headererr(br->ts, ": bindata bits");
		br->tmpbuf |= (ushrt)res << br->validbits;
		br->validbits += 8;
	}
	res = br->tmpbuf & (((ushrt)~(0)) >> (TMPBsize - n));
	br->tmpbuf >>= n;
	br->validbits -= n;
	return res;
}


/* read pending bits */
int tightB_readpending(BuffReader *br, int *out) {
	if (br->validbits == 0) return  0;
	if (out) *out = br->tmpbuf & (((ushrt)~(0)) >> (TMPBsize - br->validbits));
	int n = br->validbits;
	br->validbits = 0;
	return n;
}


/* generate MD5 digest and store it into 'out' */
void tightB_genMD5(BuffReader *br, long size, byte *out) {
	MD5ctx ctx;

	tight5_init(&ctx);
	do {
		/* can't hit EOF while 'size' > 0 */
		t_assert(br->current != NULL);
		/* 'n' will get clamped to 'UINT_MAX' if it overflows */
		long n = size;
		tightB_brfill(br, &n);
		size -= n;
		tight5_update(&ctx, --br->current, n);
	} while (size > 0);
	t_assert(hlen == 0);
	tight5_final(&ctx, out);
}



/*--------------------------------------------------------------------------
 * BuffWriter
 *-------------------------------------------------------------------------- */

/* flush 'buf' into the current 'wfd' */
void tightB_writefile(BuffWriter *bw) {
	tight_State *ts = bw->ts;
	if (t_unlikely(bw->len > 0 && write(ts->wfd, bw->buf, bw->len) < 0))
		tightD_errnoerr(ts, "write error while writting output file");
	bw->len = 0;
}


/* write 'byte' into 'buf' */
void tightB_writebyte(BuffWriter *bw, byte byte) {
	if (t_unlikely(bw->len + 1 > TIGHT_WBUFFSIZE))
		tightB_writefile(bw);
	bw->buf[bw->len++] = byte;
}


/* write 'ushrt' into 'buf' */
void tightB_writeshort(BuffWriter *bw, ushrt shrt) {
	tightB_writebyte(bw, shrt & 0xFF);
	tightB_writebyte(bw, shrt >> 8);
}


/* write bits to 'tmpbuf' */
void tightB_writenbits(BuffWriter *bw, int code, int len) {
	if (bw->validbits > TMPBsize - len) { /* would overflow ? */
		tightB_writeshort(bw, bw->tmpbuf);
		bw->tmpbuf = (ushrt)code >> (TMPBsize - bw->validbits);
		bw->validbits += len - TMPBsize;
	} else { /* no overflow */
		bw->tmpbuf |= (ushrt)code << bw->validbits;
		bw->validbits += len;
	}
}


/* write 'buf' into file, taking into account 'tmpbuf' */
void tightB_writepending(BuffWriter *bw) {
	while (bw->validbits > 0) {
		if (bw->validbits >= 8) {
			tightB_writebyte(bw, bw->tmpbuf);
			bw->validbits -= 8;
		} else {
			tightB_writenbits(bw, 0, 8 - bw->validbits);
			tightB_writebyte(bw, bw->tmpbuf);
			bw->validbits = 0;
		}
	}
	t_assert(bw->validbits == 0); /* must be exactly '0' */
}



/* 
 * - MISC -
 */

/* dup strings */
char *tightB_strdup(tight_State *ts, const char *str) {
	t_assert(str != NULL);
	size_t len = strlen(str);
	char *new = tightA_malloc(ts, len + 1);
	memcpy(new, str, len);
	new[len] = '\0';
	return new;
}
