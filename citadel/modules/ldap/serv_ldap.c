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
#include "tools.h"


#include "ctdl_module.h"



#ifdef HAVE_LDAP

#define LDAP_DEPRECATED 1	/* to stop warnings with newer libraries */

#include <ldap.h>

LDAP *dirserver = NULL;

/*
 * LDAP connector cleanup function
 */
void serv_ldap_cleanup(void)
{
	if (!dirserver) return;

	lprintf(CTDL_INFO, "LDAP: Unbinding from directory server\n");
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
	for (i=0; topdc[i]; ++i) {
		if (topdc[i] == ',') {
			topdc[i] = 0;
			break;
		}
	}
	for (i=0; topdc[i]; ++i) {
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
	lprintf(CTDL_DEBUG, "LDAP: Setting up Base DN node...\n");
	begin_critical_section(S_LDAP);
	i = ldap_add_s(dirserver, config.c_ldap_base_dn, mods);
	end_critical_section(S_LDAP);

	if (i == LDAP_ALREADY_EXISTS) {
		lprintf(CTDL_INFO, "LDAP: Base DN is already present in the directory; no need to add it again.\n");
	}
	else if (i != LDAP_SUCCESS) {
		lprintf(CTDL_CRIT, "LDAP: ldap_add_s() failed: %s (%d)\n",
			ldap_err2string(i), i);
	}
}


/*
 * Create an OU node representing a Citadel host.
 * parameter cn is not used, its just there to keep the hook interface consistant
 * parameter object not used here, present for interface compatability
 */
int CtdlCreateLdapHostOU(char *cn, char *host, void **object) {
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
	lprintf(CTDL_DEBUG, "LDAP: Setting up Host OU node...\n");
	begin_critical_section(S_LDAP);
	i = ldap_add_s(dirserver, dn, mods);
	end_critical_section(S_LDAP);

	if (i == LDAP_ALREADY_EXISTS) {
		lprintf(CTDL_INFO, "LDAP: Host OU is already present in the directory; no need to add it again.\n");
	}
	else if (i != LDAP_SUCCESS) {
		lprintf(CTDL_CRIT, "LDAP: ldap_add_s() failed: %s (%d)\n",
			ldap_err2string(i), i);
		return -1;
	}
	return 0;
}








void CtdlConnectToLdap(void) {
	int i;
	int ldap_version = 3;

	lprintf(CTDL_INFO, "LDAP: Connecting to LDAP server %s:%d...\n",
		config.c_ldap_host, config.c_ldap_port);

	dirserver = ldap_init(config.c_ldap_host, config.c_ldap_port);
	if (dirserver == NULL) {
		lprintf(CTDL_CRIT, "LDAP: Could not connect to %s:%d : %s\n",
			config.c_ldap_host,
			config.c_ldap_port,
			strerror(errno));
		return;
	}

	ldap_set_option(dirserver, LDAP_OPT_PROTOCOL_VERSION, &ldap_version);

	lprintf(CTDL_INFO, "LDAP: Binding to %s\n", config.c_ldap_bind_dn);

	i = ldap_simple_bind_s(dirserver,
				config.c_ldap_bind_dn,
				config.c_ldap_bind_pw
	);
	if (i != LDAP_SUCCESS) {
		lprintf(CTDL_CRIT, "LDAP: Cannot bind: %s (%d)\n", ldap_err2string(i), i);
		dirserver = NULL;	/* FIXME disconnect from ldap */
		return;
	}

	CtdlCreateLdapRoot();
}



/*
 * Create a base LDAP object for the interface
 */
 
int CtdlCreateLdapObject(char *cn, char *ou, void **object)
{
	// We do nothing here, this just gets the base structure created by the interface.
	lprintf (CTDL_DEBUG, "LDAP: Created ldap object\n");
	return 0;
}


/*
 * Add an attribute to the ldap object
 */
 
int CtdlAddLdapAttr(char *cn, char *ou, void **object)
{
	LDAPMod **attrs ;
	int num_attrs = 0;
	int num_values = 0;
	int cur_attr;
	
	
	lprintf (CTDL_DEBUG, "LDAP: Adding ldap attribute name:\"%s\" vlaue:\"%s\"\n", cn, ou);
	
	attrs = *object;
	if (attrs)
	{
		while (attrs[num_attrs])
			num_attrs++;
	}
	
	for (cur_attr = 0; cur_attr < num_attrs ; cur_attr++)
	{
		if (!strcmp(attrs[cur_attr]->mod_type, cn))
		{	// Adding a value to the attribute
			if (attrs[cur_attr]->mod_values)
			{
				while (attrs[cur_attr]->mod_values[num_values])
					num_values++;
			}
			attrs[cur_attr]->mod_values = realloc(attrs[cur_attr]->mod_values, (num_values + 2) * (sizeof(char *)));
			attrs[cur_attr]->mod_values[num_values] = strdup(ou);
			attrs[cur_attr]->mod_values[num_values+1] = NULL;
			return 0;
		}
	}
	if (num_attrs)
		attrs = realloc(attrs, (sizeof(LDAPMod *)) * (num_attrs + 2));
	else
		attrs = malloc((sizeof(LDAPMod *)) * (num_attrs + 2));
	attrs[num_attrs] = malloc(sizeof(LDAPMod));
	memset(attrs[num_attrs], 0, sizeof(LDAPMod));
	attrs[num_attrs+1] = NULL;
	attrs[num_attrs]->mod_op	= LDAP_MOD_ADD;
	attrs[num_attrs]->mod_type	= strdup(cn);
	attrs[num_attrs]->mod_values	= malloc(2 * sizeof(char *));
	attrs[num_attrs]->mod_values[0]	= strdup(ou);
	attrs[num_attrs]->mod_values[1]	= NULL;
	*object = attrs;
	return 0;
}


/*
 * SAve the object to the LDAP server
 */
int CtdlSaveLdapObject(char *cn, char *ou, void **object)
{
	int i, j;
	
	char this_dn[SIZ];
	LDAPMod **attrs ;
	int num_attrs = 0;
	int count = 0;
	
	if (dirserver == NULL) return -1;
	if (ou == NULL) return -1;
	if (cn == NULL) return -1;
	
	sprintf(this_dn, "euid=%s,ou=%s,%s", cn, ou, config.c_ldap_base_dn);
	
	lprintf(CTDL_INFO, "LDAP: Calling ldap_add_s() for dn of '%s'\n", this_dn);

	/* The last attribute must be a NULL one. */
	attrs = (LDAPMod **)*object;
	if (attrs)
	{
		while (attrs[num_attrs])
		{
			count = 0;
			while (attrs[num_attrs]->mod_values[count])
			{
				lprintf (CTDL_DEBUG, "LDAP: attribute %d, value %d = \'%s=%s\'\n", num_attrs, count, attrs[num_attrs]->mod_type, attrs[num_attrs]->mod_values[count]);
				count++;
			}
			num_attrs++;
		}
	}
	else
	{
		lprintf(CTDL_ERR, "LDAP: no attributes in CtdlSaveLdapObject\n");
		return -1;
	}
	
	begin_critical_section(S_LDAP);
	i = ldap_add_s(dirserver, this_dn, attrs);
	end_critical_section(S_LDAP);
	
	if (i == LDAP_SERVER_DOWN)
	{	// failed to connect so try to re init the connection
		serv_ldap_cleanup();
		CtdlConnectToLdap();
		// And try the save again.
		begin_critical_section(S_LDAP);
		i = ldap_add_s(dirserver, this_dn, attrs);
		end_critical_section(S_LDAP);
	}

	/* If the entry already exists, repopulate it instead */
	if (i == LDAP_ALREADY_EXISTS) {
		for (j=0; j<(num_attrs); ++j) {
			attrs[j]->mod_op = LDAP_MOD_REPLACE;
		}
		lprintf(CTDL_INFO, "LDAP: Calling ldap_modify_s() for dn of '%s'\n", this_dn);
		begin_critical_section(S_LDAP);
		i = ldap_modify_s(dirserver, this_dn, attrs);
		end_critical_section(S_LDAP);
	}

	if (i != LDAP_SUCCESS) {
		lprintf(CTDL_ERR, "LDAP: ldap_add_s() failed: %s (%d)\n",
			ldap_err2string(i), i);
		return -1;
	}
	return 0;
}


/*
 * Free the object
 */
int CtdlFreeLdapObject(char *cn, char *ou, void **object)
{
	int i, j;
	
	LDAPMod **attrs ;
	int num_attrs = 0;

	attrs = (LDAPMod **)*object;
	if (attrs)
	{
		while (attrs[num_attrs])
			num_attrs++;
	}

	lprintf(CTDL_DEBUG, "LDAP: Freeing attributes\n");
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
	free(attrs[i]);
	free(attrs);
	*object = NULL;
	return 0;
}


/*
 * Delete a record from the LDAP
 *
 * parameter object not used here, present for hook interface compatability
 */
int CtdlDeleteFromLdap(char *cn, char *ou, void **object)
{
	int i;
	
	char this_dn[SIZ];
	
	if (dirserver == NULL) return -1;
	if (ou == NULL) return -1;
	if (cn == NULL) return -1;
	
	sprintf(this_dn, "cn=%s,ou=%s,%s", cn, ou, config.c_ldap_base_dn);
	
	lprintf(CTDL_DEBUG, "LDAP: Calling ldap_delete_s()\n");
	
	begin_critical_section(S_LDAP);
	i = ldap_delete_s(dirserver, this_dn);
	end_critical_section(S_LDAP);
	
	if (i == LDAP_SERVER_DOWN)
	{	// failed to connect so try to re init the connection
		serv_ldap_cleanup();
		CtdlConnectToLdap();
		// And try the delete again.
		begin_critical_section(S_LDAP);
		i = ldap_delete_s(dirserver, this_dn);
		end_critical_section(S_LDAP);
	}

	if (i != LDAP_SUCCESS) {
		lprintf(CTDL_ERR, "LDAP: ldap_delete_s() failed: %s (%d)\n",
			ldap_err2string(i), i);
		return -1;
	}
	return 0;
}


#endif				/* HAVE_LDAP */


/*
 * Initialize the LDAP connector module ... or don't, if we don't have LDAP.
 */
CTDL_MODULE_INIT(ldap)
{
#ifdef HAVE_LDAP
	if (!IsEmptyStr(config.c_ldap_base_dn))
	{
		CtdlRegisterCleanupHook(serv_ldap_cleanup);
		CtdlRegisterDirectoryServiceFunc(CtdlDeleteFromLdap, DIRECTORY_USER_DEL, "ldap");
		CtdlRegisterDirectoryServiceFunc(CtdlCreateLdapHostOU, DIRECTORY_CREATE_HOST, "ldap");
		CtdlRegisterDirectoryServiceFunc(CtdlCreateLdapObject, DIRECTORY_CREATE_OBJECT, "ldap");
		CtdlRegisterDirectoryServiceFunc(CtdlAddLdapAttr, DIRECTORY_ATTRIB_ADD, "ldap");
		CtdlRegisterDirectoryServiceFunc(CtdlSaveLdapObject, DIRECTORY_SAVE_OBJECT, "ldap");
		CtdlRegisterDirectoryServiceFunc(CtdlFreeLdapObject, DIRECTORY_FREE_OBJECT, "ldap");
		CtdlConnectToLdap();
	}

#endif				/* HAVE_LDAP */

	/* return our Subversion id for the Log */
	return "$Id$";
}
