#ifndef TIGHTDEBUG_H
#define TIGHTDEBUG_H

#include "tight.h"
#include "tinternal.h"
#include "tstate.h"
#include "ttree.h"


/* string memory errors */
#define TIGHT_MEMERROR	"out of memory"


TIGHT_FUNC t_noret tightD_compresserror(tight_State *ts, const char *desc);
TIGHT_FUNC t_noret tightD_decompresserror(tight_State *ts, const char *desc);
TIGHT_FUNC t_noret tightD_headererror(tight_State *ts, const char *extra);
TIGHT_FUNC t_noret tightD_errnoerror(tight_State *ts, const char *fn);
TIGHT_FUNC t_noret tightD_limiterror(tight_State *ts, const char *what, size_t limit);
TIGHT_FUNC t_noret tightD_versionerror(tight_State *ts, byte major);


#if defined(TIGHT_TRACE)
TIGHT_FUNC void tightD_printbits(int code, int nbits);
TIGHT_FUNC void tightD_printtree(const TreeData *root);
TIGHT_FUNC void tightD_printchecksum(byte *checksum, size_t size);
#else
#define tightD_printbits(c, nb)			((void)0)
#define tightD_printtree(r)				((void)0)
#define tightD_printchecksum(s,l)		((void)0)
#endif

#endif /* TIGHTDEBUG_H */
