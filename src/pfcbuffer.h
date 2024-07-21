/*****************************************
 * Copyright (C) 2024 Jure B.
 * Refer to 'pfc.h' for license details.
 *****************************************/

#ifndef PFCBUFFER_H
#define PFCBUFFER_H


#include "pfc.h"
#include "pfcinternal.h"


#define pfcB_pop(b)		((b)->str[--(b)->len])
#define pfcB_last(b)	((b)->str[(b)->len - 1])


typedef struct Buffer {
	char *str;
	size_t len;
	size_t size;
} Buffer;


void pfcB_init(pfc_State *ps, Buffer *buf);
void pfcB_free(pfc_State *ps, Buffer *buf);
void pfcB_addint(pfc_State *ps, Buffer *buf, int i);
void pfcB_addsizet(pfc_State *ps, Buffer *buf, size_t n);
void pfcB_addstring(pfc_State *ps, Buffer *buf, const char *str, size_t len);
void pfcB_push(pfc_State *ps, Buffer *buf, unsigned char c);
char *pfcB_strdup(pfc_State *ps, const char *str);

#endif
