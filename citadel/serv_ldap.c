/*
 * $Id$
 *
 * A module which implements the LDAP connector for Citadel.
 *
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <pwd.h>
#include <errno.h>
#include <sys/types.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <sys/wait.h>
#include <string.h>
#include <limits.h>
#include "citadel.h"
#include "server.h"
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "serv_extensions.h"
#include "room_ops.h"
#include "policy.h"
#include "database.h"
#include "msgbase.h"
#include "serv_ldap.h"
#include "vcard.h"

#ifdef HAVE_LDAP

#include <ldap.h>

LDAP *dirserver = NULL;

/*
 * LDAP connector cleanup function
 */
void serv_ldap_cleanup(void)
{
	if (!dirserver) return;

	lprintf(7, "Unbinding from directory server\n");
	ldap_unbind(dirserver);
	dirserver = NULL;
}

#endif				/* HAVE_LDAP */


void CtdlConnectToLdap(void) {
	int i;
	int ldap_version = 3;

	lprintf(7, "Connecting to LDAP server %s:%d...\n",
		config.c_ldap_host, config.c_ldap_port);

	dirserver = ldap_init(config.c_ldap_host, config.c_ldap_port);
	if (dirserver == NULL) {
		lprintf(3, "Could not connect to %s:%d : %s\n",
			config.c_ldap_host,
			config.c_ldap_port,
			strerror(errno));
		return;
	}

	ldap_set_option(dirserver, LDAP_OPT_PROTOCOL_VERSION, &ldap_version);

	lprintf(7, "Binding to %s\n", config.c_ldap_bind_dn);

	i = ldap_simple_bind_s(dirserver,
				config.c_ldap_bind_dn,
				config.c_ldap_bind_pw
	);
	if (i != LDAP_SUCCESS) {
		lprintf(3, "Cannot bind: %s (%d)\n", ldap_err2string(i), i);
		dirserver = NULL;	/* FIXME disconnect from ldap */
	}
}




/*
 * Write (add, or change if already exists) a directory entry to the
 * LDAP server, based on the information supplied in a vCard.
 */
void ctdl_vcard_to_ldap(struct CtdlMessage *msg) {
	struct vCard *v = NULL;

	char this_dn[SIZ];

	if (msg == NULL) return;
	if (msg->cm_fields['M'] == NULL) return;
	if (msg->cm_fields['A'] == NULL) return;
	if (msg->cm_fields['N'] == NULL) return;

	sprintf(this_dn, "cn=%s,ou=%s,%s",
		msg->cm_fields['A'],
		msg->cm_fields['N'],
		config.c_ldap_base_dn
	);

	lprintf(9, "this_dn: <%s>\n", this_dn);

	v = vcard_load(msg->cm_fields['M']);

	vcard_free(v);
}




/*
 * Initialize the LDAP connector module ... or don't, if we don't have LDAP.
 */
char *serv_ldap_init(void)
{
#ifdef HAVE_LDAP
	CtdlRegisterCleanupHook(serv_ldap_cleanup);

	if (strlen(config.c_ldap_host) > 0) {
		CtdlConnectToLdap();
	}

#endif				/* HAVE_LDAP */
	return "$Id$";
}
