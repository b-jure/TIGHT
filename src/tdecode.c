#include <string.h>
#include <unistd.h>
#if defined(TIGHT_TRACE)
#include <ctype.h>
#endif

#include "tbuffer.h"
#include "tdebug.h"
#include "tight.h"
#include "tinternal.h"
#include "tstate.h"


/* extract 'eof' bits from 'bits' */
#define geofbits(bits)		((bits) & 0x07)



/* decode header 'magic' */
static void decodemagic(BuffReader *br) {
	int c;

	t_trace("---Decoding [magic]---\n");
	for (uint i = 0; (c = tightB_brgetc(br)) != TIGHTEOF; i++) {
		if (t_unlikely(c != MAGIC[i]))
			tightD_headererr(br->ts, ": invalid magic bytes");
		if (t_unlikely(i == sizeof(MAGIC) - 1)) {
			t_tracef("%.*s\n", (int)sizeof(MAGIC), MAGIC);
			return; /* ok */
		}
	}
	tightD_headererr(br->ts, ": incomplete magic");
}


/* auxiliary to 'decodebindata', decodes encode huffman tree */
static TreeData *decodetree(BuffReader *br) {
	int bits = tightB_readnbits(br, 1);

	if (bits == 0) { /* parent ? */ 
		t_trace("[");
		TreeData *t1 = decodetree(br);
		t_trace(", ");
		TreeData *t2 = decodetree(br);
		t_trace("]");
		return tightT_newparent(br->ts, t1, t2, 0);
	} else { /* leaf */
		bits = tightB_readnbits(br, 8); /* get symbol */
		t_tracef((isgraph(bits) ? "%c" : "%d"), bits);
		return tightT_newleaf(br->ts, 0, bits);
	}
}


/* decode header 'bindata' */
static inline void decodebindata(BuffReader *br, int mode) {
	t_assert(mode & MODEALL);
	if (mode & MODEHUFF) {
		t_trace("---Decoding [tree]----\n");
		br->ts->hufftree = decodetree(br); /* anchor to state */
		t_trace("\n");
		tightD_printtree(br->ts->hufftree);
		t_assert(br->ts->hufftree != NULL);
		t_assert(br->validbits > 0); /* should have leftover */
		tightB_readpending(br, NULL); /* rest is just padding */
	}
}


/* decode header 'checksum' */
static void decodechecksum(BuffReader *br, byte *checksum, size_t size) {
	int c;

	memset(checksum, 0, size);
	t_trace("---Decoding [checksum]---\n");
	for (size_t i = 0; (c = tightB_brgetc(br)) != TIGHTEOF; i++) {
		checksum[i] = c;
		if (i == size - 1) {
			tightD_printchecksum(checksum, size);
			return; /* ok */
		}
	}
	tightD_headererr(br->ts, ": missing checksum bytes");
}


/* verify decoded 'checksum' */ 
static void verifychecksum(BuffReader *br, byte *checksum, off_t bindatastart,
						   ulong bindatasize) 
{
	byte out[16];
	off_t offset;

	if (t_unlikely((offset = lseek(br->fd, bindatastart, SEEK_SET)) < 0))
		tightD_errnoerr(br->ts, "lseek error in input file");
	t_assert(br->validbits == 0);
	tightB_initbr(br, br->ts, br->fd); /* reset reader */
	tightB_genMD5(br->ts, bindatasize, br->fd, out);
	t_assert(tightB_offsetreader(br) == bindatastart + bindatasize);
	int res = memcmp(out, checksum, sizeof(out));
	if (t_unlikely(res != 0))
		tightD_headererr(br->ts, ": checksum doesn't match");
}


/* decode 'TIGHT' header */
static int decodeheader(BuffReader *br) {
	byte checksum[16];

	/* decode 'magic' */
	decodemagic(br);

	int mode = MODEHUFF;
	/* TODO(jure): int mode = decodemode(br) */
	t_assert(mode & MODEALL);

	/* start of 'bindata' for 'verifychecksum' */
	off_t bindatastart = tightB_offsetreader(br);
	decodebindata(br, mode);
	off_t bindatasize = tightB_offsetreader(br);
	bindatasize -= bindatastart;

	/* decode 'checksum' and verify it */
	decodechecksum(br, checksum, sizeof(checksum));
	off_t checksumend = tightB_offsetreader(br);
	verifychecksum(br, checksum, bindatastart, bindatasize);
	if (t_unlikely(lseek(br->fd, checksumend, SEEK_SET) < 0))
		tightD_errnoerr(br->ts, "lseek error in input file");
	return mode;
}


