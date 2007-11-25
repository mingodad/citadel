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
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "room_ops.h"
#include "policy.h"
#include "database.h"
#include "serv_ldap.h"


#include "ctdl_module.h"



#ifdef HAVE_LDAP

#define LDAP_DEPRECATED 1	/* to stop warnings with newer libraries */

#include <ldap.h>

LDAP *dirserver = NULL;
int ldap_time_disconnect = 0;



/* There is a forward referance so.... */
int delete_from_ldap(char *cn, char *ou, void **object);


/*
 * LDAP connector cleanup function
 */
void serv_ldap_cleanup(void)
{
	if (dirserver) {
		CtdlLogPrintf(CTDL_INFO,
			"LDAP: Unbinding from directory server\n");
		ldap_unbind(dirserver);
	}
	dirserver = NULL;
	ldap_time_disconnect = 0;
}


/*
 * connect_to_ldap
 *
 * BIG FAT WARNING
 * Make sure this function is only called from within a begin_critical_section(S_LDAP)
 * If you don't things will break!!!!!.
 */


int connect_to_ldap(void)
{
	int i;
	int ldap_version = 3;

	if (dirserver) {	// Already connected
		ldap_time_disconnect = 1 ;	// reset the timer.
		return 0;
	}

	CtdlLogPrintf(CTDL_INFO, "LDAP: Connecting to LDAP server %s:%d...\n",
		config.c_ldap_host, config.c_ldap_port);

	dirserver = ldap_init(config.c_ldap_host, config.c_ldap_port);
	if (dirserver == NULL) {
		CtdlLogPrintf(CTDL_CRIT,
			"LDAP: Could not connect to %s:%d : %s\n",
			config.c_ldap_host, config.c_ldap_port,
			strerror(errno));
		CtdlAideMessage(strerror(errno),
			     "LDAP: Could not connect to server.");
		return -1;
	}

	ldap_set_option(dirserver, LDAP_OPT_PROTOCOL_VERSION,
			&ldap_version);

	CtdlLogPrintf(CTDL_INFO, "LDAP: Binding to %s\n", config.c_ldap_bind_dn);

	i = ldap_simple_bind_s(dirserver,
			       config.c_ldap_bind_dn,
			       config.c_ldap_bind_pw);
	if (i != LDAP_SUCCESS) {
		CtdlLogPrintf(CTDL_CRIT, "LDAP: Cannot bind: %s (%d)\n",
			ldap_err2string(i), i);
		dirserver = NULL;	/* FIXME disconnect from ldap */
		CtdlAideMessage(ldap_err2string(i),
			     "LDAP: Cannot bind to server");
		return -1;
	}
	ldap_time_disconnect = 1;
	return 0;
}



/*
 * Create the root node.  If it's already there, so what?
 */
void create_ldap_root(void)
{
	char *dc_values[2];
	char *objectClass_values[3];
	LDAPMod dc, objectClass;
	LDAPMod *mods[3];
	char topdc[SIZ];
	int i;

	/* We just want the top-level dc, not the whole hierarchy */
	strcpy(topdc, config.c_ldap_base_dn);
	for (i = 0; topdc[i]; ++i) {
		if (topdc[i] == ',') {
			topdc[i] = 0;
			break;
		}
	}
	for (i = 0; topdc[i]; ++i) {
		if (topdc[i] == '=')
			strcpy(topdc, &topdc[i + 1]);
	}

	/* Set up the transaction */
	dc.mod_op = LDAP_MOD_ADD;
	dc.mod_type = "dc";
	dc_values[0] = topdc;
	dc_values[1] = NULL;
	dc.mod_values = dc_values;
	objectClass.mod_op = LDAP_MOD_ADD;
	objectClass.mod_type = "objectClass";
	objectClass_values[0] = "top";
	objectClass_values[1] = "domain";
	objectClass_values[2] = NULL;
	objectClass.mod_values = objectClass_values;
	mods[0] = &dc;
	mods[1] = &objectClass;
	mods[2] = NULL;

	/* Perform the transaction */
	CtdlLogPrintf(CTDL_DEBUG, "LDAP: Setting up Base DN node...\n");

	begin_critical_section(S_LDAP);
	if (connect_to_ldap()) {
		end_critical_section(S_LDAP);
		return;
	}
	i = ldap_add_s(dirserver, config.c_ldap_base_dn, mods);
	end_critical_section(S_LDAP);

	if (i == LDAP_ALREADY_EXISTS) {
		CtdlLogPrintf(CTDL_INFO,
			"LDAP: Base DN is already present in the directory; no need to add it again.\n");
	} else if (i != LDAP_SUCCESS) {
		CtdlLogPrintf(CTDL_CRIT, "LDAP: ldap_add_s() failed: %s (%d)\n",
			ldap_err2string(i), i);
	}
}


