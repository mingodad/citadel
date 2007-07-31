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
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "room_ops.h"
#include "policy.h"
#include "database.h"
#include "msgbase.h"
#include "serv_ldap.h"
#include "vcard.h"
#include "tools.h"


#include "ctdl_module.h"



#ifdef HAVE_LDAP

#include <ldap.h>

LDAP *dirserver = NULL;

/*
 * LDAP connector cleanup function
 */
void serv_ldap_cleanup(void)
{
	if (!dirserver) return;

	lprintf(CTDL_INFO, "Unbinding from directory server\n");
	ldap_unbind(dirserver);
	dirserver = NULL;
}



/*
 * Create the root node.  If it's already there, so what?
 */
void CtdlCreateLdapRoot(void) {
	char *dc_values[2];
	char *objectClass_values[3];
	LDAPMod dc, objectClass;
	LDAPMod *mods[3];
	char topdc[SIZ];
	int i;

	/* We just want the top-level dc, not the whole hierarchy */
	strcpy(topdc, config.c_ldap_base_dn);
	for (i=0; i<strlen(topdc); ++i) {
		if (topdc[i] == ',') topdc[i] = 0;
	}
	for (i=0; i<strlen(topdc); ++i) {
		if (topdc[i] == '=') strcpy(topdc, &topdc[i+1]);
	}

	/* Set up the transaction */
	dc.mod_op		= LDAP_MOD_ADD;
	dc.mod_type		= "dc";
	dc_values[0]		= topdc;
	dc_values[1]		= NULL;
	dc.mod_values		= dc_values;
	objectClass.mod_op	= LDAP_MOD_ADD;
	objectClass.mod_type	= "objectClass";
	objectClass_values[0]	= "top";
	objectClass_values[1]	= "domain";
	objectClass_values[2]	= NULL;
	objectClass.mod_values	= objectClass_values;
	mods[0] = &dc;
	mods[1] = &objectClass;
	mods[2] = NULL;

	/* Perform the transaction */
	lprintf(CTDL_DEBUG, "Setting up Base DN node...\n");
	begin_critical_section(S_LDAP);
	i = ldap_add_s(dirserver, config.c_ldap_base_dn, mods);
	end_critical_section(S_LDAP);

	if (i == LDAP_ALREADY_EXISTS) {
		lprintf(CTDL_INFO, "Base DN is already present in the directory; no need to add it again.\n");
	}
	else if (i != LDAP_SUCCESS) {
		lprintf(CTDL_CRIT, "ldap_add_s() failed: %s (%d)\n",
			ldap_err2string(i), i);
	}
}


/*
 * Create an OU node representing a Citadel host.
 */
void CtdlCreateHostOU(char *host) {
	char *dc_values[2];
	char *objectClass_values[3];
	LDAPMod dc, objectClass;
	LDAPMod *mods[3];
	int i;
	char dn[SIZ];

	/* The DN is this OU plus the base. */
	snprintf(dn, sizeof dn, "ou=%s,%s", host, config.c_ldap_base_dn);

	/* Set up the transaction */
	dc.mod_op		= LDAP_MOD_ADD;
	dc.mod_type		= "ou";
	dc_values[0]		= host;
	dc_values[1]		= NULL;
	dc.mod_values		= dc_values;
	objectClass.mod_op	= LDAP_MOD_ADD;
	objectClass.mod_type	= "objectClass";
	objectClass_values[0]	= "top";
	objectClass_values[1]	= "organizationalUnit";
	objectClass_values[2]	= NULL;
	objectClass.mod_values	= objectClass_values;
	mods[0] = &dc;
	mods[1] = &objectClass;
	mods[2] = NULL;

	/* Perform the transaction */
	lprintf(CTDL_DEBUG, "Setting up Host OU node...\n");
	begin_critical_section(S_LDAP);
	i = ldap_add_s(dirserver, dn, mods);
	end_critical_section(S_LDAP);

	if (i == LDAP_ALREADY_EXISTS) {
		lprintf(CTDL_INFO, "Host OU is already present in the directory; no need to add it again.\n");
	}
	else if (i != LDAP_SUCCESS) {
		lprintf(CTDL_CRIT, "ldap_add_s() failed: %s (%d)\n",
			ldap_err2string(i), i);
	}
}








