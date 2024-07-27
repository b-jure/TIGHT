#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "tdebug.h"
#include "tinternal.h"
#include "tstate.h"
#include "tbuffer.h"


/* write 'magic' */
static inline void encodemagic(BuffWriter *bw) {
	/* unroll */
	t_assert(bw->len == 0 && bw->validbits == 0);
	tightB_writebyte(bw, MAGIC[0]);
	tightB_writebyte(bw, MAGIC[1]);
	tightB_writebyte(bw, MAGIC[2]);
	tightB_writebyte(bw, MAGIC[3]);
	tightB_writebyte(bw, MAGIC[4]);
	tightB_writebyte(bw, MAGIC[5]);
	tightB_writebyte(bw, MAGIC[6]);
	tightB_writebyte(bw, MAGIC[7]);
}


/* auxiliary to 'encodebindata', encodes Huffman tree structure */
static void encodetree(BuffWriter *bw, TreeData *ht) {
	if (ht->left) { /* parent tree ? */
		tightB_writenbits(bw, 0, 1); /* mark as parent */
		encodetree(bw, ht->left); /* traverse left */
		encodetree(bw, ht->right); /* traverse right */
	} else { /* leaf */
		t_assert(ht->right == NULL && 0 <= ht->c && ht->c < 256);
		tightB_writenbits(bw, 1, 1); /* mark as leaf */
		tightB_writenbits(bw, ht->c, 8); /* write symbol */
	}
}


static inline void encodebindata(BuffWriter *bw) {
	t_assert(bw->ts->hufftree);
	encodetree(bw, bw->ts->hufftree);
	tightB_writepending(bw);
	tightB_writefile(bw);
}


/* write MD5 digest */
static inline void encodechecksum(BuffWriter *bw, BuffReader *br) {
	tight_State *ts = bw->ts;
	byte md5digest[16];

	/* get size of 'bindata' */
	long bindatasz = lseek(ts->wfd, 0, SEEK_CUR) - TIGHTbindataoffset;
	if (t_unlikely(bindatasz < 0))
		tightD_errnoerr(bw->ts, "lseek error in output file");
	t_assert(hlen > 0);

	/* seek to start of the 'bindata' */
	if (t_unlikely(lseek(ts->wfd, TIGHTbindataoffset, SEEK_SET) < 0))
		tightD_errnoerr(bw->ts, "lseek error in output file");
	tightB_genMD5(br, bindatasz, md5digest);

	/* unroll */
	tightB_writebyte(bw, md5digest[0]);
	tightB_writebyte(bw, md5digest[0]);
	tightB_writebyte(bw, md5digest[1]);
	tightB_writebyte(bw, md5digest[2]);
	tightB_writebyte(bw, md5digest[3]);
	tightB_writebyte(bw, md5digest[4]);
	tightB_writebyte(bw, md5digest[5]);
	tightB_writebyte(bw, md5digest[6]);
	tightB_writebyte(bw, md5digest[7]);
	tightB_writebyte(bw, md5digest[8]);
	tightB_writebyte(bw, md5digest[8]);
	tightB_writebyte(bw, md5digest[9]);
	tightB_writebyte(bw, md5digest[10]);
	tightB_writebyte(bw, md5digest[11]);
	tightB_writebyte(bw, md5digest[12]);
	tightB_writebyte(bw, md5digest[13]);
	tightB_writebyte(bw, md5digest[14]);
	tightB_writebyte(bw, md5digest[15]);
	tightB_writefile(bw);
}


/* compress/encode header */
static inline void encodeheader(BuffWriter *bw, BuffReader *br) {
	encodemagic(bw);
	encodebindata(bw);
	encodechecksum(bw, br);
}


/* compress file contents */
static void encodefile(BuffReader *br, BuffWriter *bw) {
	tight_State *ts = br->ts;
	int c;

	while ((c = tightB_brgetc(br)) != TIGHTEOF) {
		t_assert(c >= 0 && c <= UCHAR_MAX);
		HuffCode *hc = &ts->codes[c];
		tightB_writenbits(bw, hc->code, hc->nbits);
	}
}


/* 
 * Write 'eof' for huffman codes; last 'EOFBITS'
 * of the last byte encode how many bits to read
 * from the byte before it + 'EOFBIAS'.
 * For example, in case last 'EOFBITS' are '001' (1),
 * then 7 bits will be read from the previous byte.
 */
static void encodeeof(BuffWriter *bw) {
	int extra = 0;

	if (bw->validbits >= 8) {
		tightB_writebyte(bw, bw->tmpbuf);
		bw->tmpbuf >>= 8;
		bw->validbits -= 8;
		extra = 2;
	}
	if (bw->validbits <= 5) { /* 'eof' can fit ? */
		bw->tmpbuf <<= 8 - bw->validbits; /* shift valid bits */
		bw->tmpbuf |= bw->validbits + extra; /* add 'eof' bits */
		tightB_writebyte(bw, bw->tmpbuf);
	} else { /* 'eof' must be in separate byte */
		t_assert(EOFBIAS <= bw->validbits);
		tightB_writebyte(bw, bw->tmpbuf);
		tightB_writebyte(bw, bw->validbits - EOFBIAS);
	}
	bw->validbits = 0;
	tightB_writefile(bw); /* write all */
}


/* 
 * Encode input file, write the result into output file; 
 * user of the API should ensure that the input file is a
 * regular file and not a directory, symbolic link or any
 * other special file, otherwise some system calls might
 * have undefined behaviour.
 */
TIGHT_API void tight_encode(tight_State *ts) {
	BuffReader br; BuffWriter bw;

	/* must have valid files */
	t_assert(ts->rfd >= 0 && ts->wfd >= 0 && ts->rfs != ts->wfd);

	/* init reader and writer */
	memset(&br, 0, sizeof(br));
	memset(&bw, 0, sizeof(bw));
	br.ts = bw.ts = ts;

	/* compress/encode the file */
	encodeheader(&bw, &br);
	encodefile(&br, &bw);
	encodeeof(&bw);
}
