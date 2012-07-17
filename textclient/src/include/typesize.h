
/*
   This file defines typedefs for 8, 16, and 32 bit integers.  They are:
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
   given datatype is any particular width, e.g. we don't assume short is two
   bytes; we check for it specifically.

   This might seem excessively paranoid, but I've seen some WEIRD systems
   and some bizarre compilers (Domain/OS for instance) in my time.
*/

#ifndef _CITADEL_UX_TYPESIZE_H
#define _CITADEL_UX_TYPESIZE_H

/* Include sysdep.h if not already included */
#ifndef CTDLDIR
# include "sysdep.h"
#endif

/* 8-bit - If this fails, your compiler is broken */
#if SIZEOF_CHAR == 1
typedef char cit_int8_t;
typedef signed char cit_sint8_t;
typedef unsigned char cit_uint8_t;
#else
# error Unable to find an 8-bit integer datatype
#endif

/* 16-bit - If this fails, your compiler is broken */
#if SIZEOF_SHORT == 2
typedef short cit_int16_t;
typedef signed short cit_sint16_t;
typedef unsigned short cit_uint16_t;
#elif SIZEOF_INT == 2
typedef int cit_int16_t;
typedef signed int cit_sint16_t;
typedef unsigned int cit_uint16_t;
#else
# error Unable to find a 16-bit integer datatype
#endif

/* 32-bit - If this fails, your compiler is broken */
#if SIZEOF_INT == 4
typedef int cit_int32_t;
typedef signed int cit_sint32_t;
typedef unsigned int cit_uint32_t;
#elif SIZEOF_LONG == 4
typedef long cit_int32_t;
typedef signed long cit_sint32_t;
typedef unsigned long cit_uint32_t;
#else
# error Unable to find a 32-bit integer datatype
#endif

#endif /* _CITADEL_UX_TYPESIZE_H */
