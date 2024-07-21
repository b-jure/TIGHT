#ifndef PFCCONF_H
#define PFCCONF_H


#include <stddef.h>
#include <stdint.h>
#include <limits.h>


/* signature for API functions */
#if !defined(PFC_API)
#define PFC_API					extern
#endif


/* buffer size when reading a file */
#if !defined(PFC_READBUFFER)
#define PFC_READBUFFER			65536
#endif


/* buffer size when encoding */
#if !defined(PFC_WRITEBUFFER)
#define PFC_WRITEBUFFER			PFC_READBUFFER
#endif


/* 
 * Maximum amount of memory to read into memory
 * when encoding unbuffered. 
 */
#if !defined(PFC_MAXUNBUFFERED)
#define PFC_MAXUNBUFFERED		1000000000   /* 1 GB */
#endif


#endif
