/*
 * $Id$
 *
 */

#ifdef HAVE_LDAP

void ctdl_vcard_to_ldap(struct CtdlMessage *msg, int op);

enum {
	V2L_WRITE,
	V2L_DELETE
};

#endif /* HAVE_LDAP */
