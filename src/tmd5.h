/*****************************************
 * Copyright (C) 2024 Jure B.
 * Refer to 'tight.h' for license details.
 *****************************************/

#ifndef TIGHTMD5_H
#define TIGHTMD5_H

#include <stdio.h>

#include "tight.h"
#include "tinternal.h"



/* maximum 'word' value */
#define MAXWORD		UINT32_MAX

/* 'word' width */
#define WORDWIDTH	(sizeof(uint32_t) * 8)


/* Per RFC a word is 32-bit quantity */
typedef uint32_t word;

/* double word used for bit count */
typedef word dword[2];


/* MD5 context */
typedef struct MD5ctx{
  word state[4]; /* state (ABCD) */
  dword count; /* number of bits, modulo 2^64 */
  byte buffer[64]; /* input buffer */
} MD5ctx;


TIGHT_FUNC void tight5_init(MD5ctx *ctx);
TIGHT_FUNC void tight5_update(MD5ctx *ctx, byte *b, unsigned int len);
TIGHT_FUNC void tight5_final(MD5ctx *ctx, byte out[16]);

#endif
