#include <string.h>
#include <limits.h>

#include "pfcstate.h"



/* create new huffman code */
#define newcode(nb,c)		(HuffCode){.nbits = nb, .code = c}



typedef struct SortedBytes {
	int bytes[PFC_BYTES*2]; /* bytes */
	int len; /* number of elements in 'bytes' */
	size_t *freqs;
} SortedBytes;



/* swap */
static inline void swap_(int *x, int *y) {
	*x ^= *y;
	*y ^= *x;
	*x ^= *y;
}


/* sort bytes */
static void pfcqsort(int *const v, void* ud, int start, int end) {
	size_t *freqs = (size_t*)ud;
	if (start >= end) return;
    swap_(&v[start], &v[(start + end) >> 1]); /* move pivot to start */
    int last = start;
    for(int i = last + 1; i <= end; i++)
        if (freqs[v[i]] > freqs[v[start]])
            swap_(&v[++last], &v[i]);
    swap_(&v[start], &v[last]);
	pfcqsort(v, ud, start, last - 1);
	pfcqsort(v, ud, last + 1, end);
}


/* return index where to insert the byte */
static int getsortedindex(const int *bytes, int l, int h, int byte) {
	int mid;

	while (l < h) {
		mid = (l + h) >> 1;
		if (bytes[mid] > byte) l = mid;
		else h = mid;
	}
	return mid;
}


/* insert 'byte' into sorted bytes */
static void sortedinsert(SortedBytes *sb, int byte) {
	int i = getsortedindex(sb->bytes, 0, sb->len - 1, byte);
	memmove(&sb->bytes[i + 1], &sb->bytes[i], sb->len - i);
	sb->bytes[i] = byte;
	sb->len++;
}


/* generate huffman codes */
void pfcS_gencodes(pfc_State *ps, size_t freqs[PFC_BYTES]) {
	SortedBytes sb = { 0 }; /* storage for (sorted) bytes */
	size_t combfreqs[PFC_BYTES * 2]; /* freqs + combined frequencies */
	int nodeidx[PFC_BYTES * 2]; /* combined nodes indexes in 'combfreqs' */
	int fi = PFC_BYTES; /* next available position in 'combfreqs' */
	int b1, b2; /* left/right node byte */
	int i; /* loop counter */

	/* copy over 'freqs' and initialize upper part to 0 */
	memcpy(combfreqs, freqs, sizeof(size_t)*PFC_BYTES);
	memset(&combfreqs[PFC_BYTES], 0, sizeof(size_t)*PFC_BYTES);

	/* add all bytes with frequency bigger than 0 */
	for (i = 0; i < PFC_BYTES; i++)
		if (combfreqs[i] != 0)
			sb.bytes[sb.len++] = i;

	/* sort unsorted bytes */
	pfcqsort(sb.bytes, combfreqs, 0, sb.len);

	/* build huffman tree */
	while (sb.len > 1) {
		b1 = sb.bytes[--sb.len]; /* left side byte */
		b2 = sb.bytes[--sb.len]; /* right side byte */
		combfreqs[fi] = combfreqs[b1] + combfreqs[b2]; /* combine frequencies */
		sortedinsert(&sb, fi); /* b1 <- R -> b2 */
		nodeidx[b1] = -fi; /* left */
		nodeidx[b2] = fi; /* right */
		fi++; /* advance index into 'combfreqs' */
	}
	pfc_assert(sb.len == 1);
	b1 = sb.bytes[--sb.len];
	nodeidx[b1] = b1; /* index is the actual byte */

	int nbits, code, y;
	for (i = 0; i < PFC_BYTES; i++) {
		if (combfreqs[i] == 0) /* skip 0 frequency bytes */
			continue;
		y = i; /* preserve 'i' */
		nbits = code = 0;
		/* build the huffman code */
		while (p_abs(nodeidx[y]) != y) { /* while not tree root */
			code |= (nodeidx[y] >= 0) << nbits; /* 1 if right, 0 if left */
			y = p_abs(nodeidx[y]); /* follow the node to root */
			nbits++; /* increment number of bits in 'code' */
		}
		ps->codes[i] = newcode(nbits, code);
	}
}
