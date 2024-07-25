#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "tdebug.h"
#include "tinternal.h"
#include "tstate.h"
#include "tbuffer.h"


/* maximum bits in huffman code */
#define MAXCODE			16

/* size of 'tempbuf' in 'BuffWriter' */
#define TMPBsize		MAXCODE



/* buffered writer */
typedef struct BuffWriter {
	tight_State *ts; /* state */
	uint len; /* number of elements in 'buf' */
	byte buf[TIGHT_WBUFFSIZE]; /* write buffer */
	int validbits; /* valid bits in 'tmpbuf' */
	ushrt tmpbuf; /* temporary bits buffer */
} BuffWriter;


/* flush 'buf' into the current 'wfd' */
static inline void writefile(BuffWriter *bw) {
	tight_State *ts = bw->ts;
	if (t_unlikely(bw->len > 0 && write(ts->wfd, bw->buf, bw->len) < 0))
		tightD_errnoerr(ts, "write error while writting output file");
	bw->len = 0;
}


/* write 'byte' into 'buf' */
static inline void writebyte(BuffWriter *bw, byte byte) {
	if (t_unlikely(bw->len + 1 > TIGHT_WBUFFSIZE))
		writefile(bw);
	bw->buf[bw->len++] = byte;
}


/* write 'ushrt' into 'buf' */
static inline void writeshort(BuffWriter *bw, ushrt shrt) {
	writebyte(bw, shrt & 0xFF);
	writebyte(bw, shrt >> 8);
}


/* write bits to 'tmpbuf' */
static inline void writenbits(BuffWriter *bw, int code, int len) {
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
static inline void writependingbits(BuffWriter *bw) {
	if (bw->validbits > 0) {
		if (bw->validbits != TMPBsize)
			writenbits(bw, 0, TMPBsize - bw->validbits);
		writeshort(bw, bw->tmpbuf);
	}
}


/* write 'magic' */
static inline void writemagic(BuffWriter *bw) {
	/* unroll */
	t_assert(bw->len == 0 && bw->validbits == 0);
	writebyte(bw, MAGIC[0]);
	writebyte(bw, MAGIC[1]);
	writebyte(bw, MAGIC[2]);
	writebyte(bw, MAGIC[3]);
	writebyte(bw, MAGIC[4]);
	writebyte(bw, MAGIC[5]);
	writebyte(bw, MAGIC[6]);
	writebyte(bw, MAGIC[7]);
}


/* auxiliary to 'writeheader', writes compressed Huffman tree structure */
static inline void writehufftree(BuffWriter *bw, TreeData *ht) {
	t_assert(bw->len > 0); /* must of written MAGIC already */
	if (ht->left) { /* parent tree ? */
		writenbits(bw, 0, 1);
		writehufftree(bw, ht->left);
		writehufftree(bw, ht->right);
	} else { /* leaf */
		t_assert(ht->right == NULL && 0 <= ht->c && ht->c < 256);
		writenbits(bw, 1, 1);
		writenbits(bw, ht->c, 8); /* write char */
	}
}


/* write MD5 digest */
static inline void writechecksum(BuffWriter *bw, byte checksum[16]) {
	/* unroll */
	t_assert(bw->len == 0 && bw->validbits == 0);
	writebyte(bw, checksum[0]);
	writebyte(bw, checksum[0]);
	writebyte(bw, checksum[1]);
	writebyte(bw, checksum[2]);
	writebyte(bw, checksum[3]);
	writebyte(bw, checksum[4]);
	writebyte(bw, checksum[5]);
	writebyte(bw, checksum[6]);
	writebyte(bw, checksum[7]);
}


/* write header */
static void writeheader(BuffWriter *bw, BuffReader *br) {
	tight_State *ts = bw->ts;
	MD5ctx ctx;
	byte md5digest[16];
	long offset;

	/* tree must already be built */
	t_assert(bw->ts->hufftree != NULL);

	/* write 'magic' and 'bindata' */
	writemagic(bw);
	writehufftree(bw, bw->ts->hufftree);
	writependingbits(bw);
	writefile(bw);

	/* get size of 'bindata' */
	long bindatasz = lseek(ts->wfd, 0, SEEK_CUR) - TIGHTbindataoffset;
	if (t_unlikely(bindatasz < 0))
		tightD_errnoerr(bw->ts, "lseek error in output file");
	t_assert(hlen > 0);
	/* seek to start of the 'bindata' */
	if (t_unlikely(lseek(ts->wfd, TIGHTbindataoffset, SEEK_SET) < 0))
		tightD_errnoerr(bw->ts, "lseek error in output file");

	/* generate MD5 checksum (of 'bindata') */
	tight5_init(&ctx);
	do {
		/* can't hit EOF while 'bindatasz' > 0 */
		t_assert(br->current != NULL);
		/* 'n' will get clamped to 'UINT_MAX' if it overflows */
		long n = bindatasz;
		tightB_brfill(br, &n);
		bindatasz -= n;
		tight5_update(&ctx, --br->current, n);
	} while (bindatasz > 0);
	t_assert(hlen == 0);
	tight5_final(&ctx, md5digest);
	writechecksum(bw, md5digest); /* write MD5 digest */
	writefile(bw); /* write all */
}


/* 
 * Write 'eof' for huffman codes; last 3 bits
 * of the last byte encode how many bits to read
 * from the byte before it + 'bias'.
 * For example, in case last 3 bits are '001' (1),
 * then 7 bits will be read from the previous byte.
 */
static void writeeof(BuffWriter *bw) {
	const int bias = 6;
	int extra = 0;

	if (bw->validbits >= 8) {
		writebyte(bw, bw->tmpbuf);
		bw->tmpbuf >>= 8;
		bw->validbits -= 8;
		extra = 2;
	}
	if (bw->validbits <= 5) { /* 'eof' can fit ? */
		bw->tmpbuf <<= 8 - bw->validbits; /* shift valid bits */
		bw->tmpbuf |= bw->validbits + extra; /* add 'eof' bits */
		writebyte(bw, bw->tmpbuf);
	} else { /* 'eof' must be in separate byte */
		t_assert(bias <= bw->validbits);
		writebyte(bw, bw->tmpbuf);
		writebyte(bw, bw->validbits - bias);
	}
	bw->validbits = 0;
	writefile(bw); /* write all */
}


/* 
 * Encode input file, write the result into output file; 
 * user of the API should ensure that the input file is a
 * regular file and not a directory, symbolic link or any
 * other special file, otherwise some system calls might
 * have undefined behaviour.
 */
TIGHT_API void tight_encode(tight_State *ts) {
	BuffReader br;
	BuffWriter bw;
	Buffer outfile;
	int c;

	/* must have valid files */
	t_assert(ts->rfd >= 0 && ts->wfd >= 0 && ts->rfs != ts->wfd);

	/* init reader and writer */
	memset(&br, 0, sizeof(br));
	memset(&bw, 0, sizeof(bw));
	br.ts = bw.ts = ts;

	/* write header (magic + tree + checksum) */
	writeheader(&bw, &br);

	/* compress the file */
	while ((c = tightB_brgetc(&br)) != TIGHTEOF) {
		t_assert(c >= 0 && c <= UCHAR_MAX);
		HuffCode *hc = &ts->codes[c];
		writenbits(&bw, hc->code, hc->nbits);
	}

	/* write EOF */
	writeeof(&bw);
}