/* get symbol from huffman 'code' */
static int getsymbol(TreeData **cp, TreeData *curr, int *code, int *nbits, int limit) 
{
	if (curr->left == NULL) { /* leaf ? */
		t_assert(curr->right == NULL);
		t_tracef((isgraph(curr->c) ? "%c" : "%hu"), curr->c);
		return curr->c;
	} else if (*nbits <= limit) { /* hit limit ? */
		*cp = curr; /* set checkpoint */
		return -1; /* return indicator */
	} else { /* parent */
		byte direction = *code & 0x01;
		*code >>= 1;
		t_assert(*nbits > 0);
		*nbits -= 1;
		int sym;
		if (direction) /* 1 (right) */
			sym = getsymbol(cp, curr->right, code, nbits, limit);
		else /* 0 (left) */
			sym = getsymbol(cp, curr->left, code, nbits, limit);
		return sym;
	}
}


/* decompress/decode file contents */
static inline void decodehuffman(BuffWriter *bw, BuffReader *br) {
	tight_State *ts = bw->ts;
	TreeData *at = ts->hufftree; /* checkpoint */
	int ahead, sym, code, left;

	t_assert(br->validbits == 0); /* data must be aligned */
	t_assert(ts->hufftree != NULL); /* must have decoded huffman tree */
	t_trace("---Decoding [huffman]---\n");

	int c1 = tightB_brgetc(br);
	for (int i = 0; i <= br->n; i++)
		t_tracelong("(", tightD_printbits(br->buf[i], 8), ")");
	t_trace("\n");
	t_assert(c1 != TIGHTEOF); /* can't be empty */

	int c2 = tightB_brgetc(br);
	if (t_unlikely(c2 == TIGHTEOF)) { /* 'eof' in 'c1' ? */
		left = geofbits(c1); /* eof without bias */
		t_assert(left <= 8 - EOFBITS);
		code = c1 >> (8 - left);
		goto eof;
	}

	code = (c2 << 8) | c1;
	left = MAXCODE;
	t_trace("[");
	while ((ahead = tightB_brgetc(br)) != TIGHTEOF) {
		t_assert(left == MAXCODE);
		for (;;) {
			sym = getsymbol(&at, at, &code, &left, MAXCODE >> 1);
			if (sym == -1) break; /* check next 8 bits */
			t_trace(",");
			at = ts->hufftree;
			tightB_writebyte(bw, sym);
		}
		t_assert(left == 8);
		code |= ahead << left;
		left += 8;
	}
	t_assert(left == MAXCODE);
	/* eof with bias and full prev byte */
	left = geofbits(code >> 8) + EOFBIAS + 2;
	t_assert(left <= MAXCODE - EOFBITS);
	code = ((code >> (MAXCODE - left)) & 0xff00) | (code & 0xff);

eof:
	while (left > 0) {
		t_trace((sym != -1 ? "," : ""));
		sym = getsymbol(&at, at, &code, &left, 0);
		if (t_unlikely(sym == -1))
			tightD_error(ts, "file is improperly encoded");
		at = ts->hufftree;
		t_assert(sym >= 0);
		tightB_writebyte(bw, sym);
	}
	t_trace("]\n");
	t_assert(left == 0);
	tightB_writefile(bw);
}



/* encode input file writing the changes to output file */
TIGHT_API void tight_decode(tight_State *ts) {
	BuffReader br; BuffWriter bw;

	t_assert(ts->rfd >= 0 && ts->wfd >= 0 && ts->rfd != ts->wfd);

	/* init reader and writer */
	tightB_initbr(&br, ts, ts->rfd);
	tightB_initbw(&bw, ts, ts->wfd);

	int mode = decodeheader(&br);
	if (mode & MODEHUFF)
		decodehuffman(&bw, &br);
	if (mode & MODELZW) {/* TODO(jure): implement LZW */}
	t_trace("\n***Decoding complete!***\n\n");
}
