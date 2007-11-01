/**
 *
 * $Id: artv_serialize.h 5474 2007-09-20 04:11:52Z ajc $
 *
 * this file contains the defines that cause our x-macros to serialize for artv export
 */

#define CFG_VALUE(a,b) a
#define NO_ARTV(a)
#define PROTOCOL_ONLY(a)
#define SERVER_PRIVATE(a) a
#define SUBSTRUCT(a)
#define SUBSTRUCT_ELEMENT(a) a

#define UNSIGNED(a) cprintf(" %s: %u\n", #a, buf->a)
#define UNSIGNED_INT(a) cprintf(" %s: %u\n", #a buf->a)
#define INTEGER(a) cprintf(" %s: %d\n", #a, buf->a)
#define LONG(a) cprintf(" %s: %ld\n", #a, buf->a)
#define UINT8(a) cprintf(" %s: %d\n", #a, buf->a)
#define UNSIGNED_SHORT(a) cprintf(" %s: %u\n", #a, buf->a)
#define CHAR(a) cprintf(" %s: %d\n", #a, buf->a)

#define TIME(a) cprintf(" %s: %ld\n", #a, (long)buf->a)
#define UID_T(a) cprintf(" %s: %ld\n", #a, (long)buf->a)

#define STRING_BUF(a, b) cprintf(" %s: %s\n", #a, buf->a)
#define STRING(a) cprintf(" %s: %s\n", #a, buf->a)
