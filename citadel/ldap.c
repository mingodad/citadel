/*
 * 
 */


int ldap_version = 3;

#ifdef HAVE_LDAP

#include "sysdep.h"
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <pwd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

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

#include <string.h>
#include <limits.h>
#include <libcitadel.h>
#include "auth.h"
#include "citadel.h"
#include "server.h"
#include "database.h"
#include "user_ops.h"
#include "sysdep_decls.h"
#include "support.h"
#include "room_ops.h"
#include "file_ops.h"
#include "control.h"
#include "msgbase.h"
#include "config.h"
#include "citserver.h"
#include "citadel_dirs.h"
#include "genstamp.h"
#include "threads.h"
#include "citadel_ldap.h"

#define LDAP_DEPRECATED 1	/* Needed to suppress misleading warnings */

#include <ldap.h>

int CtdlTryUserLDAP(char *username, char *found_dn, int found_dn_size, char *fullname, int fullname_size)
{
	LDAP *ldserver = NULL;
	int i;
	LDAPMessage *search_result = NULL;
	LDAPMessage *entry = NULL;
	char searchstring[1024];
	struct timeval tv;
	char **values;
	char *user_dn = NULL;

	safestrncpy(fullname, username, fullname_size);

	ldserver = ldap_init(CTDL_LDAP_HOST, CTDL_LDAP_PORT);
	if (ldserver == NULL) {
		CtdlLogPrintf(CTDL_ALERT, "LDAP: Could not connect to %s:%d : %s\n",
			CTDL_LDAP_HOST, CTDL_LDAP_PORT,
			strerror(errno));
		return(errno);
	}

	ldap_set_option(ldserver, LDAP_OPT_PROTOCOL_VERSION, &ldap_version);

	i = ldap_simple_bind_s(ldserver, BIND_DN, BIND_PW);
	if (i != LDAP_SUCCESS) {
		CtdlLogPrintf(CTDL_ALERT, "LDAP: Cannot bind: %s (%d)\n", ldap_err2string(i), i);
		return(i);
	}

	tv.tv_sec = 10;
	tv.tv_usec = 0;

	sprintf(searchstring, SEARCH_STRING, username);

	i = ldap_search_st(ldserver,
		BASE_DN,
		LDAP_SCOPE_SUBTREE,
		searchstring,
		NULL,	// return all attributes
		0,	// attributes + values
		&tv,	// timeout
		&search_result
	);
	if (i != LDAP_SUCCESS) {
		CtdlLogPrintf(CTDL_DEBUG,
			"Couldn't find what I was looking for: %s (%d)\n", ldap_err2string(i), i);
		ldap_unbind(ldserver);
		return(i);
	}

	if (search_result == NULL) {
		CtdlLogPrintf(CTDL_DEBUG, "No results were returned\n");
		ldap_unbind(ldserver);
		return(2);
	}

	/* At this point we've got at least one result from our query.  If there are multiple
	 * results, we still only look at the first one.
	 */
	entry = ldap_first_entry(ldserver, search_result);
	if (entry) {

		user_dn = ldap_get_dn(ldserver, entry);
		if (user_dn) {
			CtdlLogPrintf(CTDL_DEBUG, "dn = %s\n", user_dn);
		}

		values = ldap_get_values(ldserver, search_result, "cn");
		if (values) {
			if (values[0]) {
				safestrncpy(fullname, values[0], fullname_size);
				CtdlLogPrintf(CTDL_DEBUG, "cn = %s\n", values[0]);
			}
			ldap_value_free(values);
		}

		values = ldap_get_values(ldserver, search_result, "uidNumber");
		if (values) {
			if (values[0]) {
				CtdlLogPrintf(CTDL_DEBUG, "uidNumber = %s\n", values[0]);
			}
			ldap_value_free(values);
		}

		values = ldap_get_values(ldserver, search_result, "objectGUID");
		if (values) {
			if (values[0]) {
				CtdlLogPrintf(CTDL_DEBUG, "objectGUID = (%d characers)\n", strlen(values[0]));
			}
			ldap_value_free(values);
		}

	}

	/* free the results */
	ldap_msgfree(search_result);

	/* unbind so we can go back in as the authenticating user */
	ldap_unbind(ldserver);

	if (!user_dn) {
		CtdlLogPrintf(CTDL_DEBUG, "No such user was found.\n");
		return(4);
	}

	safestrncpy(found_dn, user_dn, found_dn_size);
	ldap_memfree(user_dn);
	return(0);
}


int CtdlTryPasswordLDAP(char *user_dn, char *password)
{
	LDAP *ldserver = NULL;
	int i;

	ldserver = ldap_init(CTDL_LDAP_HOST, CTDL_LDAP_PORT);
	if (ldserver) {
		ldap_set_option(ldserver, LDAP_OPT_PROTOCOL_VERSION, &ldap_version);
		i = ldap_simple_bind_s(ldserver, user_dn, password);
		if (i == LDAP_SUCCESS) {
			CtdlLogPrintf(CTDL_DEBUG, "LDAP: bind succeeded\n");
		}
		else {
			CtdlLogPrintf(CTDL_DEBUG, "LDAP: Cannot bind: %s (%d)\n", ldap_err2string(i), i);
		}
		ldap_unbind(ldserver);
	}

	return((i == LDAP_SUCCESS) ? 0 : 1);
}




#endif /* HAVE_LDAP */
