#ifndef PFCDEBUG_H
#define PFCDEBUG_H

#include "pfc.h"
#include "pfcinternal.h"

pfc_noret pfcD_error(pfc_State *pfc, const char *fmt, ...);
pfc_noret pfcD_memerror(pfc_State *ps);

#endif
