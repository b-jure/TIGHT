#ifndef TIGHTCONF_H
#define TIGHTCONF_H


#include <stddef.h>
#include <stdint.h>
#include <limits.h>


/* signature for API functions */
#if !defined(TIGHT_API)
#define TIGHT_API					extern
#endif


/* buffer size when reading a file */
#if !defined(TIGHT_RBUFFSIZE)
#define TIGHT_RBUFFSIZE				65536	/* 64K */
#endif


/* buffer size when encoding; reading from file stream. */
#if !defined(TIGHT_WBUFFSIZE)
#define TIGHT_WBUFFSIZE				TIGHT_RBUFFSIZE
#endif


#endif