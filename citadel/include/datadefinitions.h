/**
 *
 * $Id$
 *
 * this file contains the defines that convert our x-macros to datatypes
 */
#define CFG_VALUE(a,b) a
#define SUBSTRUCT(a) a
#define SUBSTRUCT_ELEMENT(a)
#define PROTOCOL_ONLY(a)
#define SERVER_PRIVATE(a) a
#define NO_ARTV(a) a

#define LONG(a) long a
#define UNSIGNED(a) unsigned a
#define UNSIGNED_INT(a) unsigned int a
#define INTEGER(a) int a

#define UNSIGNED_SHORT(a) unsigned short a
#define UINT8(a) cit_uint8_t a
#define CHAR(a) char a

#define TIME(a) time_t a
#define UID_T(a) uid_t a

#define STRING_BUF(a, b) char a[b]
#define STRING(a) char *a
