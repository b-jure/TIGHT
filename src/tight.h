#ifndef TIGHT_H
#define TIGHT_H

#include "tconf.h"


/* project name */
#define TIGHT_NAME			"tight"

/* version and copyright information */
#define TIGHT_VERSION_MAJOR				"1"
#define TIGHT_VERSION_MINOR				"0"
#define TIGHT_VERSION_RELEASE			"0"

#define TIGHT_VERSION_NUM				100
#define TIGHT_VERSION_RELEASE_NUM		(TIGHT_VERSION_NUM * 100)

#define TIGHT_VERSION			TIGHT_NAME " " TIGHT_VERSION_MAJOR "." TIGHT_VERSION_MINOR
#define TIGHT_RELEASE			TIGHT_VERSION "." TIGHT_VERSION_RELEASE
#define TIGHT_AUTHOR			"B. Jure"
#define TIGHT_COPYRIGHT			TIGHT_RELEASE " Copyright (C) 2024 " TIGHT_AUTHOR


/* state */
typedef struct tight_State tight_State;


/* debug messages writer (most often errors) */
typedef void (*tight_fError)(const char *msg);

/* panic handler, invoked in errors (last function to be executed) */
typedef void (*tight_fPanic)(tight_State *ts);

/* memory (re)allocator */
typedef void *(*tight_fRealloc)(void *block, void *ud, size_t os, size_t ns);


/* create/free state */
TIGHT_API tight_State *tight_new(tight_fRealloc frealloc, void *userdata);
TIGHT_API void tight_free(tight_State *ts);

/* set hooks */
TIGHT_API tight_fError tight_seterror(tight_State *ts, tight_fError fmsg);
TIGHT_API tight_fPanic tight_setpanic(tight_State *ts, tight_fPanic fpanic);

/* set input and output file descriptors */
TIGHT_API void tight_setfiles(tight_State *ts, int rfd, int wfd, const size_t *freqs);

/* encode/decode input file and write results into output file */
TIGHT_API void tight_encode(tight_State *ts);
TIGHT_API void tight_decode(tight_State *ts);

#endif
