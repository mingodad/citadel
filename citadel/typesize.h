/* $Id$ */

/*
   This file defines macros for 8, 16, and 32 bit integers.  The macros are:
   cit_int8_t	default 8-bit int
   cit_int16_t	default 16-bit int
   cit_int32_t	default 32-bit int
   cit_int64_t	default 64-bit int (not implemented yet)
   cit_sint8_t	signed 8-bit int
   cit_sint16_t	signed 16-bit int
   cit_sint32_t	signed 32-bit int
   cit_sint64_t	signed 64-bit int (not implemented yet)
   cit_uint8_t	unsigned 8-bit int
   cit_uint16_t	unsigned 16-bit int
   cit_uint32_t	unsigned 32-bit int
   cit_uint64_t	unsigned 64-bit int (not implemented yet)

   The sizes are determined during the configure process; see the 
   AC_CHECK_SIZEOF macros in configure.in.  In no way do we assume that any
   given datatype is any particular width, e.g. we don't assume char is one
   byte; we check for it specifically.

   This might seem excessively paranoid, but I've seen some WEIRD systems
   and some bizarre compilers (Domain/OS for instance) in my time.
*/

#ifndef _CITADEL_UX_TYPESIZE_H
#define _CITADEL_UX_TYPESIZE_H

/* Include sysdep.h if not already included */
#ifndef BBSDIR
# include "sysdep.h"
#endif

/* 8-bit */
#if SIZEOF_CHAR == 1
# define cit_int8_t	char
#elif SIZEOF_SHORT == 1
# define cit_int8_t	short
#else
# error Unable to find an 8-bit integer datatype
#endif

/* 16-bit */
#if SIZEOF_SHORT == 2
# define cit_int16_t	short
#elif SIZEOF_INT == 2
# define cit_int16_t	int
#elif SIZEOF_CHAR == 2
# define cit_int16_t	char
#else
# error Unable to find a 16-bit integer datatype
#endif

/* 32-bit */
#if SIZEOF_INT == 4
# define cit_int32_t	int
#elif SIZEOF_SHORT == 4
# define cit_int32_t	short
#elif SIZEOF_LONG == 4
# define cit_int32_t	long
#else
# error Unable to find a 32-bit integer datatype
#endif

/* signed */
#define cit_sint8_t	signed cit_int8_t
#define cit_sint16_t	signed cit_int16_t
#define cit_sint32_t	signed cit_int32_t

/* unsigned */
#define cit_uint8_t	unsigned cit_int8_t
#define cit_uint16_t	unsigned cit_int16_t
#define cit_uint32_t	unsigned cit_int32_t

#endif /* _CITADEL_UX_TYPESIZE_H */
