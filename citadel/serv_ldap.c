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
#include "tools.h"

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
	int i, j;
	char this_dn[SIZ];
	LDAPMod **attrs = NULL;
	int num_attrs = 0;

	char givenname[SIZ];
	char sn[SIZ];

	if (dirserver == NULL) return;
	if (msg == NULL) return;
	if (msg->cm_fields['M'] == NULL) return;
	if (msg->cm_fields['A'] == NULL) return;
	if (msg->cm_fields['N'] == NULL) return;

	/* Initialize variables */
	strcpy(givenname, "_");
	strcpy(sn, "_");

	sprintf(this_dn, "cn=%s,ou=%s,%s",
		msg->cm_fields['A'],
		msg->cm_fields['N'],
		config.c_ldap_base_dn
	);

	/* The first LDAP attribute will be an 'objectclass' list.  Citadel
	 * doesn't do anything with this.  It's just there for compatibility
	 * with Kolab.
	 */
	num_attrs = 1;
	attrs = mallok( (sizeof(LDAPMod *) * num_attrs) );
	attrs[0] = mallok(sizeof(LDAPMod));
	memset(attrs[0], 0, sizeof(LDAPMod));
	attrs[0]->mod_op	= LDAP_MOD_ADD;
	attrs[0]->mod_type	= "objectclass";
	attrs[0]->mod_values	= mallok(5 * sizeof(char *));
	attrs[0]->mod_values[0]	= strdoop("inetOrgPerson");
	attrs[0]->mod_values[1]	= strdoop("organizationalPerson");
	attrs[0]->mod_values[2]	= strdoop("person");
	attrs[0]->mod_values[3]	= strdoop("Top");
	attrs[0]->mod_values[4]	= NULL;

	/* Add a "cn" (Common Name) attribute based on the user's screen name */
	attrs = reallok(attrs, (sizeof(LDAPMod *) * ++num_attrs) );
	attrs[num_attrs-1] = mallok(sizeof(LDAPMod));
	memset(attrs[num_attrs-1], 0, sizeof(LDAPMod));
	attrs[num_attrs-1]->mod_op		= LDAP_MOD_ADD;
	attrs[num_attrs-1]->mod_type		= "cn";
	attrs[num_attrs-1]->mod_values		= mallok(2 * sizeof(char *));
	attrs[num_attrs-1]->mod_values[0]	= strdoop(msg->cm_fields['A']);
	attrs[num_attrs-1]->mod_values[1]	= NULL;
	
	/* Convert the vCard fields to LDAP properties */
	v = vcard_load(msg->cm_fields['M']);
	if (v->numprops) for (i=0; i<(v->numprops); ++i) {

		if (!strcasecmp(v->prop[i].name, "n")) {
			extract_token(sn,		v->prop[i].value, 0, ';');
			extract_token(givenname,	v->prop[i].value, 1, ';');
		}

	}
	vcard_free(v);	/* Don't need this anymore. */

	/* "sn" (surname) based on info in vCard */
	attrs = reallok(attrs, (sizeof(LDAPMod *) * ++num_attrs) );
	attrs[num_attrs-1] = mallok(sizeof(LDAPMod));
	memset(attrs[num_attrs-1], 0, sizeof(LDAPMod));
	attrs[num_attrs-1]->mod_op		= LDAP_MOD_ADD;
	attrs[num_attrs-1]->mod_type		= "sn";
	attrs[num_attrs-1]->mod_values		= mallok(2 * sizeof(char *));
	attrs[num_attrs-1]->mod_values[0]	= strdoop(sn);
	attrs[num_attrs-1]->mod_values[1]	= NULL;
	
	/* "givenname" (first name) based on info in vCard */
	attrs = reallok(attrs, (sizeof(LDAPMod *) * ++num_attrs) );
	attrs[num_attrs-1] = mallok(sizeof(LDAPMod));
	memset(attrs[num_attrs-1], 0, sizeof(LDAPMod));
	attrs[num_attrs-1]->mod_op		= LDAP_MOD_ADD;
	attrs[num_attrs-1]->mod_type		= "givenname";
	attrs[num_attrs-1]->mod_values		= mallok(2 * sizeof(char *));
	attrs[num_attrs-1]->mod_values[0]	= strdoop(givenname);
	attrs[num_attrs-1]->mod_values[1]	= NULL;

	/* The last attribute must be a NULL one. */
	attrs = realloc(attrs, (sizeof(LDAPMod *) * ++num_attrs) );
	attrs[num_attrs - 1] = NULL;
	
	lprintf(9, "this_dn: <%s>\n", this_dn);

	lprintf(9, "Calling ldap_add_s()\n");
	begin_critical_section(S_LDAP);
	i = ldap_add_s(dirserver, this_dn, attrs);
	end_critical_section(S_LDAP);

	/* If the entry already exists, repopulate it instead */
	if (i == LDAP_ALREADY_EXISTS) {
		for (j=0; j<(num_attrs-1); ++j) {
			attrs[j]->mod_op = LDAP_MOD_REPLACE;
		}
		lprintf(9, "Calling ldap_modify_s()\n");
		begin_critical_section(S_LDAP);
		i = ldap_modify_s(dirserver, this_dn, attrs);
		end_critical_section(S_LDAP);
	}

	if (i != LDAP_SUCCESS) {
		lprintf(3, "ldap_add_s() failed: %s (%d)\n",
			ldap_err2string(i), i);
	}

	lprintf(9, "Freeing attributes\n");
	/* Free the attributes */
	for (i=0; i<num_attrs; ++i) {
		if (attrs[i] != NULL) {

			/* First, free the value strings */
			if (attrs[i]->mod_values != NULL) {
				for (j=0; attrs[i]->mod_values[j] != NULL; ++j) {
					phree(attrs[i]->mod_values[j]);
				}
			}

			/* Free the value strings pointer list */	
			if (attrs[i]->mod_values != NULL) {
				phree(attrs[i]->mod_values);
			}

			/* Now free the LDAPMod struct itself. */
			phree(attrs[i]);
		}
	}
	phree(attrs);
	lprintf(9, "LDAP operation complete.\n");
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
