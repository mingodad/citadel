/**
 * this file contains the definitions for the floors transmitted via the citadel protocol.
 */

SERVER_PRIVATE(UNSIGNED_SHORT(f_flags)); /* flags */
PROTOCOL_ONLY(INTEGER(id)); /* the floor id. its implicit in its location in the file, but here once more for the protocol. */
STRING_BUF(f_name, 256); /* our name. */
INTEGER(f_ref_count); /* how many sub elements do we contain? */

SUBSTRUCT(struct ExpirePolicy f_ep);	/* Message expiration policy        */
SUBSTRUCT_ELEMENT(INTEGER(f_ep.expire_mode));
SUBSTRUCT_ELEMENT(INTEGER(f_ep.expire_value));
