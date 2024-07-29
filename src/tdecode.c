#include <string.h>
#include <unistd.h>

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

	for (uint i = 0; (c = tightB_brgetc(br)) != TIGHTEOF; i++) {
		if (t_unlikely(c != MAGIC[i]))
			tightD_headererr(br->ts, ": invalid magic bytes");
		if (t_unlikely(i == sizeof(MAGIC) - 1)) return; /* ok */
	}
	tightD_headererr(br->ts, ": missing magic bytes");
}


/* auxiliary to 'decodebindata', decodes encodec huffman tree */
static TreeData *decodetree(BuffReader *br) {
	int bits = tightB_readnbits(br, 1);

	if (bits == 0) { /* parent ? */ 
		TreeData *t1 = decodetree(br);
		TreeData *t2 = decodetree(br);
		return tightT_newparent(br->ts, t1, t2, 0);
	} else { /* leaf */
		bits = tightB_readnbits(br, 8); /* get symbol */
		return tightT_newleaf(br->ts, 0, bits);
	}
}


/* decode header 'bindata' */
static inline void decodebindata(BuffReader *br, int mode) {
	t_assert(mode & MODEALL);
	if (mode & MODELZW) {/* TODO(jure): implement LZW */}
	if (mode & MODEHUFF) {
		br->ts->hufftree = decodetree(br);
		printf("decoded tree\n");
		fflush(stdout);
		t_assert(br->validbits > 0); /* should have leftover */
		tightB_readpending(br, NULL); /* rest is just padding */
	}
}


/* decode header 'checksum' */
static void decodechecksum(BuffReader *br, byte *checksum, size_t size) {
	int c;

	for (size_t i = 0; (c = tightB_brgetc(br)) != TIGHTEOF; i++) {
		checksum[i] = c;
		if (i == size - 1) return; /* ok */
	}
	tightD_headererr(br->ts, ": missing checksum bytes");
}


/* verify decoded 'checksum' */ 
static void verifychecksum(BuffReader *br, byte *checksum, size_t sumsize,
						   long bindatasize) 
{
	byte *md5digest = tightA_malloc(br->ts, sumsize * sizeof(byte));
	t_assert(sumsize >= 16); /* NOTE(jure): current version must be MD5 */
	tightB_genMD5(br, bindatasize, md5digest);
	int res = memcmp(md5digest, checksum, sumsize);
	tightA_free(br->ts, md5digest, sumsize * sizeof(byte));
	if (t_unlikely(res != 0))
		tightD_headererr(br->ts, ": checksum doesn't match");
}


/* decode 'TIGHT' header */
static int decodeheader(BuffWriter *bw, BuffReader *br) {
	tight_State *ts = bw->ts;
	byte checksum[16];

	/* decode 'magic' */
	printf("decoding magic\n");
	fflush(stdout);
	decodemagic(br);
	printf("decoded magic\n");
	fflush(stdout);

	int mode = MODEHUFF;
	/* TODO(jure): int mode = decodemode(br) */
	t_assert(mode & MODEALL);

	/* start of 'bindata' for 'verifychecksum' */
	long bindatastart = lseek(br->fd, 0, SEEK_CUR);
	if (t_unlikely(bindatastart < 0))
		tightD_errnoerr(ts, "lseek error in input file");

	/* decode 'bindata' */
	printf("decoding bindata\n");
	fflush(stdout);
	decodebindata(br, mode);
	printf("decoded bindata\n");
	fflush(stdout);

	long bindatasize = lseek(br->fd, 0, SEEK_CUR);
	if (t_unlikely(bindatasize < 0))
		tightD_errnoerr(ts, "lseek error in input file");
	bindatasize -= bindatastart;

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
}



/* encode input file writing the changes to output file */
TIGHT_API void tight_decode(tight_State *ts) {
	BuffReader br; BuffWriter bw;

	t_assert(ts->rfd >= 0 && ts->wfd >= 0 && ts->rfd != ts->wfd);

	/* init reader and writer */
	tightB_initbr(&br, ts, ts->rfd);
	tightB_initbw(&bw, ts, ts->wfd);

	int mode = decodeheader(&bw, &br);
	printf("decoded header\n");
	fflush(stdout);
	if (mode & MODELZW) {/* TODO(jure): implement LZW */}
	if (mode & MODEHUFF)
		decodehuffman(&bw, &br);
}
