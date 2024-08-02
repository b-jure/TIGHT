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



/* read header 'magic' */
static void readmagic(BuffReader *br) {
	int c;
	t_trace("---Reading [magic]---\n");
	for (uint i = 0; (c = tightB_brgetc(br)) != TIGHTEOF; i++) {
		if (t_unlikely(c != MAGIC[i]))
			tightD_headererror(br->ts, " (invalid magic bytes)");
		if (t_unlikely(i == sizeof(MAGIC) - 1)) {
			t_tracef(">>> %.*s <<<\n", (int)sizeof(MAGIC), MAGIC);
			return; /* ok */
		}
	}
	tightD_headererror(br->ts, " (missing magic bytes)");
}


/* read version */
static void readversion(BuffReader *br, TIGHT *header) {
	int c;
	t_trace("---Reading [version]---\n");
	for (int i = 0; (c = tightB_brgetc(br)) != TIGHTEOF; i++) {
		header->version[i] = c;
		if (i == sizeof(header->version) - 1) {
			t_tracef(">>> %.*s <<<\n", (int)sizeof(header->version), header->version);
			if (t_unlikely(header->version[0] != TIGHT_VERSION_MAJOR[0]))
				tightD_versionerror(br->ts, header->version[0]);
			return; /* ok */
		}
	}
	tightD_headererror(br->ts, " (missing version bytes)");
}


/* read operating system byte */
static void readOS(BuffReader *br, TIGHT *header) {
	t_trace("---Reading [OS]---\n");
	int c = tightB_brgetc(br);
	if (t_unlikely(c == TIGHTEOF))
		tightD_headererror(br->ts, " (missing OS byte)");
	header->os = c;
	t_tracef(">>> %d <<<\n", c);
}


/* read compression 'mode' */
static void readmode(BuffReader *br, TIGHT *header) {
	t_trace("---Reading [mode]---\n");
	int mode = tightB_brgetc(br);
	if (t_unlikely(mode == TIGHTEOF))
		tightD_headererror(br->ts, " (missing mode byte)");
	t_tracef(">>> %d <<<\n", mode);
	header->mode = (byte)mode;
}


/* auxiliary to 'readbindata' */
static TreeData *decompresstree(BuffReader *br) {
	int bits = tightB_readnbits(br, 1);
	if (bits == 0) { /* parent ? */ 
		t_trace("[");
		TreeData *t1 = decompresstree(br);
		t_trace(", ");
		TreeData *t2 = decompresstree(br);
		t_trace("]");
		return tightT_newparent(br->ts, t1, t2, 0);
	} else { /* leaf */
		bits = tightB_readnbits(br, 8); /* get symbol */
		t_tracef((isgraph(bits) ? "%c" : "%d"), bits);
		return tightT_newleaf(br->ts, 0, bits);
	}
}


/* decompress header 'bindata' */
static inline void readbindata(BuffReader *br, TIGHT* header) {
	if (header->mode & TIGHT_HUFFMAN) { /* have huffman tree ? */
		t_trace("---Decompressing [tree]----\n");
		br->ts->hufftree = decompresstree(br); /* anchor to state */
		header->bindata = 1;
		t_trace("\n");
		tightD_printtree(br->ts->hufftree);
		t_assert(br->ts->hufftree != NULL);
		t_assert(br->validbits > 0); /* should have leftover */
		tightB_readpending(br, NULL); /* rest is just padding */
	}
}


/* decompress header 'checksum' */
static void readchecksum(BuffReader *br, TIGHT *header) {
	int c;
	t_trace("---Decompressing [checksum]---\n");
	for (size_t i = 0; (c = tightB_brgetc(br)) != TIGHTEOF; i++) {
		header->checksum[i] = c;
		if (i == sizeof(header->checksum) - 1) {
			tightD_printchecksum(header->checksum, sizeof(header->checksum));
			return; /* ok */
		}
	}
	tightD_headererror(br->ts, " (missing checksum bytes)");
}


