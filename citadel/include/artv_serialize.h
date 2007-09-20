/**
 *
 * $Id$
 *
 * this file contains the defines that cause our x-macros to serialize for artv export
 */

#define CFG_VALUE(a,b) a
#define NO_ARTV(a)
#define PROTOCOL_ONLY(a)
#define SERVER_PRIVATE(a) a
#define SUBSTRUCT(a)
#define SUBSTRUCT_ELEMENT(a) a

#define UNSIGNED(a) cprintf("%u\n", buf->a)
#define UNSIGNED_INT(a) cprintf("%u\n", buf->a)
#define INTEGER(a) cprintf("%d\n", buf->a)
#define LONG(a) cprintf("%ld\n", buf->a)
#define UINT8(a) cprintf("%d\n", buf->a)
#define UNSIGNED_SHORT(a) cprintf("%u\n", buf->a)
#define CHAR(a) cprintf("%d\n", buf->a)

#define TIME(a) cprintf("%ld\n", (long)buf->a)
#define UID_T(a) cprintf("%ld\n", (long)buf->a)

#define STRING_BUF(a, b) cprintf("%s\n", buf->a)
#define STRING(a) cprintf("%s\n", buf->a)
