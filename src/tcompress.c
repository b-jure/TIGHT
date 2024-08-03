/*****************************************
 * Copyright (C) 2024 Jure B.
 * Refer to 'tight.h' for license details.
 *****************************************/

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#if defined(TIGHT_TRACE)
#include <ctype.h>
#endif

#include "tdebug.h"
#include "tinternal.h"
#include "tstate.h"
#include "tbuffer.h"



/* write 'magic' */
static inline void writemagic(BuffWriter *bw) {
	t_trace("---Writing [magic]---\n");
	t_assert(bw->len == 0 && bw->validbits == 0);
	tightB_writebyte(bw, MAGIC[0]);
	tightB_writebyte(bw, MAGIC[1]);
	tightB_writebyte(bw, MAGIC[2]);
	tightB_writebyte(bw, MAGIC[3]);
	tightB_writebyte(bw, MAGIC[4]);
	tightB_writebyte(bw, MAGIC[5]);
	tightB_writebyte(bw, MAGIC[6]);
	tightB_writebyte(bw, MAGIC[7]);
	t_tracef(">>> %.*s <<<\n", (int)sizeof(MAGIC), MAGIC);
}


/* write version string */
static inline void writeversion(BuffWriter *bw) {
	t_trace("---Writing [version]---\n");
	tightB_writebyte(bw, TIGHT_VERSION_MAJOR[0]);
	tightB_writebyte(bw, TIGHT_VERSION_MINOR[0]);
	tightB_writebyte(bw, TIGHT_VERSION_RELEASE[0]);
	t_tracef(">>> [0x%02X][0x%02X][0x%02X] <<<\n", 
			TIGHT_VERSION_MAJOR[0],
			TIGHT_VERSION_MINOR[0],
			TIGHT_VERSION_RELEASE[0]);
}


/* write OS */
static inline void writeOS(BuffWriter *bw) {
	t_trace("---Writing [OS]---\n");
#if TIGHT_OS == TIGHT_GNULINUX
	t_tracef(">>> 0x%02X(TIGHT_GNULINUX) <<<\n", TIGHT_OS);
#elif TIGHT_OS == TIGTH_ANDROID
#error OS not yet supported.
#elif TIGHT_OS == TIGHT_WINDOWS
#error OS not yet supported.
#elif TIGHT_OS == TIGHT_MAC
#error OS not yet supported.
#elif TIGHT_OS == TIGHT_FREEBSD
#error OS not yet supported.
#else
#error unsupported operating system.
#endif
	tightB_writebyte(bw, TIGHT_OS);
}


#define ALLMODES	(TIGHT_HUFFMAN | TIGHT_RLE)

/* write compression mode */
static inline void writemode(BuffWriter *bw, int mode) {
	t_trace("---Writing [mode]---\n");
	t_assert(TIGHT_NONE <= mode && mode <= ALLMODES);
	t_assert(mode <= UCHAR_MAX);
	tightB_writebyte(bw, (byte)mode);
	t_tracef(">>> 0x%02X <<<\n", mode);
}


/* auxiliary to 'compressbindata', compress Huffman tree structure */
static void writetree(BuffWriter *bw, TreeData *ht) {
	if (ht->left) { /* parent tree ? */
		tightB_writenbits(bw, 0, 1); /* mark as parent */
		t_trace("[");
		writetree(bw, ht->left); /* traverse left */
		t_trace(", ");
		writetree(bw, ht->right); /* traverse right */
		t_trace("]");
	} else { /* leaf */
		t_assert(ht->right == NULL && 0 <= ht->c && ht->c < 256);
		tightB_writenbits(bw, 1, 1); /* mark as leaf */
		t_tracef((isgraph(ht->c) ? "%c" : "%hu"), ht->c);
		tightB_writenbits(bw, ht->c, 8); /* write symbol */
	}
}


static inline void writebindata(BuffWriter *bw, int mode) {
	if (mode & TIGHT_HUFFMAN) {
		t_assert(bw->ts->hufftree);
		t_trace("---Writing [tree]---\n");
		writetree(bw, bw->ts->hufftree);
		t_trace("\n");
	}
	if (mode & TIGHT_RLE) {/* TODO(jure): implement LZW */}
	tightB_writepending(bw);
	tightB_writefile(bw);
}


/* write MD5 digest */
static inline void writechecksum(BuffWriter *bw, int mode) {
	byte checksum[16];

	t_assert(bw->len == 0); /* buffer must be flushed */
	/* TODO(jure): Implement LZW */
	if (mode & TIGHT_RLE) {
		/* not yet implemented, set dummy checksum */
		memset(checksum, 0, sizeof(checksum));
	} else {
		t_assert(mode & TIGHT_HUFFMAN);
		off_t bindatasz = tightB_seekwriter(bw, 0, SEEK_CUR) - TIGHTbindataoffset;
		t_assert(bindatasz > 0);
		tightB_seekwriter(bw, TIGHTbindataoffset, SEEK_SET);
		tightB_genMD5(bw->ts, bindatasz, bw->fd, checksum);
		t_assert(lseek(bw->fd, 0, SEEK_CUR) == TIGHTbindataoffset + bindatasz);
	}
	t_trace("---Writing [checksum(MD5)]---\n");
	tightB_writebyte(bw, checksum[0]);
	tightB_writebyte(bw, checksum[1]);
	tightB_writebyte(bw, checksum[2]);
	tightB_writebyte(bw, checksum[3]);
	tightB_writebyte(bw, checksum[4]);
	tightB_writebyte(bw, checksum[5]);
	tightB_writebyte(bw, checksum[6]);
	tightB_writebyte(bw, checksum[7]);
	tightB_writebyte(bw, checksum[8]);
	tightB_writebyte(bw, checksum[9]);
	tightB_writebyte(bw, checksum[10]);
	tightB_writebyte(bw, checksum[11]);
	tightB_writebyte(bw, checksum[12]);
	tightB_writebyte(bw, checksum[13]);
	tightB_writebyte(bw, checksum[14]);
	tightB_writebyte(bw, checksum[15]);
	tightD_printchecksum(checksum, sizeof(checksum));
}


