#include <string.h>
#include <limits.h>
#include <unistd.h>

#include "tstate.h"


/* header magic */
const byte MAGIC[8] = { 
	0x54, 0x49, 0x47, 0x48, 0x54, /* T I G H T */
	0x00, 0x00, 0x00, /* padding */
};


typedef struct TreeHeap {
	TreeData *trees[TIGHTBYTES*2]; /* trees */
	int len; /* number of elements in 'trees' */
} TreeHeap;



/* create state */
TIGHT_API tight_State *tight_new(tight_fRealloc frealloc, void *userdata) {
	tight_State *ts = (tight_State *)frealloc(NULL, userdata, 0, SIZEOFSTATE);
	if (t_unlikely(ts == NULL))
		return NULL;
	ts->frealloc = frealloc;
	ts->ud = userdata;
	ts->fmsg = NULL;
	ts->fpanic = NULL;
	ts->hufftree = NULL;
	memset(ts->codes, 0, sizeof(ts->codes));
	ts->ncodes = 0;
	ts->rfd = ts->wfd = -1;
	return ts;
}


/* set error writer */
TIGHT_API tight_fError tight_seterror(tight_State *ts, tight_fError fmsg) {
	tight_fError old = ts->fmsg;
	ts->fmsg = fmsg;
	return old;
}


/* set panic handler */
TIGHT_API tight_fPanic tight_setpanic(tight_State *ts, tight_fPanic fpanic) {
	tight_fPanic old = ts->fpanic;
	ts->fpanic = fpanic;
	return old;
}


/* delete state */
TIGHT_API void tight_free(tight_State *ts) {
	if (ts->hufftree) tightT_freeparent(ts, ts->hufftree);
	if (ts->rfd != -1) close(ts->rfd);
	if (ts->wfd != -1) close(ts->wfd);
	ts->frealloc(ts, ts->ud, SIZEOFSTATE, 0);
}


/* set input and output file descriptors */
TIGHT_API void tight_setfiles(tight_State *ts, int rfd, int wfd) {
	t_assert(rfd >= 0 && wfd >= 0 && rfd != wfd);
	ts->rfd = rfd;
	ts->wfd = wfd;
}


/* swap */
static inline void swap_(TreeData **x, TreeData **y) {
	TreeData *temp = *x;
	*x = *y;
	*y = temp;
}


/* sort tree data */
static void tightqsort(TreeData **v, int start, int end) {
	if (start >= end) return;
    swap_(&v[start], &v[(start + end) >> 1]); /* move pivot to start */
    int last = start;
    for(int i = last + 1; i <= end; i++)
        if (v[i]->freq > v[start]->freq)
            swap_(&v[++last], &v[i]);
    swap_(&v[start], &v[last]);
	tightqsort(v, start, last - 1);
	tightqsort(v, last + 1, end);
}


/* binary search */
static unsigned int getsortedindex(TreeData **v, int l, int h, TreeData *td) {
	int mid;

	while (l < h) {
		mid = (l + h) >> 1;
		if (v[mid]->freq > td->freq) l = mid;
		else h = mid;
	}
	return mid;
}


/* insert tree into tree heap */
static void sortedinsert(TreeHeap *ht, TreeData *td) {
	unsigned int i = getsortedindex(ht->trees, 0, ht->len - 1, td);
	memmove(&ht->trees[i + 1], &ht->trees[i], ht->len - i);
	ht->trees[i] = td;
	ht->len++;
}


/* reverse bits */
static inline unsigned reversebits(unsigned code, int len) {
	register unsigned res;
	t_assert(len > 0);
	do {
		res |= code & 1;
		code >>= 1;
		res <<= 1;
	} while (len-- > 0);
	return res >> 1;
}


/* generate huffman codes table and build huffman tree */
static void tightS_gencodes(tight_State *ts, size_t freqs[TIGHTBYTES]) {
	TreeHeap ht = { 0 }; /* huffman tree stack */
	size_t combfreqs[TIGHTBYTES * 2]; /* freqs + combined frequencies */
	int parentindexes[TIGHTBYTES * 2]; /* parent frequency indexes in 'combfreqs' */
	int fi = TIGHTBYTES; /* next available position in 'combfreqs' */
	TreeData *t1, *t2; /* left/right subtree */
	int i; /* loop counter */

	/* copy over counts */
	mempcpy(combfreqs, freqs, TIGHTBYTES);
	memset(&combfreqs[TIGHTBYTES], 0, TIGHTBYTES);

	/* make leafs */
	for (i = 0; i < TIGHTBYTES; i++)
		if (combfreqs[i] != 0)
			ht.trees[ht.len++] = tightT_newleaf(ts, combfreqs[i], i);

	/* sort leaf trees */
	tightqsort(ht.trees, 0, ht.len - 1);

	/* build huffman tree and frequency array */
	while (ht.len > 1) {
		t1 = ht.trees[--ht.len]; /* left subtree */
		t2 = ht.trees[--ht.len]; /* right subtree */
		sortedinsert(&ht, tightT_newparent(ts, t1, t2, fi)); /* t1 <- p -> t2 */
		combfreqs[fi] = t1->freq + t2->freq;
		parentindexes[t1->c] = -fi; /* left */
		parentindexes[t2->c] = fi; /* right */
		fi++; /* advance index into 'combfreqs' */
	}
	t_assert(sb.len == 1);
	t1 = ht.trees[--ht.len]; /* pop huffman tree */
	parentindexes[t1->c] = t1->c; /* mark as no parent (root) */
	ts->hufftree = t1; /* anchor to state */

	/* build huffman codes */
	int nbits, code, y;
	for (i = 0; i < TIGHTBYTES; i++) {
		if (combfreqs[i] == 0) /* skip 0 frequency bytes */
			continue;
		y = i; /* preserve 'i' */
		nbits = code = 0;
		/* build the huffman code */
		while (t_abs(parentindexes[y]) != y) { /* while not tree root */
			code |= (parentindexes[y] >= 0) << nbits; /* 1 if right, 0 if left */
			y = t_abs(parentindexes[y]); /* follow the node to root */
			nbits++; /* increment number of bits in 'code' */
		}
		t_assert(nbits > 0);
		ts->codes[i].code = reversebits(code, nbits);
		ts->codes[i].nbits = nbits;
	}
}
