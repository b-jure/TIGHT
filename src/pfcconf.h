#ifndef PFCCONF_H
#define PFCCONF_H


#include <stddef.h>
#include <stdint.h>


/* signature for functions that do not return */
#if defined(__GNUC__)
#define pfc_noret		__attribute__((noreturn)) void
#else
#define pfc_noret		void
#endif


/* branch prediction */
#if defined(__GNUC__)
#define pfc_likely(e)		__builtin_expect((e) != 0, 1)
#define pfc_unlikely(e)		__builtin_expect((e) != 0, 0)
#endif


#if defined(PFC_ASSERT)
#undef NDBG
#include <assert.h>
#define pfc_assert(e)		assert(e)
#else
#define pfc_assert(e)		((void)0)
#endif


/* shorter typedef */
typedef unsigned char p_byte;


#endif
