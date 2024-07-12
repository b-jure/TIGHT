#ifndef PFC_H
#define PFC_H

#include "pfcconf.h"


/* state */
typedef struct pfc_State pfc_State;


/* debug messages writer (often errors) */
typedef void (*pfc_MessageFn)(pfc_State *ps, const char *msg);


/* panic handler, invoked in errors (last function to be executed) */
typedef pfc_noret (*pfc_PanicFn)(pfc_State *ps);


/* memory (re)allocator */
typedef void *(*pfc_ReallocFn)(void *block, void *ud, size_t os, size_t ns);


/* file reader */
typedef const char *(*pfc_ReaderFn)(pfc_State *ps, void *userdata, size_t *szread);


#endif
