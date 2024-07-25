#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "tdebug.h"
#include "tinternal.h"
#include "tstate.h"
#include "tbuffer.h"


/* size of 'tempbuf' in 'BuffWriter' */
#define TMPBsize		16


/* buffered writer */
typedef struct BuffWriter {
	tight_State *ts; /* state */
	const char *filepath; /* debug */
	uint len; /* number of elements in 'buf' */
	byte buf[TIGHT_WBUFFSIZE]; /* write buffer */
	int validbits; /* valid bits in 'tmpbuf' */
	ushrt tmpbuf; /* temporary bits buffer */
} BuffWriter;


/* flush 'buf' into the file */
static inline void writefile(BuffWriter *bw) {
	tight_State *ts = bw->ts;
	if (t_unlikely(bw->len > 0 && write(ts->wfd, bw->buf, bw->len) < 0))
		tightD_errnoerr(ts, "write error while writting to '%s%s'", bw->filepath,
							".tight");
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
	writebyte(bw, shrt >> 1);
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
		tightD_errnoerr(bw->ts, "lseek error for '%s%s'", bw->filepath, ".tight");
	t_assert(hlen > 0);
	/* seek to start of the 'bindata' */
	if (t_unlikely(lseek(ts->wfd, TIGHTbindataoffset, SEEK_SET) < 0))
		tightD_errnoerr(bw->ts, "lseek error for '%s%s'", bw->filepath, ".tight");

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


/* write 'eof' for huffman codes */
static void writeeof(BuffWriter *bw) {

}


/* encode file 'filepath' */
static int encodefile(tight_State *ts, const char *filepath) {
	BuffReader br;
	BuffWriter bw;
	Buffer outfile;
	int c;

	/* init reader */
	memset(&br, 0, sizeof(br));
	br.ts = ts;
	br.filepath = filepath;

	/* init writer */
	memset(&bw, 0, sizeof(bw));
	bw.ts = ts;
	bw.filepath = filepath;
	tightB_init(ts, &outfile);
	tightB_addstring(ts, &outfile, filepath, strlen(filepath)); /* base name */
	tightB_addstring(ts, &outfile, ".tight", sizeof(".tight")); /* suffix */
	ts->wfd = open(outfile.str, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
	tightB_free(ts, &outfile);
	if (t_unlikely(ts->wfd < 0))
		tightD_errnoerr(ts, "can't create '%s%s'", filepath, ".tight");

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
	return 1;
}


/* 
 * Encode 'filename'; only regular files are encoded,
 * directories, symbolic links and other special files
 * are ignored.
 */
int tightE_encode(tight_State *ts, const char *filepath) {
	struct stat st;
	const char *dotp;

	if (lstat(filepath, &st) < 0)
		tightD_errnoerr(ts, "couldn't stat file '%s'", filepath);
	if (S_ISREG(st.st_mode)) { /* regular file ? */
		ts->rfd = open(filepath, O_RDONLY);
		if (t_unlikely(ts->rfd < 0))
			tightD_errnoerr(ts, "couldn't open '%s'", filepath);
		return encodefile(ts, filepath);
	}
	return 0;
}
