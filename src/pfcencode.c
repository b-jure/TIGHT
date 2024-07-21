#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pfcdebug.h"
#include "pfcinternal.h"


/* end of file */
#if !defined(EOF)
#define CFEOF		(-1)
#else
#define CFEOF		EOF
#endif



/* buffered writer */
typedef struct BuffWriter {
	pfc_State *ps;
	size_t len; /* position in 'buf' */
	byte buf[PFC_WRITEBUFFER]; /* write buffer */
} BuffWriter;



/* buffered reader */
typedef struct BuffReader {
	FILE *fp; /* file stream */
	pfc_State *ps;
	size_t n; /* chars left to read in 'rbuf' */
	byte *current; /* current position in 'rbuf' */
	byte buf[PFC_READBUFFER]; /* read buffer */
} BuffReader;



static int brfill(BuffReader *br) {
	pfc_assert(br->n == 0 && br->current != NULL);
	br->n = fread(br->buf, 1, sizeof(br->buf), br->fp);
	if (feof(br->fp))
		return (br->current = NULL, CFEOF);
	return (br->current = br->buf, *br->current++);
}


static inline int brgetc(BuffReader *br) {
	if (br->n > 0)
		return *br->current++;
	else if (br->current == NULL)
		return CFEOF;
	return brfill(br);
}


/* 
 * Encode 'filename'; in case it is a directory then
 * traverse it recursively encoding every regular file
 * and ignoring symlinks and any other special files.
 */
void pfcE_encode(pfc_State *ps, const char *filename) {
	BuffReader br = { .ps = ps };

	br.fp = fopen(filename, "r");
	if (pfc_unlikely(br.fp < 0))
		pfcD_error(ps, "error while opening '%s': %s", filename, strerror(errno));
	// TODO: how will we count, maybe providing external hook?
}
