/*****************************************
 * Copyright (C) 2024 Jure B.
 * Refer to 'tight.h' for license details.
 *****************************************/

#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>

#include "tstate.h"
#include "tdebug.h"
#include "talloc.h"



/* 
 * Internal frequency table 
 * Generated by scanning my personal home directory...
 * TODO(jure): clamp these values.
 */
static const size_t internal_freqs[TIGHTBYTES] = {
	19545057148, 3531108214, 1608094686, 1345198511, 1051892339, 1281745510,
	1025513827, 1293687674, 1074268988, 1039079184, 1506712383, 1006938116,
	795036552, 819284256, 852720474, 968347569, 1196237159, 807296250,
	786485745, 731167573, 781870300, 763658141, 738518199, 758042149,
	829081582, 685694952, 678796172, 676470373, 702196128, 666433739,
	683222022, 736999294, 2958994857, 779549982, 970811770, 748844265,
	2791295041, 782214617, 749453427, 746809483, 901992964, 854420995,
	853510029, 752541364, 862078787, 895318580, 871196875, 996286245,
	1142965387, 951312275, 897361636, 832370440, 833594196, 877280491,
	816667022, 818486808, 852146631, 857172604, 853105995, 776436170,
	846287620, 806242722, 824778664, 891514664, 1568753874, 978313472,
	853034202, 828046685, 951234698, 904186093, 765447813, 721993000,
	952508445, 2772622197, 763032728, 707757167, 809345295, 784046744,
	779820569, 748080777, 916397991, 741497902, 895733761, 825590259,
	921582190, 1410512585, 779314963, 852379720, 829165307, 710956480,
	776220169, 721986867, 755255426, 745245184, 763461684, 1211140497,
	1227317123, 1180594804, 925826420, 1026633696, 1017023507, 1432370508,
	918557547, 816561729, 918723200, 1164397897, 777094249, 792277091,
	1016234870, 960079981, 1126487943, 1098093242, 978665597, 721869246,
	1191747305, 1138392234, 1264131985, 955209081, 833546979, 843656902,
	904019433, 781422670, 816126058, 791862043, 720578188, 765742304,
	788591215, 1147773006, 1712655311, 817868332, 844748524, 823909767,
	812708759, 736280498, 745345572, 701548435, 802590272, 884113835,
	786187744, 858603931, 734290325, 788617276, 705835575, 717950119,
	848129761, 713005070, 2568746106, 705717055, 759590307, 752072442,
	717126578, 711291732, 729814559, 712423716, 696898312, 704859226,
	709502527, 676758481, 677280091, 689800715, 1242406300, 685941610,
	763057463, 701885520, 731867652, 743022578, 716962124, 702450609,
	822948599, 753694915, 1518687795, 827312002, 727994792, 824051557,
	763994388, 815026818, 721157140, 673981331, 688732575, 669665978,
	699130050, 853058759, 764322091, 728838860, 702351647, 685593462,
	761087802, 743002265, 687169052, 845386414, 755058012, 849555754,
	1379241633, 673989477, 680363645, 699588927, 691508678, 680381279,
	717122561, 730331068, 688976887, 719295107, 686018355, 698392536,
	715046721, 708421468, 717085182, 703626425, 745836891, 675432857,
	674991176, 674358242, 667656210, 845247119, 734289225, 776049057,
	707136643, 669937268, 691263779, 745442244, 703373116, 746489879,
	740342449, 775085830, 1481481747, 652846418, 689050812, 690898299,
	684159252, 680178686, 688683763, 734799744, 798865773, 715575417,
	805350383, 776156546, 695178132, 739140401, 792396706, 813911924,
	795949588, 702618702, 672532244, 704033862, 682235836, 840446583,
	748366370, 809374385, 967943703, 818837829, 1044098607, 1498659876,
	1565985814, 1125047180, 1200921949, 3807361529, 
};


/* header magic */
const byte MAGIC[8] = { 
	0x54, 0x49, 0x47, 0x48, 0x54, /* T I G H T */
	0x00, 0x00, 0x00, /* padding */
};


typedef struct TreeHeap {
	TreeData *trees[TIGHTCODES]; /* trees */
	int len; /* number of elements in 'trees' */
} TreeHeap;



/* create state */
TIGHT_API tight_State *tight_new(tight_fRealloc frealloc, void *userdata) {
	tight_State *ts = (tight_State *)frealloc(NULL, userdata, 0, SIZEOFSTATE);
	if (t_unlikely(ts == NULL))
		return NULL;
	ts->frealloc = frealloc;
	ts->ud = userdata;
	ts->error = NULL;
	ts->temp = NULL;
	ts->hufftree = NULL;
	memset(ts->codes, 0, sizeof(ts->codes));
	ts->errjmp = NULL;
	ts->rfd = ts->wfd = -1;
	return ts;
}