/*
 * Create an OU node representing a Citadel host.
 * parameter cn is not used, its just there to keep the hook interface consistant
 * parameter object not used here, present for interface compatability
 */
int create_ldap_host_OU(char *cn, char *host, void **object)
{
	char *dc_values[2];
	char *objectClass_values[3];
	LDAPMod dc, objectClass;
	LDAPMod *mods[3];
	int i;
	char dn[SIZ];

	/* The DN is this OU plus the base. */
	snprintf(dn, sizeof dn, "ou=%s,%s", host, config.c_ldap_base_dn);

	/* Set up the transaction */
	dc.mod_op = LDAP_MOD_ADD;
	dc.mod_type = "ou";
	dc_values[0] = host;
	dc_values[1] = NULL;
	dc.mod_values = dc_values;
	objectClass.mod_op = LDAP_MOD_ADD;
	objectClass.mod_type = "objectClass";
	objectClass_values[0] = "top";
	objectClass_values[1] = "organizationalUnit";
	objectClass_values[2] = NULL;
	objectClass.mod_values = objectClass_values;
	mods[0] = &dc;
	mods[1] = &objectClass;
	mods[2] = NULL;

	/* Perform the transaction */
	CtdlLogPrintf(CTDL_DEBUG, "LDAP: Setting up Host OU node...\n");

	begin_critical_section(S_LDAP);
	if (connect_to_ldap()) {
		end_critical_section(S_LDAP);
		return -1;
	}
	i = ldap_add_s(dirserver, dn, mods);
	end_critical_section(S_LDAP);

	if (i == LDAP_ALREADY_EXISTS) {
		CtdlLogPrintf(CTDL_INFO,
			"LDAP: Host OU is already present in the directory; no need to add it again.\n");
	} else if (i != LDAP_SUCCESS) {
		CtdlLogPrintf(CTDL_CRIT, "LDAP: ldap_add_s() failed: %s (%d)\n",
			ldap_err2string(i), i);
		return -1;
	}
	return 0;
}






/*
 * Create a base LDAP object for the interface
 */

int create_ldap_object(char *cn, char *ou, void **object)
{
	// We do nothing here, this just gets the base structure created by the interface.
	CtdlLogPrintf(CTDL_DEBUG, "LDAP: Created ldap object\n");
	return 0;
}


/*
 * Add an attribute to the ldap object
 */

