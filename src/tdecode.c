#include <string.h>
#include <unistd.h>
#if defined(TIGHT_TRACE)
#include <ctype.h>
#endif

#include "talloc.h"
#include "tbuffer.h"
#include "tdebug.h"
#include "tight.h"
#include "tinternal.h"
#include "tstate.h"


/* extract 'eof' bits from 'mask' */
#define geofbits(mask)		((mask) & 0x07)



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
		br->ts->hufftree = decodetree(br);
		t_trace("\n");
		t_assert(br->validbits > 0); /* should have leftover */
		tightB_readpending(br, NULL); /* rest is just padding */
	}
}


/* decode header 'checksum' */
static void decodechecksum(BuffReader *br, byte *checksum, size_t size) {
	int c;

	t_trace("---Decoding [checksum]---\n");
	for (size_t i = 0; (c = tightB_brgetc(br)) != TIGHTEOF; i++) {
		checksum[i] = c;
		if (i == size - 1) {
#if defined(TIGHT_TRACE)
			for (uint i = 0; i < size; i++) {
				t_tracef("%02X", checksum[i]);
				if ((i + 1) % 2 == 0) t_trace(" ");
			}
			t_trace("\n");
#endif
			return; /* ok */
		}
	}
	tightD_headererr(br->ts, ": missing checksum bytes");
}


/* verify decoded 'checksum' */ 
static void verifychecksum(BuffReader *br, byte *checksum, size_t sumsize,
						   long bindatasize) 
{
	byte *md5digest = tightA_malloc(br->ts, sumsize * sizeof(byte));
	t_assert(sumsize >= 16); /* NOTE(jure): currently only MD5 is used */
	tightB_genMD5(br->ts, bindatasize, br->fd, md5digest);
	int res = memcmp(md5digest, checksum, sumsize);
	tightA_free(br->ts, md5digest, sumsize * sizeof(byte));
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
	off_t bindatastart = tightB_getoffset(br);
	decodebindata(br, mode);
	off_t bindatasize = tightB_getoffset(br);
	t_assert(bindatasize > bindatastart);
	bindatasize -= bindatastart;
	if (t_unlikely(lseek(br->fd, bindatastart, SEEK_SET) < 0))
		tightD_errnoerr(br->ts, "seek error in input file");

	/* decode 'checksum' and verify it */
	decodechecksum(br, checksum, sizeof(checksum));
	verifychecksum(br, checksum, sizeof(checksum), bindatasize);
	return mode;
}


/* get symbol from huffman 'code' */
static int getsymbol(TreeData **t, int *code, int *nbits, int limit) 
{
	TreeData *curr = *t;

	if (curr->left == NULL) { /* leaf ? */
		t_assert(curr->right = NULL);
		return curr->c;
	} else if (*nbits <= limit) { /* hit limit ? */
		*t = curr; /* set checkpoint */
		return -1; /* return indicator */
	} else { /* parent */
		byte direction = *code & 0x01;
		*code >>= 1;
		t_assert(*nbits > 0);
		*nbits -= 1;
		int sym;
		if (direction) /* 1 (right) */
			sym = getsymbol(&curr->right, code, nbits, limit);
		else /* 0 (left) */
			sym = getsymbol(&curr->left, code, nbits, limit);
		return sym;
	}
}


/* decompress/decode file contents */
static inline void decodehuffman(BuffWriter *bw, BuffReader *br) {
	tight_State *ts = bw->ts;
	int ahead, sym, code, left;

	t_assert(br->validbits == 0); /* data must be aligned */
	t_assert(ts->hufftree != NULL); /* must have decoded huffman tree */

	t_trace("---Decoding [huffman]---\n");
	int c1 = tightB_brgetc(br);
	t_assert(c1 != TIGHTEOF); /* can't be empty */
	int c2 = tightB_brgetc(br);
	if (t_unlikely(c2 == TIGHTEOF)) { /* very small file */
		left = geofbits(c1);
		code = (c1 >> (8 - left));
		goto eof;
	}
	TreeData *at = ts->hufftree; /* checkpoint */
	code = (c2 << 8) | c1;
	left = 16;
	while ((ahead = tightB_brgetc(br)) != TIGHTEOF) {
		for (;;) {
			sym = getsymbol(&at, &code, &left, 8);
			if (sym == -1) break; /* check next 8 bits */
			at = ts->hufftree;
			tightB_writebyte(bw, sym);
		}
		t_assert(left == 8 && at != ts->hufftree);
		code |= (ahead << left);
		left += 8;
	}
	t_assert(left == MAXCODE);
	left = geofbits(code) + EOFBIAS; /* have bias */
	code >>= MAXCODE - left;
eof:
	while (left > 0) {
		sym = getsymbol(&ts->hufftree, &code, &left, 0);
		if (t_unlikely(sym == -1))
			tightD_error(ts, "file is improperly encoded");
		t_assert(sym >= 0);
		tightB_writebyte(bw, sym);
	}
	t_assert(left == 0);
	t_trace("Decoding huffman done!");
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
}