/* delete state */
TIGHT_API void tight_free(tight_State *ts) {
	if (ts->hufftree) 
		tightT_freeparent(ts, ts->hufftree);
	if (ts->error)
		tightA_free(ts, ts->error, strlen(ts->error) + 1);
	for (TempMem *curr = ts->temp; curr != NULL; curr = curr->next)
		tightA_freetempmem(ts, curr);
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


#if defined(TIGHT_TRACE)
/* print tree heap (debug) */
static void printTreeHeap(TreeHeap *ht) {
	t_trace("TreeHeap[");
	for (int i = 0; i < ht->len; i++) {
		t_tracef("%zu", ht->trees[i]->freq);
		if (i + 1 < ht->len) t_trace(",");
	}
	t_trace("]\n");
}
#else
#define printTreeHeap(ht)		((void)0)
#endif


/* insert tree into tree heap */
static void sortedinsert(TreeHeap *ht, TreeData *td) {
	int i = getsortedindex(ht->trees, 0, ht->len - 1, td);
	memmove(&ht->trees[i + 1], &ht->trees[i], (ht->len - i) * sizeof(ht->trees[0]));
	ht->trees[i] = td;
	ht->len++;
}


/* reverse bits */
static inline unsigned reversebits(uint code, int len) {
	register uint res = 0;
	t_assert(0 < len  && len <= 16);
	do {
		res |= code & 1;
		code >>= 1;
		res <<= 1;
	} while (--len > 0);
	return res >> 1;
}


/* TODO(jure): 'Tree' doesn't need frequency, pass 'combfreqs'
 * as userdata to quicksort implementation */
/* generate huffman codes table and build huffman tree */
void tightS_gencodes(tight_State *ts, const size_t *freqs) {
	TempMem *tm, *tmstart = ts->temp;
	TreeHeap ht; /* huffman tree stack */
	size_t combfreqs[TIGHTCODES]; /* freqs + combined frequencies */
	int parents[TIGHTCODES]; /* parent frequency indexes in 'combfreqs' */
	TreeData *t1, *t2, *t; /* left/right subtree */
	int fi = TIGHTBYTES; /* next available position in 'combfreqs' */
	int i; /* loop counter */

	if (t_unlikely(freqs == NULL)) /* use internal_freqs ? */
		freqs = internal_freqs;

	memset(ts->codes, 0, sizeof(ts->codes));
	memset(&ht, 0, sizeof(ht));
	memcpy(combfreqs, freqs, TIGHTBYTES * sizeof(size_t));
	memset(&combfreqs[TIGHTBYTES], 0, TIGHTBYTES * sizeof(size_t));

makeleafs:
	for (i = 0; i < TIGHTBYTES; i++) {
		if (combfreqs[i] != 0) {
			tm = tightA_newtempmem(ts);
			t = tightT_newleaf(ts, combfreqs[i], i);
			updatetm(tm, t, sizeof(*t));
			ht.trees[ht.len++] = t;
		}
	}

	if (t_unlikely(ht.len == 0)) { /* not a single leaf tree ? */
		/* use 'internal_freqs' to generate leafs */
		memcpy(combfreqs, internal_freqs, TIGHTBYTES * sizeof(size_t));
		goto makeleafs;
	}

	tightqsort(ht.trees, 0, ht.len - 1); /* sort leaf trees */
	printTreeHeap(&ht);

	/* build huffman tree and frequency array */
	while (ht.len > 1) {
		t1 = ht.trees[--ht.len]; /* left subtree */
		t2 = ht.trees[--ht.len]; /* right subtree */
		tm = tightA_newtempmem(ts);
		t = tightT_newparent(ts, t1, t2, fi);
		updatetm(tm, t, sizeof(*t));
		sortedinsert(&ht, t); /* t1 <- t -> t2 */
		printTreeHeap(&ht);
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
	while (ts->temp != tmstart) /* unlink and free temporary memory */
		tightS_poptemp(ts);

	/* build huffman codes */
	int nbits, code, y;
	memset(ts->codes, 0, sizeof(ts->codes));
	for (i = 0; i < TIGHTBYTES; i++) {
		if (combfreqs[i] == 0) /* skip 0 frequency bytes */
			continue;
		y = i; /* preserve 'i' */
		nbits = code = 0;
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
		t_tracef("[%d]=", i); tightD_printbits(ts->codes[i].code, nbits); t_trace("\n");
	}
}


TIGHT_API void tight_setfiles(tight_State *ts, int rfd, int wfd) {
	t_assert(rfd >= 0); t_assert(wfd >= 0); t_assert(wfd != rfd);
	if (ts->hufftree)
		tightT_freeparent(ts, ts->hufftree);
	memset(ts->codes, 0, sizeof(ts->codes));
	ts->rfd = rfd;
	ts->wfd = wfd;
}


/* remove/unlink first TempMem */
void tightS_poptemp(tight_State *ts) {
	t_assert(ts->temp != NULL);
	TempMem *tm = ts->temp;
	ts->temp = ts->temp->next;
	tightA_free(ts, tm, sizeof(*tm));
}


/* run protected function 'fn' */
int tightS_protectedcall(tight_State *ts, void *ud, fProtected fn) {
	Tightjmpbuf errjmp;
	t_assert(ts->errjmp == NULL);
	ts->errjmp = &errjmp;
	ts->status = TIGHT_OK;
	errjmp.haveap = 0;
	if (setjmp(errjmp.buf) == 0)
		fn(ts, ud);
	if (errjmp.haveap)
		va_end(errjmp.ap);
	ts->errjmp = NULL;
	return ts->status;
}


/* auxiliary to 'tightS_throw' */
static inline void freetempmem(tight_State *ts) {
	TempMem *curr = ts->temp;
	while (curr != NULL) {
		TempMem *tm = curr->next;
		tightA_freetempmem(ts, curr);
		curr = tm;
	}
	ts->temp = NULL;
}


/* set status code and jump to 'errjmp' */
t_noret tightS_throw(tight_State *ts, int errcode) {
	t_assert(ts->errjmp != NULL);
	freetempmem(ts);
	ts->status = errcode;
	longjmp(ts->errjmp->buf, 1);
}


TIGHT_API const char *tight_version(void) {
	return TIGHT_RELEASE;
}
