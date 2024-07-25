/*****************************************
 * Copyright (C) 2024 Jure B.
 * Refer to 'pfc.h' for license details.
 *****************************************/

#ifndef TIGHTBUFFER_H
#define TIGHTBUFFER_H

#include <stdio.h>

#include "tight.h"
#include "tinternal.h"


/* end of file */
#if !defined(EOF)
#define TIGHTEOF		(-1)
#else
#define TIGHTEOF		EOF
#endif


#define tightB_pop(b)		((b)->str[--(b)->len])
#define tightB_last(b)		((b)->str[(b)->len - 1])


/* string buffer */
typedef struct Buffer {
	char *str;
	uint len;
	uint size;
} Buffer;


TIGHT_FUNC void tightB_init(tight_State *ts, Buffer *buf);
TIGHT_FUNC void tightB_free(tight_State *ts, Buffer *buf);
TIGHT_FUNC void tightB_addint(tight_State *ts, Buffer *buf, int i);
TIGHT_FUNC void tightB_adduint(tight_State *ts, Buffer *buf, uint u);
TIGHT_FUNC void tightB_push(tight_State *ts, Buffer *buf, unsigned char c);
TIGHT_FUNC void tightB_addstring(tight_State *ts, Buffer *buf, const char *str,
								 size_t len);


/* buffered reader */
typedef struct BuffReader {
	tight_State *ts;
	byte *current; /* current position in 'rbuf' */
	byte buf[TIGHT_RBUFFSIZE]; /* read buffer */
	uint n; /* chars left to read in 'rbuf' */
} BuffReader;


TIGHT_FUNC int tightB_brgetc(BuffReader *br);
TIGHT_FUNC int tightB_brfill(BuffReader *br, long *n);

TIGHT_FUNC char *tightB_strdup(tight_State *ts, const char *str);

#endif
