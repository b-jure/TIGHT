#ifndef PFC_H
#define PFC_H

#include "pfcconf.h"


/* state */
typedef struct pfc_State pfc_State;


/* debug messages writer (often errors) */
typedef void (*pfc_fError)(const char *msg);

/* panic handler, invoked in errors (last function to be executed) */
typedef void (*pfc_fPanic)(pfc_State *ps);

/* memory (re)allocator */
typedef void *(*pfc_fRealloc)(void *block, void *ud, size_t os, size_t ns);


/* create/free state */
PFC_API pfc_State *pfc_new(pfc_fRealloc frealloc, void *userdata);
PFC_API void pfc_free(pfc_State *ps);

/* set hooks */
PFC_API pfc_fError pfc_seterror(pfc_State *ps, pfc_fError fmsg);
PFC_API pfc_fPanic pfc_setpanic(pfc_State *ps, pfc_fPanic fpanic);

#endif
