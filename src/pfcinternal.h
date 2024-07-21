#ifndef PFCINTERNAL_H
#define PFCINTERNAL_H


/* signature for functions that do not return */
#if defined(__GNUC__)
#define pfc_noret			__attribute__((noreturn)) void
#else
#define pfc_noret			void
#endif


/* branch prediction */
#if defined(__GNUC__)
#define pfc_likely(e)		__builtin_expect((e) != 0, 1)
#define pfc_unlikely(e)		__builtin_expect((e) != 0, 0)
#endif


/* internal assertions */
#if defined(PFC_ASSERT)
#undef NDBG
#include <assert.h>
#define pfc_assert(e)		assert(e)
#else
#define pfc_assert(e)		((void)0)
#endif


/* 
 * Maximum amount of values that fit in an
 * unsigned byte.
 */
#if !defined(PFC_BYTES)
#define PFC_BYTES			(UCHAR_MAX + 1)
#endif


/* math abs */
#if !defined(p_abs)
#define p_abs(x)	((x) >= 0 ? (x) : -(x))
#endif


/* typedefs */
typedef unsigned char		byte;
typedef unsigned short		ushrt;

#endif
