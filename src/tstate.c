#include <string.h>
#include <limits.h>
#include <unistd.h>

#include "tstate.h"
#include "tdebug.h"


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
	if (ts->hufftree) 
		tightT_freeparent(ts, ts->hufftree);
	ts->frealloc(ts, ts->ud, SIZEOFSTATE, 0);
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
static unsigned int getsortedindex(TreeData **v, int low, int high, TreeData *td) {
	int mid;

	if (t_unlikely(low >= high)) return low;
	while (low <= high) {
		mid = low + ((high - low) >> 1);
		if (v[mid]->freq > td->freq) 
			low = mid + 1;
		else
			high = mid - 1;
	}
	return mid;
}


/* insert tree into tree heap */
static void sortedinsert(TreeHeap *ht, TreeData *td) {
	int i = getsortedindex(ht->trees, 0, ht->len - 1, td);
	memmove(&ht->trees[i + 1], &ht->trees[i], (ht->len - i) * sizeof(TreeData));
	ht->trees[i] = td;
	ht->len++;
	printf("TreeHeap:");
	for (i = 0; i < ht->len; i++)
		printf(" [%zu]", ht->trees[i]->freq);
	printf("\n");
	fflush(stdout);
}


/* reverse bits */
static inline unsigned reversebits(uint code, int len) {
	register uint res = 0;
	t_assert(0 < len  && len < 16);
	do {
		res |= code & 1;
		code >>= 1;
		res <<= 1;
	} while (--len > 0);
	return res >> 1;
}


/* generate huffman codes table and build huffman tree */
void tightS_gencodes(tight_State *ts, const size_t *freqs) {
	TreeHeap ht = { 0 }; /* huffman tree stack */
	size_t combfreqs[TIGHTCODES]; /* freqs + combined frequencies */
	int parents[TIGHTCODES]; /* parent frequency indexes in 'combfreqs' */
	TreeData *t1, *t2; /* left/right subtree */
	int fi = TIGHTBYTES; /* next available position in 'combfreqs' */
	int i; /* loop counter */

	/* copy over counts */
	memcpy(combfreqs, freqs, TIGHTBYTES * sizeof(size_t));
	memset(&combfreqs[TIGHTBYTES], 0, TIGHTBYTES * sizeof(size_t));

	/* make leafs */
	for (i = 0; i < TIGHTBYTES; i++)
		if (combfreqs[i] != 0)
			ht.trees[ht.len++] = tightT_newleaf(ts, combfreqs[i], i);

	printf("built %d leaf trees\n", ht.len);
	fflush(stdout);
	/* sort leaf trees */
	tightqsort(ht.trees, 0, ht.len - 1);
	printf("TreeHeap:");
	for (i = 0; i < ht.len; i++)
		printf(" [%zu]", ht.trees[i]->freq);
	printf("\n");
	fflush(stdout);

	/* build huffman tree and frequency array */
	while (ht.len > 1) {
		t1 = ht.trees[--ht.len]; /* left subtree */
		t2 = ht.trees[--ht.len]; /* right subtree */
		sortedinsert(&ht, tightT_newparent(ts, t1, t2, fi)); /* t1 <- p -> t2 */
		combfreqs[fi] = t1->c + t2->freq;
		parents[t1->c] = -fi; /* left */
		parents[t2->c] = fi; /* right */
		fi++; /* advance index into 'combfreqs' */
	}
	t_assert(ht.len == 1);
	t1 = ht.trees[--ht.len]; /* pop huffman tree */
	parents[t1->c] = t1->c;
	ts->hufftree = t1; /* anchor to state */

	tightD_printtree(ts->hufftree);
	/* build huffman codes */
	int nbits, code, y;
	memset(ts->codes, 0, sizeof(ts->codes));
	for (i = 0; i < TIGHTBYTES; i++) {
		if (combfreqs[i] == 0) /* skip 0 frequency bytes */
			continue;
		y = i; /* preserve 'i' */
		nbits = code = 0;
		/* build the huffman code */
		printf("parents[%d]: ", i);
		while (t_abs(parents[y]) != y) { /* while not tree root */
			code |= (parents[y] >= 0) << nbits; /* 1 if right, 0 if left */
			y = t_abs(parents[y]); /* follow the node to root */
			nbits++; /* increment number of bits in 'code' */
		}
		t_assert(nbits > 0 || parents[y] == t1->c);
		if (t_likely(nbits > 0)) {
			ts->codes[i].code = reversebits(code, nbits);
			ts->codes[i].nbits = nbits;
		} else {
			ts->codes[i].code = ts->codes[i].nbits = 0;
		}
		printf("<code> ");
		tightD_printbits(ts->codes[i].code, nbits);
		printf(", <nbits> %d", nbits);
		printf("\n");
		fflush(stdout);
	}
}


/* 
 * Set input and output file descriptors and generate
 * huffman codes table based on provided frequencies.
 * 'freqs' must be of size 256 (or TIGHTBYTES).
 * In case 'freqs' is NULL then internal frequency table
 * is used 'internal_freqs'.
 */
TIGHT_API void tight_setfiles(tight_State *ts, int rfd, int wfd) {
	t_assert(rfd >= 0 && wfd >= 0 && rfd != wfd);
	if (ts->hufftree)
		tightT_freeparent(ts, ts->hufftree);
	memset(ts->codes, 0, sizeof(ts->codes));
	ts->rfd = rfd;
	ts->wfd = wfd;
}
