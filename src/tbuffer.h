/*****************************************
 * Copyright (C) 2024 Jure B.
 * Refer to 'pfc.h' for license details.
 *****************************************/

#ifndef TIGHTBUFFER_H
#define TIGHTBUFFER_H

#include <stdio.h>
#include <sys/types.h>

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



/* size of 'tmpbuf' in 'BuffWriter' and 'BuffReader' */
#define TMPBsize		MAXCODE


/* get next char */
#define tightB_brgetc(br) \
	((br)->n-- > 0 ? *(br)->current++ : tightB_brfill(br, NULL))


/* buffered reader */
typedef struct BuffReader {
	tight_State *ts;
	byte *current; /* current position in 'rbuf' */
	byte buf[TIGHT_RBUFFSIZE]; /* read buffer */
	ssize_t n; /* chars left to read in 'rbuf' */
	int validbits; /* valid bits in 'tmpbuf' */
	ushrt tmpbuf; /* temporary bits buffer */
	int fd; /* file descriptor */
} BuffReader;


TIGHT_FUNC void tightB_initbr(BuffReader *br, tight_State *ts, int fd);
TIGHT_FUNC int tightB_brfill(BuffReader *br, ulong *n);
TIGHT_FUNC byte tightB_readnbits(BuffReader *br, int n);
TIGHT_FUNC int tightB_readpending(BuffReader *br, int *out);
TIGHT_FUNC void tightB_genMD5(BuffReader *br, ulong size, byte *out);



/* buffered writer */
typedef struct BuffWriter {
	tight_State *ts; /* state */
	uint len; /* number of elements in 'buf' */
	byte buf[TIGHT_WBUFFSIZE]; /* write buffer */
	int validbits; /* valid bits in 'tmpbuf' */
	ushrt tmpbuf; /* temporary bits buffer */
	int fd; /* file descriptor */
} BuffWriter;


TIGHT_FUNC void tightB_initbw(BuffWriter *bw, tight_State *ts, int fd);
TIGHT_FUNC void tightB_writefile(BuffWriter *bw);
TIGHT_FUNC void tightB_writebyte(BuffWriter *bw, byte byte);
TIGHT_FUNC void tightB_writeshort(BuffWriter *bw, ushrt shrt);
TIGHT_FUNC void tightB_writenbits(BuffWriter *bw, int code, int len);
TIGHT_FUNC void tightB_writepending(BuffWriter *bw);

/* misc func */
TIGHT_FUNC char *tightB_strdup(tight_State *ts, const char *str);

#endif
