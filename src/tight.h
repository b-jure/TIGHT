/**********************************************************************************************
 * Copyright (C) 2024 Jure B.
 *
 * This file is part of tight.
 * tight is free software: you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * tight is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with tight.
 * If not, see <https://www.gnu.org/licenses/>.
 **********************************************************************************************/

#ifndef TIGHT_H
#define TIGHT_H

#include "tconf.h"

#ifdef __cplusplus
extern "C" {
#endif

/* project name */
#define TIGHT_NAME			"tight"

/* version */
#define TIGHT_VERSION_NUM		100
#define TIGHT_VERSION_MAJOR		"1"
#define TIGHT_VERSION_MINOR		"0"
#define TIGHT_VERSION_RELEASE	"0"
#define TIGHT_VERSION			TIGHT_VERSION_MAJOR "." TIGHT_VERSION_MINOR
#define TIGHT_RELEASE			TIGHT_VERSION "." TIGHT_VERSION_RELEASE

/* copyright */
#define TIGHT_COPYRIGHT \
	TIGHT_NAME " " TIGHT_RELEASE " Copyright (C) 2024 B. Jure"



/* tight state */
typedef struct tight_State tight_State;


/* memory allocator */
typedef void *(*tight_fRealloc)(void *block, void *ud, size_t os, size_t ns);



/* status codes */
#define TIGHT_ERRNO			(-1)	/* errno error */
#define TIGHT_OK			0		/* ok */
#define TIGHT_ERRCOMP		1		/* compression error */
#define TIGHT_ERRDECOMP		2 		/* decompression error */
#define TIGHT_ERRHEAD  		3 		/* header error */
#define TIGHT_ERRMEM		4 		/* memory error */
#define TIGHT_ERRVER		5 		/* version error */
#define TIGHT_ERRLIMIT		6		/* internal limit error */


/* modes for compression */
#define TIGHT_NONE			0
#define TIGHT_HUFFMAN		1		/* compress with huffman codes */
#define TIGHT_RLE			2		/* TODO(jure): implement */
#define TIGHT_DEFAULT		(TIGHT_HUFFMAN | TIGHT_RLE)



/*
 * Get current version string (semantic versioning).
 */
TIGHT_API const char *tight_version(void);


/* 
 * Create new state with its own allocator.
 * 'frealloc' must be provided, 'userdata' is optional.
 * In case state allocation fails this function returns NULL.
 */
TIGHT_API tight_State *tight_new(tight_fRealloc frealloc, void *userdata);


/*
 * Free all of the dynamically allocated memory anchored to state,
 * including the state itself.
 * Any of the file descriptors set with 'tight_setfiles' won't get
 * closed, it is upon user to manage provided file descriptors.
 */
TIGHT_API void tight_free(tight_State *ts);


/* 
 * Set input (read) and output (write) file descriptors.
 * This can't fail but it asserts that file descriptors are
 * valid (>=0) and are not equal.
 */
TIGHT_API void tight_setfiles(tight_State *ts, int rfd, int wfd);


/*
 * Compress previously set 'rfd' into 'wfd'.
 * Compression algorithms and strategies being used correspond to 'mode' bitmask.
 * 'freqs' is table of symbol frequencies (8-bit ASCII), this is only
 * used when 'mode' contains 'TIGHT_HUFFMAN', in case it is omitted (NULL) 
 * while 'mode' bits expect 'freqs' to be valid, internal frequency table 
 * is used, omitting this might result in suboptimal compression ratio.
 * Upon completion returns one of the status codes and removes previously set
 * file descriptors from 'tight_State'.
 * If no errors occurred, file offset for 'rfd' will be at the end of the file.
 * In case of errors file offset is not specified.
 */
TIGHT_API int tight_compress(tight_State *ts, int mode, const size_t *freqs);


/*
 * Decompress previously set 'rfd' into 'wfd'.
 * Decompressing does not require prior knowledge on how the input file 
 * is encoded, all of the information required for correct decompression is
 * contained inside of the compressed file.
 * Upon completion returns one of the status codes and removes previously
 * set file descriptors from 'tight_State'.
 * If no errors occurred, file offset for 'rfd' will be at the end of the file.
 * In case of errors file offset is not specified.
 */
TIGHT_API int tight_decompress(tight_State *ts);


/*
 * Get message describing the latest error.
 * Returns NULL if no error occurred.
 * Lifetime of error message is valid up until the occurrence of new error.
 * In case of memory errors, error message lifetime is static.
 */
TIGHT_API const char *tight_geterror(tight_State *ts);


#ifdef __cplusplus
}
#endif

#endif