/* verify decompressed 'checksum' */ 
static void verifychecksum(BuffReader *br, TIGHT *header, off_t bindatastart,
						   ulong bindatasize) 
{
	byte out[16];
	off_t offset;
	if (t_unlikely((offset = lseek(br->fd, bindatastart, SEEK_SET)) < 0))
		tightD_errnoerror(br->ts, "lseek (input file)");
	t_assert(br->validbits == 0);
	tightB_initbr(br, br->ts, br->fd); /* reset reader */
	tightB_genMD5(br->ts, bindatasize, br->fd, out);
	t_assert((ulong)tightB_offsetreader(br) == bindatastart + bindatasize);
	int res = memcmp(out, header->checksum, sizeof(out));
	if (t_unlikely(res != 0))
		tightD_headererror(br->ts, " (checksum doesn't match)");
}


/* decompress 'TIGHT' header */
static void readheader(BuffReader *br, TIGHT *header) {
	t_assert(sizeof(header->magic) == sizeof(MAGIC));
	memset(header, 0, sizeof(*header));
	memcpy(header->magic, MAGIC, sizeof(header->magic));
	readmagic(br);
	readversion(br, header);
	readOS(br, header);
	readmode(br, header);
	off_t bindatastart = tightB_offsetreader(br);
	readbindata(br, header);
	off_t bindatasize = tightB_offsetreader(br) - bindatastart;
	readchecksum(br, header);
	off_t endofheader = tightB_offsetreader(br);
	verifychecksum(br, header, bindatastart, bindatasize);
	if (t_unlikely(lseek(br->fd, endofheader, SEEK_SET) < 0))
		tightD_errnoerror(br->ts, "lseek (input file)");
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


/* decompress file contents */
static inline void huffmandecompression(BuffWriter *bw, BuffReader *br) {
	tight_State *ts = bw->ts;
	TreeData *at = ts->hufftree; /* checkpoint */
	int ahead, sym = -1, code, left;

	t_assert(br->validbits == 0); /* data must be aligned */
	t_assert(ts->hufftree != NULL); /* must have decompressed huffman tree */
	t_trace("---Decompressing [huffman]---\n");

	int c1 = tightB_brgetc(br);
	if (t_unlikely(c1 == TIGHTEOF)) { /* empty file ? */
		t_assert(bw->len == 0); /* nothing to write */
		return;
	}

	int c2 = tightB_brgetc(br);
	if (t_unlikely(c2 == TIGHTEOF)) { /* 'eof' in 'c1' ? */
		left = geofbits(c1) - 2; /* eof without bias */
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
	/* eof with bias and previous byte has 8 valid bits */
	left = geofbits(code >> 8) + EOFBIAS;
	t_assert(left <= MAXCODE - EOFBITS);
	code = ((code >> (MAXCODE - left)) & 0xff00) | (code & 0xff);

eof:
	while (left > 0) {
		t_trace((sym != -1 ? "," : ""));
		sym = getsymbol(&at, at, &code, &left, 0);
		if (t_unlikely(sym == -1)) {
			t_trace("\n");
			tightD_decompresserror(ts, "invalid huffman code");
		}
		at = ts->hufftree;
		t_assert(sym >= 0);
		tightB_writebyte(bw, sym);
	}
	t_trace("]\n");
	t_assert(left == 0);
	tightB_writefile(bw); /* write all */
}


/* TODO(jure): implement LZW */
/* TODO(jure): Implement Vitter algorithm */
/* TODO(jure): combine Huffman and LZW to prevent reading the file twice */
/* protected decompression */
static void pdecompress(tight_State *ts, void *ud) {
	TIGHT header;
	BuffReader br; BuffWriter bw;

	t_trace("\n***Decompression start!***\n\n");
	(void)ud; /* unused */
	tightB_initbr(&br, ts, ts->rfd);
	tightB_initbw(&bw, ts, ts->wfd);
	readheader(&br, &header);
	if (header.mode & TIGHT_HUFFMAN)
		huffmandecompression(&bw, &br);
	if (header.mode & TIGHT_RLE) {}
	t_trace("\n***Decompression complete!***\n\n");
}


TIGHT_API int tight_decompress(tight_State *ts) {
	t_assert(ts->rfd >= 0 && ts->wfd >= 0 && ts->rfd != ts->wfd);
	return tightS_protectedcall(ts, NULL, pdecompress);
}
