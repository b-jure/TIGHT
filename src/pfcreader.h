#ifndef PFCREADER_H
#define PFCREADER_H

#include "pfc.h"


#define PFCEOF		(-1)


typedef struct Reader {
    size_t n; /* unread bytes */
    const char* buff; /* position in buffer */
    pfc_ReaderFn reader; /* reader function */
    void* userdata; /* user data for 'pfc_ReaderFn' */
    pfc_State* ps; /* 'pfc_State' for 'pfc_ReaderFn' */
} Reader;


void pfcR_init(pfc_State* ps, Reader* r, pfc_ReaderFn fn, void* ud);
int pfcR_fill(Reader* r);
size_t pfcR_readn(Reader* r, size_t n);

#endif
