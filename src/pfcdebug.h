#ifndef PFCDEBUG_H
#define PFCDEBUG_H

#include "pfc.h"

void pfcD_freealltrees(pfc_State *ps);
pfc_noret pfcD_error(pfc_State *pfc, const char *err);

#endif