void CtdlConnectToLdap(void) {
	int i;
	int ldap_version = 3;

	lprintf(CTDL_INFO, "Connecting to LDAP server %s:%d...\n",
		config.c_ldap_host, config.c_ldap_port);

	dirserver = ldap_init(config.c_ldap_host, config.c_ldap_port);
	if (dirserver == NULL) {
		lprintf(CTDL_CRIT, "Could not connect to %s:%d : %s\n",
			config.c_ldap_host,
			config.c_ldap_port,
			strerror(errno));
		return;
	}

	ldap_set_option(dirserver, LDAP_OPT_PROTOCOL_VERSION, &ldap_version);

	lprintf(CTDL_INFO, "Binding to %s\n", config.c_ldap_bind_dn);

	i = ldap_simple_bind_s(dirserver,
				config.c_ldap_bind_dn,
				config.c_ldap_bind_pw
	);
	if (i != LDAP_SUCCESS) {
		lprintf(CTDL_CRIT, "Cannot bind: %s (%d)\n", ldap_err2string(i), i);
		dirserver = NULL;	/* FIXME disconnect from ldap */
		return;
	}

	CtdlCreateLdapRoot();
}


/* 
 * vCard-to-LDAP conversions.
 *
 * If 'op' is set to V2L_WRITE, then write
 * (add, or change if already exists) a directory entry to the
 * LDAP server, based on the information supplied in a vCard.
 *
 * If 'op' is set to V2L_DELETE, then delete the entry from LDAP.
 */