int add_ldap_object(char *cn, char *ou, void **object)
{
	LDAPMod **attrs;
	int num_attrs = 0;
	int num_values = 0;
	int cur_attr;


	CtdlLogPrintf(CTDL_DEBUG,
		"LDAP: Adding ldap attribute name:\"%s\" value:\"%s\"\n",
		cn, ou);

	attrs = *object;
	if (attrs) {
		while (attrs[num_attrs])
			num_attrs++;
	}

	for (cur_attr = 0; cur_attr < num_attrs; cur_attr++) {
		if (!strcmp(attrs[cur_attr]->mod_type, cn)) {	// Adding a value to the attribute
			if (attrs[cur_attr]->mod_values) {
				while (attrs[cur_attr]->
				       mod_values[num_values]) {
					if (!strcmp
					    (ou,
					     attrs[cur_attr]->
					     mod_values[num_values])) {
						CtdlLogPrintf(CTDL_DEBUG,
							"LDAP: Ignoring duplicate attribute/value pair\n");
						return 0;
					}
					num_values++;
				}
			}
			attrs[cur_attr]->mod_values =
			    realloc(attrs[cur_attr]->mod_values,
				    (num_values + 2) * (sizeof(char *)));
			attrs[cur_attr]->mod_values[num_values] =
			    strdup(ou);
			attrs[cur_attr]->mod_values[num_values + 1] = NULL;
			return 0;
		}
	}
	if (num_attrs)
		attrs =
		    realloc(attrs, (sizeof(LDAPMod *)) * (num_attrs + 2));
	else
		attrs = malloc((sizeof(LDAPMod *)) * (num_attrs + 2));
	attrs[num_attrs] = malloc(sizeof(LDAPMod));
	memset(attrs[num_attrs], 0, sizeof(LDAPMod));
	attrs[num_attrs + 1] = NULL;
	attrs[num_attrs]->mod_op = LDAP_MOD_ADD;
	attrs[num_attrs]->mod_type = strdup(cn);
	attrs[num_attrs]->mod_values = malloc(2 * sizeof(char *));
	attrs[num_attrs]->mod_values[0] = strdup(ou);
	attrs[num_attrs]->mod_values[1] = NULL;
	*object = attrs;
	return 0;
}


/*
 * SAve the object to the LDAP server
 */
int save_ldap_object(char *cn, char *ou, void **object)
{
	int i;

	char this_dn[SIZ];
	LDAPMod **attrs;
	int num_attrs = 0;
	int count = 0;

	if (dirserver == NULL)
		return -1;
	if (cn == NULL)
		return -1;

	sprintf(this_dn, "%s,%s", cn, config.c_ldap_base_dn);

	CtdlLogPrintf(CTDL_INFO, "LDAP: Calling ldap_add_s() for dn of '%s'\n",
		this_dn);

	/* The last attribute must be a NULL one. */
	attrs = (LDAPMod **) * object;
	if (attrs) {
		while (attrs[num_attrs]) {
			count = 0;
			while (attrs[num_attrs]->mod_values[count]) {
				CtdlLogPrintf(CTDL_DEBUG,
					"LDAP: attribute %d, value %d = \'%s=%s\'\n",
					num_attrs, count,
					attrs[num_attrs]->mod_type,
					attrs[num_attrs]->
					mod_values[count]);
				count++;
			}
			num_attrs++;
		}
	} else {
		CtdlLogPrintf(CTDL_ERR,
			"LDAP: no attributes in save_ldap_object\n");
		return -1;
	}

	begin_critical_section(S_LDAP);
	if (connect_to_ldap()) {
		end_critical_section(S_LDAP);
		return -1;
	}

	i = ldap_add_s(dirserver, this_dn, attrs);

	if (i == LDAP_SERVER_DOWN) {
		CtdlAideMessage
		    ("The LDAP server appears to be down.\nThe save to LDAP did not occurr.\n",
		     "LDAP: save failed");
		end_critical_section(S_LDAP);
		return -1;
	}

	/* If the entry already exists, repopulate it instead */
	/* repopulating doesn't work as Citadel may want some attributes to be deleted.
	 * we have no way of knowing which attributes to delete and LDAP won't work it out for us
	 * so now we delete the old entry and create a new one.
	 */
	if (i == LDAP_ALREADY_EXISTS) {
		end_critical_section(S_LDAP);
		CtdlLogPrintf(CTDL_INFO,
			"LDAP: Create, already exists, deleteing first.\n");
		if (delete_from_ldap(cn, ou, NULL))
			return -1;
		begin_critical_section(S_LDAP);
		CtdlLogPrintf(CTDL_INFO,
			"LDAP: Calling ldap_add_s() to recreate for dn of '%s'\n",
			this_dn);
		i = ldap_add_s(dirserver, this_dn, attrs);
	}

	if (i != LDAP_SUCCESS) {
		CtdlLogPrintf(CTDL_ERR, "LDAP: ldap_add_s() failed: %s (%d)\n",
			ldap_err2string(i), i);
		CtdlAideMessage
		    ("The LDAP server refused the save command.\nDid you update the schema?\n",
		     "LDAP: save failed (schema?)");
		end_critical_section(S_LDAP);
		return -1;
	}
	end_critical_section(S_LDAP);
	return 0;
}


