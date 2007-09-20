/**
 *
 * $Id$
 *
 * this file contains the defines that cause our x-macros to serialize for artv export
 */
#define CFG_VALUE(a,b) a
#define PROTOCOL_ONLY(a)
#define SERVER_PRIVATE(a) a
#define NO_ARTV(a)
#define SUBSTRUCT(a)
#define SUBSTRUCT_ELEMENT(a) a

#define UNSIGNED(a) client_getln(cbuf, sizeof cbuf); buf->a = atoi(cbuf)
#define UNSIGNED_INT(a) client_getln(cbuf, sizeof cbuf); buf->a = atoi(cbuf)
#define INTEGER(a) client_getln(cbuf, sizeof cbuf); buf->a = atoi(cbuf)
#define LONG(a) client_getln(cbuf, sizeof cbuf); buf->a = atol(cbuf)
#define UINT8(a) client_getln(cbuf, sizeof cbuf); buf->a = atoi(cbuf)
#define UNSIGNED_SHORT(a) client_getln(cbuf, sizeof cbuf); buf->a = atoi(cbuf)
#define CHAR(a) client_getln(cbuf, sizeof cbuf); buf->a = atoi(cbuf)

#define TIME(a) client_getln(cbuf, sizeof cbuf); buf->a = atol(cbuf)
#define UID_T(a) client_getln(cbuf, sizeof cbuf); buf->a = atol(cbuf)

#define STRING_BUF(a, b) client_getln(buf->a, b)
#define STRING(a) client_getln(buf->a, sizeof buf->a)
