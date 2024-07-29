#ifndef TIGHTINTERNAL_H
#define TIGHTINTERNAL_H


/* signature for functions that do not return */
#if defined(__GNUC__)
#define t_noret				__attribute__((noreturn)) void
#else
#define t_noret				void
#endif


/* do not conform to ABI in favor of optimizations for non-API functions */
#if defined(__GNUC__) && ((__GNUC__ * 100 + __GNUC_MINOR__) >= 302) && defined(__ELF__)
#define TIGHT_FUNC		__attribute__((visibility("internal"))) extern
#else
#define TIGHT_FUNC		extern
#endif


/* branch prediction */
#if defined(__GNUC__)
#define t_likely(e)			__builtin_expect((e) != 0, 1)
#define t_unlikely(e)		__builtin_expect((e) != 0, 0)
#endif


/* enables internal assertions */
#if defined(TIGHT_ASSERT)
#undef NDBG
#include <assert.h>
#define t_assert(e)		assert(e)
#else
#define t_assert(e)		((void)0)
#endif


/* 
 * Maximum amount of values that fit in an
 * unsigned byte.
 */
#if !defined(TIGHTBYTES)
#define TIGHTBYTES				(UCHAR_MAX + 1)
#endif


/* size of table for codes */
#if !defined(TIGHTCODES)
#define TIGHTCODES				(TIGHTBYTES * 2)
#endif


/* math abs */
#if !defined(t_abs)
#define t_abs(x)	((x) >= 0 ? (x) : -(x))
#endif


/* typedefs */
typedef unsigned char		byte;
typedef unsigned short		ushrt;
typedef unsigned int		uint;
typedef unsigned long		ulong;

#endif
