#include <stdio.h>
#include <memory.h>
#include <unistd.h>

#include "talloc.h"
#include "tbuffer.h"
#include "tdebug.h"
#include "tmd5.h"


#define ensurebuf(ts,b,n) \
	tightA_ensurevec(ts, (b)->str, (b)->size, UINT_MAX, (b)->len, n)

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

/* initialize buff reader */
void tightB_initbr(BuffReader *br, tight_State *ts, int fd) {
	br->ts = ts;
	br->current = br->buf;
	br->n = 0;
	br->validbits = 0;
	br->tmpbuf = 0;
	br->fd = fd;
}


/* 
 * Fill buffer so it contains 'n' unread bytes; 
 * in case 'n' is ommited, then fill buffer as
 * much as possible.
 */
int tightB_brfill(BuffReader *br, ulong *n) {
	size_t nbytes = sizeof(br->buf);

	t_assert(sizeof(br->buf) <= UINT_MAX);
	br->n = br->n + (br->n < 0); /* in case buffer is empty */
	t_assert(br->n >= 0);
	if (n) {
		if ((ulong)br->n >= *n) 
			goto ret;
		nbytes = (*n > nbytes ? nbytes : *n);
	}
	nbytes -= br->n;
	memmove(br->buf, br->current, br->n);
	br->current = &br->buf[br->n - (br->n > 0)];
	ssize_t readn = read(br->fd, br->current, nbytes);
	if (t_unlikely(readn < 0))
		tightD_errnoerr(br->ts, "read error while reading input file");
	br->n += readn;
	t_assert(br->n <= (ssize_t)nbytes);
	t_assert(br->n <= (ssize_t)sizeof(br->buf));
	if (n) *n = br->n;
	if (br->n == 0) /* EOF ? */
		return TIGHTEOF;
ret:
	br->n--;
	return *br->current++;
}


/* 
 * Read (up to 8) bits; only used while decoding certain
 * parts of the header; should never be used while decoding
 * actual compressed data, as we need to have a lookahead
 * byte for 'eof' bits.
 */
byte tightB_readnbits(BuffReader *br, int n) {
	int res;

	t_assert(0 <= n && n <= 8);
	if (br->validbits < n) { /* read more bits ?  */
		t_assert(br->validbits + n <= TMPBsize);
		res = tightB_brgetc(br);
		if (t_unlikely(res == TIGHTEOF))
			tightD_headererr(br->ts, ": binary data");
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
	int n = br->validbits;
	if (n == 0) 
		return 0;
	if (out) 
		*out = br->tmpbuf & (((ushrt)~(0)) >> (TMPBsize - n));
	br->validbits = 0;
	return n;
}


/* get adjusted offset */
off_t tightB_offsetreader(BuffReader *br) {
	off_t n = lseek(br->fd, 0, SEEK_CUR);
	if (t_unlikely(n < 0))
		tightD_errnoerr(br->ts, "lseek error in input file");
	return n - br->n;
}


/* generate MD5 digest and store it into 'out' */
void tightB_genMD5(tight_State *ts, ulong size, int fd, byte *out) {
	MD5ctx ctx;
	BuffReader br;

	/* 'fd' is already rewinded to start of data */
	tightB_initbr(&br, ts, fd);
	tight5_init(&ctx);
	do {
		ulong n = size;
		tightB_brfill(&br, &n);
		size -= n;
		tight5_update(&ctx, br.current - 1, n);
		br.n -= --n;
	} while (size > 0);
	t_assert(br.n == 0);
	t_assert(size == 0);
	tight5_final(&ctx, out);
}



/*--------------------------------------------------------------------------
 * BuffWriter
 *-------------------------------------------------------------------------- */

/* initialize buff writer */
void tightB_initbw(BuffWriter *bw, tight_State *ts, int fd) {
	bw->ts = ts;
	bw->len = 0;
	bw->validbits = 0;
	bw->tmpbuf = 0;
	bw->fd = fd;
}


/* flush 'buf' into the current 'wfd' */
void tightB_writefile(BuffWriter *bw) {
	if (t_unlikely(bw->len > 0 && write(bw->fd, bw->buf, bw->len) < 0))
		tightD_errnoerr(bw->ts, "write error while writting output file");
	bw->len = 0;
}


/* write 'byte' into 'buf' */
void tightB_writebyte(BuffWriter *bw, byte byte) {
	if (t_unlikely(bw->len >= sizeof(bw->buf)))
		tightB_writefile(bw); /* flush */
	bw->buf[bw->len++] = byte;
}


/* write 'ushrt' into 'buf' */
static inline void writeshort(BuffWriter *bw, ushrt shrt) {
	tightB_writebyte(bw, shrt & 0xff);
	tightB_writebyte(bw, shrt >> 8);
}


/* write bits to 'tmpbuf' */
void tightB_writenbits(BuffWriter *bw, int code, int len) {
	bw->tmpbuf |= (ushrt)code << bw->validbits;
	if (bw->validbits > TMPBsize - len) { /* would overflow ? */
		writeshort(bw, bw->tmpbuf);
		bw->tmpbuf = (ushrt)code >> (TMPBsize - bw->validbits);
		bw->validbits += len - TMPBsize;
	} else { /* no overflow */
		bw->validbits += len;
	}
}


/* write 'buf' into file, taking into account 'tmpbuf' */
void tightB_writepending(BuffWriter *bw) {
	while (bw->validbits > 0) {
		if (bw->validbits >= 8) {
			tightB_writebyte(bw, bw->tmpbuf);
			bw->tmpbuf >>= 8;
			bw->validbits -= 8;
		} else {
			tightB_writenbits(bw, 0, 8 - bw->validbits);
			tightB_writebyte(bw, bw->tmpbuf);
			bw->validbits = 0;
		}
	}
	t_assert(bw->validbits == 0); /* must be exactly '0' */
}


/* lseek for writer */
off_t tightB_seekwriter(BuffWriter *bw, off_t off, int whence) {
	off_t offset = lseek(bw->fd, off, whence);
	if (t_unlikely(offset < 0))
		tightD_errnoerr(bw->ts, "lseek error in output file");
	return offset;
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