void ctdl_vcard_to_ldap(struct CtdlMessage *msg, int op) {
	struct vCard *v = NULL;
	int i, j;
	char this_dn[SIZ];
	LDAPMod **attrs = NULL;
	int num_attrs = 0;
	int num_emails = 0;
	int alias_attr = (-1);
	int num_phones = 0;
	int phone_attr = (-1);
	int have_addr = 0;
	int have_cn = 0;

	char givenname[128];
	char sn[128];
	char uid[256];
	char street[256];
	char city[128];
	char state[3];
	char zipcode[10];
	char calFBURL[256];

	if (dirserver == NULL) return;
	if (msg == NULL) return;
	if (msg->cm_fields['M'] == NULL) return;
	if (msg->cm_fields['A'] == NULL) return;
	if (msg->cm_fields['N'] == NULL) return;

	/* Initialize variables */
	strcpy(givenname, "");
	strcpy(sn, "");
	strcpy(calFBURL, "");

	sprintf(this_dn, "cn=%s,ou=%s,%s",
		msg->cm_fields['A'],
		msg->cm_fields['N'],
		config.c_ldap_base_dn
	);
		
	sprintf(uid, "%s@%s",
		msg->cm_fields['A'],
		msg->cm_fields['N']
	);

	/* Are we just deleting?  If so, it's simple... */
	if (op == V2L_DELETE) {
		lprintf(CTDL_DEBUG, "Calling ldap_delete_s()\n");
		begin_critical_section(S_LDAP);
		i = ldap_delete_s(dirserver, this_dn);
		end_critical_section(S_LDAP);
		if (i != LDAP_SUCCESS) {
			lprintf(CTDL_ERR, "ldap_delete_s() failed: %s (%d)\n",
				ldap_err2string(i), i);
		}
		return;
	}

	/*
	 * If we get to this point then it must be a V2L_WRITE operation.
	 */

	/* First make sure the OU for the user's home Citadel host is created */
	CtdlCreateHostOU(msg->cm_fields['N']);

	/* The first LDAP attribute will be an 'objectclass' list.  Citadel
	 * doesn't do anything with this.  It's just there for compatibility
	 * with Kolab.
	 */
	num_attrs = 1;
	attrs = malloc( (sizeof(LDAPMod *) * num_attrs) );
	attrs[0] = malloc(sizeof(LDAPMod));
	memset(attrs[0], 0, sizeof(LDAPMod));
	attrs[0]->mod_op	= LDAP_MOD_ADD;
	attrs[0]->mod_type	= "objectclass";
	attrs[0]->mod_values	= malloc(3 * sizeof(char *));
	attrs[0]->mod_values[0]	= strdup("citadelInetOrgPerson");
	attrs[0]->mod_values[1]	= NULL;

	/* Convert the vCard fields to LDAP properties */
	v = vcard_load(msg->cm_fields['M']);
	if (v->numprops) for (i=0; i<(v->numprops); ++i) if (striplt(v->prop[i].value), strlen(v->prop[i].value) > 0) {

		if (!strcasecmp(v->prop[i].name, "n")) {
			extract_token(sn,		v->prop[i].value, 0, ';', sizeof sn);
			extract_token(givenname,	v->prop[i].value, 1, ';', sizeof givenname);
		}

		if (!strcasecmp(v->prop[i].name, "fn")) {
			attrs = realloc(attrs, (sizeof(LDAPMod *) * ++num_attrs) );
			attrs[num_attrs-1] = malloc(sizeof(LDAPMod));
			memset(attrs[num_attrs-1], 0, sizeof(LDAPMod));
			attrs[num_attrs-1]->mod_op		= LDAP_MOD_ADD;
			attrs[num_attrs-1]->mod_type		= "cn";
			attrs[num_attrs-1]->mod_values		= malloc(2 * sizeof(char *));
			attrs[num_attrs-1]->mod_values[0]	= strdup(v->prop[i].value);
			attrs[num_attrs-1]->mod_values[1]	= NULL;
			have_cn = 1;
		}

		if (!strcasecmp(v->prop[i].name, "title")) {
			attrs = realloc(attrs, (sizeof(LDAPMod *) * ++num_attrs) );
			attrs[num_attrs-1] = malloc(sizeof(LDAPMod));
			memset(attrs[num_attrs-1], 0, sizeof(LDAPMod));
			attrs[num_attrs-1]->mod_op		= LDAP_MOD_ADD;
			attrs[num_attrs-1]->mod_type		= "title";
			attrs[num_attrs-1]->mod_values		= malloc(2 * sizeof(char *));
			attrs[num_attrs-1]->mod_values[0]	= strdup(v->prop[i].value);
			attrs[num_attrs-1]->mod_values[1]	= NULL;
		}

		if (!strcasecmp(v->prop[i].name, "org")) {
			attrs = realloc(attrs, (sizeof(LDAPMod *) * ++num_attrs) );
			attrs[num_attrs-1] = malloc(sizeof(LDAPMod));
			memset(attrs[num_attrs-1], 0, sizeof(LDAPMod));
			attrs[num_attrs-1]->mod_op		= LDAP_MOD_ADD;
			attrs[num_attrs-1]->mod_type		= "o";
			attrs[num_attrs-1]->mod_values		= malloc(2 * sizeof(char *));
			attrs[num_attrs-1]->mod_values[0]	= strdup(v->prop[i].value);
			attrs[num_attrs-1]->mod_values[1]	= NULL;
		}

		if ( (!strcasecmp(v->prop[i].name, "adr"))
		   ||(!strncasecmp(v->prop[i].name, "adr;", 4)) ) {
			/* Unfortunately, we can only do a single address */
			if (!have_addr) {
				have_addr = 1;
				strcpy(street, "");
				extract_token(&street[strlen(street)],
					v->prop[i].value, 0, ';', (sizeof street - strlen(street))); /* po box */
				strcat(street, " ");
				extract_token(&street[strlen(street)],
					v->prop[i].value, 1, ';', (sizeof street - strlen(street))); /* extend addr */
				strcat(street, " ");
				extract_token(&street[strlen(street)],
					v->prop[i].value, 2, ';', (sizeof street - strlen(street))); /* street */
				striplt(street);
				extract_token(city, v->prop[i].value, 3, ';', sizeof city);
				extract_token(state, v->prop[i].value, 4, ';', sizeof state);
				extract_token(zipcode, v->prop[i].value, 5, ';', sizeof zipcode);

				attrs = realloc(attrs, (sizeof(LDAPMod *) * ++num_attrs) );
				attrs[num_attrs-1] = malloc(sizeof(LDAPMod));
				memset(attrs[num_attrs-1], 0, sizeof(LDAPMod));
				attrs[num_attrs-1]->mod_op		= LDAP_MOD_ADD;
				attrs[num_attrs-1]->mod_type		= "street";
				attrs[num_attrs-1]->mod_values		= malloc(2 * sizeof(char *));
				attrs[num_attrs-1]->mod_values[0]	= strdup(street);
				attrs[num_attrs-1]->mod_values[1]	= NULL;

				attrs = realloc(attrs, (sizeof(LDAPMod *) * ++num_attrs) );
				attrs[num_attrs-1] = malloc(sizeof(LDAPMod));
				memset(attrs[num_attrs-1], 0, sizeof(LDAPMod));
				attrs[num_attrs-1]->mod_op		= LDAP_MOD_ADD;
				attrs[num_attrs-1]->mod_type		= "l";
				attrs[num_attrs-1]->mod_values		= malloc(2 * sizeof(char *));
				attrs[num_attrs-1]->mod_values[0]	= strdup(city);
				attrs[num_attrs-1]->mod_values[1]	= NULL;

				attrs = realloc(attrs, (sizeof(LDAPMod *) * ++num_attrs) );
				attrs[num_attrs-1] = malloc(sizeof(LDAPMod));
				memset(attrs[num_attrs-1], 0, sizeof(LDAPMod));
				attrs[num_attrs-1]->mod_op		= LDAP_MOD_ADD;
				attrs[num_attrs-1]->mod_type		= "st";
				attrs[num_attrs-1]->mod_values		= malloc(2 * sizeof(char *));
				attrs[num_attrs-1]->mod_values[0]	= strdup(state);
				attrs[num_attrs-1]->mod_values[1]	= NULL;

				attrs = realloc(attrs, (sizeof(LDAPMod *) * ++num_attrs) );
				attrs[num_attrs-1] = malloc(sizeof(LDAPMod));
				memset(attrs[num_attrs-1], 0, sizeof(LDAPMod));
				attrs[num_attrs-1]->mod_op		= LDAP_MOD_ADD;
				attrs[num_attrs-1]->mod_type		= "postalcode";
				attrs[num_attrs-1]->mod_values		= malloc(2 * sizeof(char *));
				attrs[num_attrs-1]->mod_values[0]	= strdup(zipcode);
				attrs[num_attrs-1]->mod_values[1]	= NULL;
			}
		}

		if ( (!strcasecmp(v->prop[i].name, "tel"))
		   ||(!strncasecmp(v->prop[i].name, "tel;", 4)) ) {
			++num_phones;
			/* The first 'tel' property creates the 'telephoneNumber' attribute */
			if (num_phones == 1) {
				attrs = realloc(attrs, (sizeof(LDAPMod *) * ++num_attrs) );
				phone_attr = num_attrs-1;
				attrs[phone_attr] = malloc(sizeof(LDAPMod));
				memset(attrs[phone_attr], 0, sizeof(LDAPMod));
				attrs[phone_attr]->mod_op		= LDAP_MOD_ADD;
				attrs[phone_attr]->mod_type		= "telephoneNumber";
				attrs[phone_attr]->mod_values		= malloc(2 * sizeof(char *));
				attrs[phone_attr]->mod_values[0]	= strdup(v->prop[i].value);
				attrs[phone_attr]->mod_values[1]	= NULL;
			}
			/* Subsequent 'tel' properties *add to* the 'telephoneNumber' attribute */
			else {
				attrs[phone_attr]->mod_values = realloc(attrs[phone_attr]->mod_values,
								     num_phones * sizeof(char *));
				attrs[phone_attr]->mod_values[num_phones-1]
									= strdup(v->prop[i].value);
				attrs[phone_attr]->mod_values[num_phones]
									= NULL;
			}
		}


		if ( (!strcasecmp(v->prop[i].name, "email"))
		   ||(!strcasecmp(v->prop[i].name, "email;internet")) ) {
	
			++num_emails;
			lprintf(CTDL_DEBUG, "email addr %d\n", num_emails);

			/* The first email address creates the 'mail' attribute */
			if (num_emails == 1) {
				attrs = realloc(attrs, (sizeof(LDAPMod *) * ++num_attrs) );
				attrs[num_attrs-1] = malloc(sizeof(LDAPMod));
				memset(attrs[num_attrs-1], 0, sizeof(LDAPMod));
				attrs[num_attrs-1]->mod_op		= LDAP_MOD_ADD;
				attrs[num_attrs-1]->mod_type		= "mail";
				attrs[num_attrs-1]->mod_values		= malloc(2 * sizeof(char *));
				attrs[num_attrs-1]->mod_values[0]	= strdup(v->prop[i].value);
				attrs[num_attrs-1]->mod_values[1]	= NULL;
			}
			/* The second email address creates the 'alias' attribute */
			else if (num_emails == 2) {
				attrs = realloc(attrs, (sizeof(LDAPMod *) * ++num_attrs) );
				alias_attr = num_attrs-1;
				attrs[alias_attr] = malloc(sizeof(LDAPMod));
				memset(attrs[alias_attr], 0, sizeof(LDAPMod));
				attrs[alias_attr]->mod_op		= LDAP_MOD_ADD;
				attrs[alias_attr]->mod_type		= "alias";
				attrs[alias_attr]->mod_values		= malloc(2 * sizeof(char *));
				attrs[alias_attr]->mod_values[0]	= strdup(v->prop[i].value);
				attrs[alias_attr]->mod_values[1]	= NULL;
			}
			/* Subsequent email addresses *add to* the 'alias' attribute */
			else if (num_emails > 2) {
				attrs[alias_attr]->mod_values = realloc(attrs[alias_attr]->mod_values,
								     num_emails * sizeof(char *));
				attrs[alias_attr]->mod_values[num_emails-2]
									= strdup(v->prop[i].value);
				attrs[alias_attr]->mod_values[num_emails-1]
									= NULL;
			}


		}

		/* Calendar free/busy URL (take the first one we find, but if a subsequent
		 * one contains the "pref" designation then we go with that instead.)
		 */
		if ( (!strcasecmp(v->prop[i].name, "fburl"))
		   ||(!strncasecmp(v->prop[i].name, "fburl;", 6)) ) {
			if ( (strlen(calFBURL) == 0)
			   || (!strncasecmp(v->prop[i].name, "fburl;pref", 10)) ) {
				safestrncpy(calFBURL, v->prop[i].value, sizeof calFBURL);
			}
		}

	}
	vcard_free(v);	/* Don't need this anymore. */

	/* "sn" (surname) based on info in vCard */
	attrs = realloc(attrs, (sizeof(LDAPMod *) * ++num_attrs) );
	attrs[num_attrs-1] = malloc(sizeof(LDAPMod));
	memset(attrs[num_attrs-1], 0, sizeof(LDAPMod));
	attrs[num_attrs-1]->mod_op		= LDAP_MOD_ADD;
	attrs[num_attrs-1]->mod_type		= "sn";
	attrs[num_attrs-1]->mod_values		= malloc(2 * sizeof(char *));
	attrs[num_attrs-1]->mod_values[0]	= strdup(sn);
	attrs[num_attrs-1]->mod_values[1]	= NULL;

	/* "givenname" (first name) based on info in vCard */
	if (strlen(givenname) == 0) strcpy(givenname, "_");
	if (strlen(sn) == 0) strcpy(sn, "_");
	attrs = realloc(attrs, (sizeof(LDAPMod *) * ++num_attrs) );
	attrs[num_attrs-1] = malloc(sizeof(LDAPMod));
	memset(attrs[num_attrs-1], 0, sizeof(LDAPMod));
	attrs[num_attrs-1]->mod_op		= LDAP_MOD_ADD;
	attrs[num_attrs-1]->mod_type		= "givenname";
	attrs[num_attrs-1]->mod_values		= malloc(2 * sizeof(char *));
	attrs[num_attrs-1]->mod_values[0]	= strdup(givenname);
	attrs[num_attrs-1]->mod_values[1]	= NULL;

	/* "uid" is a Kolab compatibility thing.  We just do cituser@citnode */
	attrs = realloc(attrs, (sizeof(LDAPMod *) * ++num_attrs) );
	attrs[num_attrs-1] = malloc(sizeof(LDAPMod));
	memset(attrs[num_attrs-1], 0, sizeof(LDAPMod));
	attrs[num_attrs-1]->mod_op		= LDAP_MOD_ADD;
	attrs[num_attrs-1]->mod_type		= "uid";
	attrs[num_attrs-1]->mod_values		= malloc(2 * sizeof(char *));
	attrs[num_attrs-1]->mod_values[0]	= strdup(uid);
	attrs[num_attrs-1]->mod_values[1]	= NULL;

	/* Add a "cn" (Common Name) attribute based on the user's screen name,
	 * but only there was no 'fn' (full name) property in the vCard	
	 */
	if (!have_cn) {
		attrs = realloc(attrs, (sizeof(LDAPMod *) * ++num_attrs) );
		attrs[num_attrs-1] = malloc(sizeof(LDAPMod));
		memset(attrs[num_attrs-1], 0, sizeof(LDAPMod));
		attrs[num_attrs-1]->mod_op		= LDAP_MOD_ADD;
		attrs[num_attrs-1]->mod_type		= "cn";
		attrs[num_attrs-1]->mod_values		= malloc(2 * sizeof(char *));
		attrs[num_attrs-1]->mod_values[0]	= strdup(msg->cm_fields['A']);
		attrs[num_attrs-1]->mod_values[1]	= NULL;
	}

	/* Add a "calFBURL" attribute if a calendar free/busy URL exists */
	if (strlen(calFBURL) > 0) {
		attrs = realloc(attrs, (sizeof(LDAPMod *) * ++num_attrs) );
		attrs[num_attrs-1] = malloc(sizeof(LDAPMod));
		memset(attrs[num_attrs-1], 0, sizeof(LDAPMod));
		attrs[num_attrs-1]->mod_op		= LDAP_MOD_ADD;
		attrs[num_attrs-1]->mod_type		= "calFBURL";
		attrs[num_attrs-1]->mod_values		= malloc(2 * sizeof(char *));
		attrs[num_attrs-1]->mod_values[0]	= strdup(calFBURL);
		attrs[num_attrs-1]->mod_values[1]	= NULL;
	}
	
	/* The last attribute must be a NULL one. */
	attrs = realloc(attrs, (sizeof(LDAPMod *) * ++num_attrs) );
	attrs[num_attrs - 1] = NULL;
	
	lprintf(CTDL_DEBUG, "Calling ldap_add_s() for '%s'\n", this_dn);
	begin_critical_section(S_LDAP);
	i = ldap_add_s(dirserver, this_dn, attrs);
	end_critical_section(S_LDAP);

	/* If the entry already exists, repopulate it instead */
	if (i == LDAP_ALREADY_EXISTS) {
		for (j=0; j<(num_attrs-1); ++j) {
			attrs[j]->mod_op = LDAP_MOD_REPLACE;
		}
		lprintf(CTDL_DEBUG, "Calling ldap_modify_s() for '%s'\n", this_dn);
		begin_critical_section(S_LDAP);
		i = ldap_modify_s(dirserver, this_dn, attrs);
		end_critical_section(S_LDAP);
	}

	if (i != LDAP_SUCCESS) {
		lprintf(CTDL_ERR, "ldap_add_s() failed: %s (%d)\n",
			ldap_err2string(i), i);
	}

	lprintf(CTDL_DEBUG, "Freeing attributes\n");
	/* Free the attributes */
	for (i=0; i<num_attrs; ++i) {
		if (attrs[i] != NULL) {

			/* First, free the value strings */
			if (attrs[i]->mod_values != NULL) {
				for (j=0; attrs[i]->mod_values[j] != NULL; ++j) {
					free(attrs[i]->mod_values[j]);
				}
			}

			/* Free the value strings pointer list */	
			if (attrs[i]->mod_values != NULL) {
				free(attrs[i]->mod_values);
			}

			/* Now free the LDAPMod struct itself. */
			free(attrs[i]);
		}
	}
	free(attrs);
	lprintf(CTDL_DEBUG, "LDAP write operation complete.\n");
}


#endif				/* HAVE_LDAP */


/*
 * Initialize the LDAP connector module ... or don't, if we don't have LDAP.
 */
CTDL_MODULE_INIT(ldap)
{
#ifdef HAVE_LDAP
	CtdlRegisterCleanupHook(serv_ldap_cleanup);

	if (strlen(config.c_ldap_host) > 0) {
		CtdlConnectToLdap();
	}

#endif				/* HAVE_LDAP */

	/* return our Subversion id for the Log */
	return "$Id$";
}
