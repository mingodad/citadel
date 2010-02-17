/*
 * $Id$
 *
 * These functions implement the portions of AUTHMODE_LDAP and AUTHMODE_LDAP_AD which
 * actually speak to the LDAP server.
 *
 * Copyright (c) 2010 by Art Cancro and the citadel.org development team.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

int ctdl_require_ldap_version = 3;

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
#include "ctdl_module.h"

#ifdef HAVE_LDAP
#define LDAP_DEPRECATED 1 	/* Suppress libldap's warning that we are using deprecated API calls */
#include <ldap.h>

int CtdlTryUserLDAP(char *username,
		char *found_dn, int found_dn_size,
		char *fullname, int fullname_size,
		uid_t *uid)
{
	LDAP *ldserver = NULL;
	int i;
	LDAPMessage *search_result = NULL;
	LDAPMessage *entry = NULL;
	char searchstring[1024];
	struct timeval tv;
	char **values;
	char *user_dn = NULL;

	if (fullname) safestrncpy(fullname, username, fullname_size);

	ldserver = ldap_init(config.c_ldap_host, config.c_ldap_port);
	if (ldserver == NULL) {
		CtdlLogPrintf(CTDL_ALERT, "LDAP: Could not connect to %s:%d : %s\n",
			config.c_ldap_host, config.c_ldap_port,
			strerror(errno)
		);
		return(errno);
	}

	ldap_set_option(ldserver, LDAP_OPT_PROTOCOL_VERSION, &ctdl_require_ldap_version);

	striplt(config.c_ldap_bind_dn);
	striplt(config.c_ldap_bind_pw);
	CtdlLogPrintf(CTDL_DEBUG, "LDAP bind DN: %s\n", config.c_ldap_bind_dn);
	i = ldap_simple_bind_s(ldserver,
		(!IsEmptyStr(config.c_ldap_bind_dn) ? config.c_ldap_bind_dn : NULL),
		(!IsEmptyStr(config.c_ldap_bind_pw) ? config.c_ldap_bind_pw : NULL)
	);
	if (i != LDAP_SUCCESS) {
		CtdlLogPrintf(CTDL_ALERT, "LDAP: Cannot bind: %s (%d)\n", ldap_err2string(i), i);
		return(i);
	}

	tv.tv_sec = 10;
	tv.tv_usec = 0;

	if (config.c_auth_mode == AUTHMODE_LDAP_AD) {
		sprintf(searchstring, "(sAMAccountName=%s)", username);
	}
	else {
		sprintf(searchstring, "(&(objectclass=posixAccount)(uid=%s))", username);
	}

	/* Documentation of ldap_search_ext_s() is at http://tinyurl.com/y9c8a8l */
	CtdlLogPrintf(CTDL_DEBUG, "LDAP search: %s\n", searchstring);
	i = ldap_search_ext_s(ldserver,				/* ld				*/
		config.c_ldap_base_dn,				/* base				*/
		LDAP_SCOPE_SUBTREE,				/* scope			*/
		searchstring,					/* filter			*/
		NULL,						/* attrs (all attributes)	*/
		0,						/* attrsonly (attrs + values)	*/
		NULL,						/* serverctrls (none)		*/
		NULL,						/* clientctrls (none)		*/
		&tv,						/* timeout			*/
		1,						/* sizelimit (1 result max)	*/
		&search_result					/* res				*/
	);

#if 0
	/* It appears that this is unnecessary, and returns an error even when the search succeeds? */
	if (i != LDAP_SUCCESS) {
		CtdlLogPrintf(CTDL_DEBUG, "LDAP search failed: %s (%d)\n", ldap_err2string(i), i);
		ldap_unbind(ldserver);
		if (search_result != NULL) {
			/* this should never happen - warning memory leak! */
			CtdlLogPrintf(CTDL_DEBUG, "search returned error but search_result is not null!\n");
		}
		return(i);
	}
#endif

	if (search_result == NULL) {
		CtdlLogPrintf(CTDL_DEBUG, "LDAP search: zero results were returned\n");
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

		if (config.c_auth_mode == AUTHMODE_LDAP_AD) {
			values = ldap_get_values(ldserver, search_result, "displayName");
			if (values) {
				if (values[0]) {
					if (fullname) safestrncpy(fullname, values[0], fullname_size);
					CtdlLogPrintf(CTDL_DEBUG, "displayName = %s\n", values[0]);
				}
				ldap_value_free(values);
			}
		}
		else {
			values = ldap_get_values(ldserver, search_result, "cn");
			if (values) {
				if (values[0]) {
					if (fullname) safestrncpy(fullname, values[0], fullname_size);
					CtdlLogPrintf(CTDL_DEBUG, "cn = %s\n", values[0]);
				}
				ldap_value_free(values);
			}
		}

		if (config.c_auth_mode == AUTHMODE_LDAP_AD) {
			values = ldap_get_values(ldserver, search_result, "objectGUID");
			if (values) {
				if (values[0]) {
					if (uid != NULL) {
						*uid = abs(HashLittle(values[0], strlen(values[0])));
						CtdlLogPrintf(CTDL_DEBUG, "uid hashed from objectGUID = %d\n", *uid);
					}
				}
				ldap_value_free(values);
			}
		}
		else {
			values = ldap_get_values(ldserver, search_result, "uidNumber");
			if (values) {
				if (values[0]) {
					CtdlLogPrintf(CTDL_DEBUG, "uidNumber = %s\n", values[0]);
					if (uid != NULL) {
						*uid = atoi(values[0]);
					}
				}
				ldap_value_free(values);
			}
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

	if (found_dn) safestrncpy(found_dn, user_dn, found_dn_size);
	ldap_memfree(user_dn);
	return(0);
}


int CtdlTryPasswordLDAP(char *user_dn, char *password)
{
	LDAP *ldserver = NULL;
	int i = (-1);

	ldserver = ldap_init(config.c_ldap_host, config.c_ldap_port);
	if (ldserver) {
		ldap_set_option(ldserver, LDAP_OPT_PROTOCOL_VERSION, &ctdl_require_ldap_version);
		i = ldap_simple_bind_s(ldserver, user_dn, password);
		if (i == LDAP_SUCCESS) {
			CtdlLogPrintf(CTDL_DEBUG, "LDAP: bind succeeded\n");
		}
		else {
			CtdlLogPrintf(CTDL_DEBUG, "LDAP: Cannot bind: %s (%d)\n", ldap_err2string(i), i);
		}
		ldap_unbind(ldserver);
	}

	if (i == LDAP_SUCCESS) {
		return(0);
	}

	return(1);
}


/*
 * Learn LDAP attributes and stuff them into the vCard.
 * Returns nonzero if we changed anything.
 */
int Ctdl_LDAP_to_vCard(char *ldap_dn, struct vCard *v)
{
	if (!ldap_dn) return(0);
	if (!v) return(0);

	/*
	 * FIXME LDAPSTUB this is a stub function
	 *
	 * ldap_dn will contain the DN of the user, and v will contain a pointer to
	 * the vCard that needs to be (re-)populated.  Put the requisite LDAP code here.
	 *
	vcard_set_prop(v, "email;internet", xxx, 0);
	return(1);	* return nonzero to tell the caller that we made changes that need to be saved *
	 *
	 */

	return(0);	/* return zero to tell the caller that we didn't make any changes */
}

#endif /* HAVE_LDAP */