/* compress header */
static void writeheader(BuffWriter *bw, int mode) {
	writemagic(bw);
	writeversion(bw);
	writeOS(bw);
	writemode(bw, mode);
	writebindata(bw, mode);
	writechecksum(bw, mode);
	tightB_writefile(bw); /* write all */
}


/* 
 * Write 'eof' for huffman codes; last 'EOFBITS'
 * of the last byte encode how many bits to read
 * starting from the byte before it + 'EOFBIAS'.
 * For example, in case last 'EOFBITS' are '001' (1),
 * then 7 bits will be read starting from the previous byte.
 */
static void writeeof(BuffWriter *bw) {
	if (bw->validbits >= 8) {
		t_tracelong("(", tightD_printbits(bw->tmpbuf, 8), ")");
		tightB_writebyte(bw, bw->tmpbuf);
		bw->tmpbuf >>= 8;
		bw->validbits -= 8;
	}
	t_assert(bw->validbits <= 8);
	if (bw->validbits <= 5) { /* 'eof' can fit ? */
		bw->tmpbuf &= ~(0) >> (8 - bw->validbits);
		bw->tmpbuf <<= 3;
		bw->tmpbuf |= bw->validbits + 2;
		t_tracelong("(", tightD_printbits(bw->tmpbuf, 3), ")");
		tightB_writebyte(bw, bw->tmpbuf);
	} else { /* 'eof' must be in separate byte */
		t_assert(bw->validbits <= 8);
		t_tracelong("(", tightD_printbits(0, 8 - bw->validbits), ")");
		int extrabits = bw->validbits - 6;
		tightB_writenbits(bw, 0, 8 - bw->validbits);
		tightB_writebyte(bw, bw->tmpbuf);
		t_tracelong("(", tightD_printbits(extrabits, 8), ")");
		tightB_writebyte(bw, extrabits);
	}
	bw->tmpbuf = 0;
	bw->validbits = 0;
	tightB_writefile(bw); /* write all */
}
// 00010011



/* compress file contents */
static void huffmancompression(BuffReader *br, BuffWriter *bw) {
	tight_State *ts = br->ts;
	int c;

	t_assert(bw->validbits == 0);
	t_trace("---Compressing [huffman]---\n");
	while ((c = tightB_brgetc(br)) != TIGHTEOF) {
		t_assert(c >= 0 && c <= UCHAR_MAX);
		HuffCode *hc = &ts->codes[c];
		t_trace("("); tightD_printbits(hc->code, hc->nbits); t_trace(")");
		tightB_writenbits(bw, hc->code, hc->nbits);
	}
	writeeof(bw);
	t_trace("\n");
}


/* huffman encoding */
static void compressfile(BuffWriter *bw, BuffReader *br, int mode) {
	t_assert(!(mode & TIGHT_NONE));
	writeheader(bw, mode);
	t_assert(bw->len == 0 && bw->validbits == 0);
	if (mode & TIGHT_RLE) {/* TODO(jure): implement LZW */}
	if (mode & TIGHT_HUFFMAN)
		huffmancompression(br, bw);
}


/* compression data */
typedef struct CompressData {
	const size_t *freqs;
	int mode;
} CompressData;


/* run protected compression */
static void pcompress(tight_State *ts, void *ud) {
	BuffReader br; BuffWriter bw;
	CompressData *cd = (CompressData*)ud;

	if (t_unlikely(cd->mode < 0))
		tightD_compresserror(ts, "invalid mode bits");
	if (cd->mode & TIGHT_NONE)
		return;

	/* init reader and writer */
	tightB_initbr(&br, ts, ts->rfd);
	tightB_initbw(&bw, ts, ts->wfd);

	/* TODO(jure): implement LZW */
	if (cd->mode & TIGHT_HUFFMAN) /* using huffman coding ? */
		tightS_gencodes(ts, cd->freqs);

	t_trace("\n***Compression start!***\n\n");
	compressfile(&bw, &br, cd->mode);
	t_trace("\n***Compressing complete!***\n\n");
}


TIGHT_API int tight_compress(tight_State *ts, int mode, const size_t *freqs) {
	CompressData cd;
	cd.freqs = freqs;
	cd.mode = mode;
	t_assert(ts->rfd >= 0);
	t_assert(ts->wfd >= 0);
	t_assert(ts->rfd != ts->wfd);
	return tightS_protectedcall(ts, &cd, pcompress);
}