/*
 * Free the object
 */
int free_ldap_object(char *cn, char *ou, void **object)
{
	int i, j;

	LDAPMod **attrs;
	int num_attrs = 0;

	attrs = (LDAPMod **) * object;
	if (attrs) {
		while (attrs[num_attrs])
			num_attrs++;
	}

	CtdlLogPrintf(CTDL_DEBUG, "LDAP: Freeing attributes\n");
	/* Free the attributes */
	for (i = 0; i < num_attrs; ++i) {
		if (attrs[i] != NULL) {

			/* First, free the value strings */
			if (attrs[i]->mod_values != NULL) {
				for (j = 0;
				     attrs[i]->mod_values[j] != NULL;
				     ++j) {
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
int delete_from_ldap(char *cn, char *ou, void **object)
{
	int i;

	char this_dn[SIZ];

	if (dirserver == NULL)
		return -1;
	if (cn == NULL)
		return -1;

	sprintf(this_dn, "%s,%s", cn, config.c_ldap_base_dn);

	CtdlLogPrintf(CTDL_DEBUG, "LDAP: Calling ldap_delete_s()\n");

	begin_critical_section(S_LDAP);
	if (connect_to_ldap()) {
		end_critical_section(S_LDAP);
		return -1;
	}

	i = ldap_delete_s(dirserver, this_dn);

	if (i == LDAP_SERVER_DOWN) {
		end_critical_section(S_LDAP);
		CtdlAideMessage
		    ("The LDAP server appears to be down.\nThe delete from LDAP did not occurr.\n",
		     "LDAP: delete failed");
		return -1;
	}

	if (i != LDAP_SUCCESS) {
		CtdlLogPrintf(CTDL_ERR,
			"LDAP: ldap_delete_s() failed: %s (%d)\n",
			ldap_err2string(i), i);
		end_critical_section(S_LDAP);
		CtdlAideMessage(ldap_err2string(i), "LDAP: delete failed");
		return -1;
	}
	end_critical_section(S_LDAP);
	return 0;
}




void ldap_disconnect_timer(void)
{
	begin_critical_section(S_LDAP);
	if (ldap_time_disconnect) {
		ldap_time_disconnect--;
		end_critical_section(S_LDAP);
		return;
	}
	serv_ldap_cleanup();
	end_critical_section(S_LDAP);
}


#endif				/* HAVE_LDAP */


/*
 * Initialize the LDAP connector module ... or don't, if we don't have LDAP.
 */
CTDL_MODULE_INIT(ldap)
{
	if (!threading)
	{
#ifdef HAVE_LDAP
		if (!IsEmptyStr(config.c_ldap_base_dn)) {
			CtdlRegisterCleanupHook(serv_ldap_cleanup);
			CtdlRegisterSessionHook(ldap_disconnect_timer, EVT_TIMER);
			CtdlRegisterDirectoryServiceFunc(delete_from_ldap,
							 DIRECTORY_USER_DEL,
							 "ldap");
			CtdlRegisterDirectoryServiceFunc(create_ldap_host_OU,
							 DIRECTORY_CREATE_HOST,
							 "ldap");
			CtdlRegisterDirectoryServiceFunc(create_ldap_object,
							 DIRECTORY_CREATE_OBJECT,
							 "ldap");
			CtdlRegisterDirectoryServiceFunc(add_ldap_object,
							 DIRECTORY_ATTRIB_ADD,
							 "ldap");
			CtdlRegisterDirectoryServiceFunc(save_ldap_object,
							 DIRECTORY_SAVE_OBJECT,
							 "ldap");
			CtdlRegisterDirectoryServiceFunc(free_ldap_object,
							 DIRECTORY_FREE_OBJECT,
							 "ldap");
			create_ldap_root();
		}
#endif				/* HAVE_LDAP */
	}
	
	/* return our Subversion id for the Log */
	return "$Id$";
}
