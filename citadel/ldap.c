/*
 * These functions implement the portions of AUTHMODE_LDAP and AUTHMODE_LDAP_AD which
 * actually speak to the LDAP server.
 *
 * Copyright (c) 2011-2014 by the citadel.org development team.
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

int ctdl_require_ldap_version = 3;

#define _GNU_SOURCE		// Needed to suppress warning about vasprintf() when running on Linux/Linux
#include <stdio.h>
#include <libcitadel.h>
#include "citserver.h"
#include "citadel_ldap.h"
#include "ctdl_module.h"
#include "user_ops.h"

#ifdef HAVE_LDAP
#define LDAP_DEPRECATED 1 	/* Suppress libldap's warning that we are using deprecated API calls */
#include <ldap.h>



/*
 * Wrapper function for ldap_initialize() that consistently fills in the correct fields
 */
int ctdl_ldap_initialize(LDAP **ld) {

	char server_url[256];
	int ret;

	snprintf(server_url, sizeof server_url, "ldap://%s:%d", config.c_ldap_host, config.c_ldap_port);
	ret = ldap_initialize(ld, server_url);
	if (ret != LDAP_SUCCESS) {
		syslog(LOG_ALERT, "LDAP: Could not connect to %s : %s",
			server_url,
			strerror(errno)
		);
		*ld = NULL;
		return(errno);
	}

	return(ret);
}




/*
 * Look up a user in the directory to see if this is an account that can be authenticated
 */
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
	if (ctdl_ldap_initialize(&ldserver) != LDAP_SUCCESS) {
		return(errno);
	}

	ldap_set_option(ldserver, LDAP_OPT_PROTOCOL_VERSION, &ctdl_require_ldap_version);
	ldap_set_option(ldserver, LDAP_OPT_REFERRALS, (void *)LDAP_OPT_OFF);

	striplt(config.c_ldap_bind_dn);
	striplt(config.c_ldap_bind_pw);
	syslog(LOG_DEBUG, "LDAP bind DN: %s", config.c_ldap_bind_dn);
	i = ldap_simple_bind_s(ldserver,
		(!IsEmptyStr(config.c_ldap_bind_dn) ? config.c_ldap_bind_dn : NULL),
		(!IsEmptyStr(config.c_ldap_bind_pw) ? config.c_ldap_bind_pw : NULL)
	);
	if (i != LDAP_SUCCESS) {
		syslog(LOG_ALERT, "LDAP: Cannot bind: %s (%d)", ldap_err2string(i), i);
		return(i);
	}

	tv.tv_sec = 10;
	tv.tv_usec = 0;

	if (config.c_auth_mode == AUTHMODE_LDAP_AD) {
		snprintf(searchstring, sizeof(searchstring), "(sAMAccountName=%s)", username);
	}
	else {
		snprintf(searchstring, sizeof(searchstring), "(&(objectclass=posixAccount)(uid=%s))", username);
	}

	syslog(LOG_DEBUG, "LDAP search: %s", searchstring);
	(void) ldap_search_ext_s(
		ldserver,					/* ld				*/
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

	/* Ignore the return value of ldap_search_ext_s().  Sometimes it returns an error even when
	 * the search succeeds.  Instead, we check to see whether search_result is still NULL.
	 */
	if (search_result == NULL) {
		syslog(LOG_DEBUG, "LDAP search: zero results were returned");
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
			syslog(LOG_DEBUG, "dn = %s", user_dn);
		}

		if (config.c_auth_mode == AUTHMODE_LDAP_AD) {
			values = ldap_get_values(ldserver, search_result, "displayName");
			if (values) {
				if (values[0]) {
					if (fullname) safestrncpy(fullname, values[0], fullname_size);
					syslog(LOG_DEBUG, "displayName = %s", values[0]);
				}
				ldap_value_free(values);
			}
		}
		else {
			values = ldap_get_values(ldserver, search_result, "cn");
			if (values) {
				if (values[0]) {
					if (fullname) safestrncpy(fullname, values[0], fullname_size);
					syslog(LOG_DEBUG, "cn = %s", values[0]);
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
						syslog(LOG_DEBUG, "uid hashed from objectGUID = %d", *uid);
					}
				}
				ldap_value_free(values);
			}
		}
		else {
			values = ldap_get_values(ldserver, search_result, "uidNumber");
			if (values) {
				if (values[0]) {
					syslog(LOG_DEBUG, "uidNumber = %s", values[0]);
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
		syslog(LOG_DEBUG, "No such user was found.");
		return(4);
	}

	if (found_dn) safestrncpy(found_dn, user_dn, found_dn_size);
	ldap_memfree(user_dn);
	return(0);
}


int CtdlTryPasswordLDAP(char *user_dn, const char *password)
{
	LDAP *ldserver = NULL;
	int i = (-1);

	if (IsEmptyStr(password)) {
		syslog(LOG_DEBUG, "LDAP: empty passwords are not permitted");
		return(1);
	}

	syslog(LOG_DEBUG, "LDAP: trying to bind as %s", user_dn);
	ldserver = ldap_init(config.c_ldap_host, config.c_ldap_port);
	if (ldserver) {
		ldap_set_option(ldserver, LDAP_OPT_PROTOCOL_VERSION, &ctdl_require_ldap_version);
		i = ldap_simple_bind_s(ldserver, user_dn, password);
		if (i == LDAP_SUCCESS) {
			syslog(LOG_DEBUG, "LDAP: bind succeeded");
		}
		else {
			syslog(LOG_DEBUG, "LDAP: Cannot bind: %s (%d)", ldap_err2string(i), i);
		}
		ldap_set_option(ldserver, LDAP_OPT_REFERRALS, (void *)LDAP_OPT_OFF);
		ldap_unbind(ldserver);
	}

	if (i == LDAP_SUCCESS) {
		return(0);
	}

	return(1);
}

//return !0 iff property changed.
int vcard_set_props_iff_different(struct vCard *v,char *propname,int numvals, char **vals) {
	int i;
	char *oldval;
	for(i=0;i<numvals;i++) {
	  oldval = vcard_get_prop(v,propname,0,i,0);
	  if (oldval == NULL) break;
	  if (strcmp(vals[i],oldval)) break;
	}
	if (i!=numvals) {
		for(i=0;i<numvals;i++) vcard_set_prop(v,propname,vals[i],(i==0) ? 0 : 1);
		return 1;
	}
	return 0;
}


//return !0 iff property changed.
int vcard_set_one_prop_iff_different(struct vCard *v,char *propname, char *newfmt, ...) {
	va_list args;
	char *newvalue;
	int changed_something;
	va_start(args,newfmt);
	if (-1==vasprintf(&newvalue,newfmt,args)) {
		syslog(LOG_ALERT, "Out of memory!\n");
		return 0;
	}
	changed_something = vcard_set_props_iff_different(v,propname,1,&newvalue);
	va_end(args);
	free(newvalue);
	return changed_something;
}

/*
 * Learn LDAP attributes and stuff them into the vCard.
 * Returns nonzero if we changed anything.
 */
int Ctdl_LDAP_to_vCard(char *ldap_dn, struct vCard *v)
{
	int changed_something = 0;
	LDAP *ldserver = NULL;
	int i;
	struct timeval tv;
	LDAPMessage *search_result = NULL;
	LDAPMessage *entry = NULL;
	char **givenName;
	char **sn;
	char **cn;
	char **initials;
	char **o;
	char **street;
	char **l;
	char **st;
	char **postalCode;
	char **telephoneNumber;
	char **mobile;
	char **homePhone;
	char **facsimileTelephoneNumber;
	char **mail;
	char **uid;
	char **homeDirectory;
	char **uidNumber;
	char **loginShell;
	char **gidNumber;
	char **c;
	char **title;
	char **uuid;
	char *attrs[] = { "*","+",NULL};

	if (!ldap_dn) return(0);
	if (!v) return(0);
	ldserver = ldap_init(config.c_ldap_host, config.c_ldap_port);
	if (ldserver == NULL) {
		syslog(LOG_ALERT, "LDAP: Could not connect to %s:%d : %s",
			config.c_ldap_host, config.c_ldap_port,
			strerror(errno)
		);
		return(0);
	}

	ldap_set_option(ldserver, LDAP_OPT_PROTOCOL_VERSION, &ctdl_require_ldap_version);
	ldap_set_option(ldserver, LDAP_OPT_REFERRALS, (void *)LDAP_OPT_OFF);

	striplt(config.c_ldap_bind_dn);
	striplt(config.c_ldap_bind_pw);
	syslog(LOG_DEBUG, "LDAP bind DN: %s", config.c_ldap_bind_dn);
	i = ldap_simple_bind_s(ldserver,
		(!IsEmptyStr(config.c_ldap_bind_dn) ? config.c_ldap_bind_dn : NULL),
		(!IsEmptyStr(config.c_ldap_bind_pw) ? config.c_ldap_bind_pw : NULL)
	);
	if (i != LDAP_SUCCESS) {
		syslog(LOG_ALERT, "LDAP: Cannot bind: %s (%d)", ldap_err2string(i), i);
		return(0);
	}

	tv.tv_sec = 10;
	tv.tv_usec = 0;

	syslog(LOG_DEBUG, "LDAP search: %s", ldap_dn);
	(void) ldap_search_ext_s(
		ldserver,				/* ld				*/
		ldap_dn,				/* base				*/
		LDAP_SCOPE_SUBTREE,		/* scope			*/
		NULL,					/* filter			*/
		attrs,					/* attrs (all attributes)	*/
		0,						/* attrsonly (attrs + values)	*/
		NULL,					/* serverctrls (none)		*/
		NULL,					/* clientctrls (none)		*/
		&tv,					/* timeout			*/
		1,						/* sizelimit (1 result max)	*/
		&search_result			/* res				*/
	);
	
	/* Ignore the return value of ldap_search_ext_s().  Sometimes it returns an error even when
	 * the search succeeds.  Instead, we check to see whether search_result is still NULL.
	 */
	 
	if (search_result == NULL) {
		syslog(LOG_DEBUG, "LDAP search: zero results were returned");
		ldap_unbind(ldserver);
		return(0);
	}

	/* At this point we've got at least one result from our query.  If there are multiple
	 * results, we still only look at the first one.
	 */

	entry = ldap_first_entry(ldserver, search_result);
	if (entry) {
		syslog(LOG_DEBUG, "LDAP search, got user details for vcard.");
		givenName=ldap_get_values(ldserver, search_result, "givenName");
		sn=ldap_get_values(ldserver, search_result, "sn");
		cn=ldap_get_values(ldserver, search_result, "cn");
		initials=ldap_get_values(ldserver, search_result, "initials");
		title=ldap_get_values(ldserver, search_result, "title");
		o=ldap_get_values(ldserver, search_result, "o");
		street=ldap_get_values(ldserver, search_result, "street");
		l=ldap_get_values(ldserver, search_result, "l");
		st=ldap_get_values(ldserver, search_result, "st");
		postalCode=ldap_get_values(ldserver, search_result, "postalCode");
		telephoneNumber=ldap_get_values(ldserver, search_result, "telephoneNumber");
		mobile=ldap_get_values(ldserver, search_result, "mobile");
		homePhone=ldap_get_values(ldserver, search_result, "homePhone");
		facsimileTelephoneNumber=ldap_get_values(ldserver, search_result, "facsimileTelephoneNumber");
		mail=ldap_get_values(ldserver, search_result, "mail");
		uid=ldap_get_values(ldserver, search_result, "uid");
		homeDirectory=ldap_get_values(ldserver, search_result, "homeDirectory");
		uidNumber=ldap_get_values(ldserver, search_result, "uidNumber");
		loginShell=ldap_get_values(ldserver, search_result, "loginShell");
		gidNumber=ldap_get_values(ldserver, search_result, "gidNumber");
		c=ldap_get_values(ldserver, search_result, "c");
		uuid=ldap_get_values(ldserver, search_result, "entryUUID");

		if (street && l && st && postalCode && c) changed_something |= vcard_set_one_prop_iff_different(v,"adr",";;%s;%s;%s;%s;%s",street[0],l[0],st[0],postalCode[0],c[0]);
		if (telephoneNumber) changed_something |= vcard_set_one_prop_iff_different(v,"tel;work","%s",telephoneNumber[0]);
		if (facsimileTelephoneNumber) changed_something |= vcard_set_one_prop_iff_different(v,"tel;fax","%s",facsimileTelephoneNumber[0]);
		if (mobile) changed_something |= vcard_set_one_prop_iff_different(v,"tel;cell","%s",mobile[0]);
		if (homePhone) changed_something |= vcard_set_one_prop_iff_different(v,"tel;home","%s",homePhone[0]);
		if (givenName && sn) {
			if (initials) {
				changed_something |= vcard_set_one_prop_iff_different(v,"n","%s;%s;%s",sn[0],givenName[0],initials[0]);
			}
			else {
				changed_something |= vcard_set_one_prop_iff_different(v,"n","%s;%s",sn[0],givenName[0]);
			}
		}
		if (mail) {
			changed_something |= vcard_set_props_iff_different(v,"email;internet",ldap_count_values(mail),mail);
		}
		if (uuid) changed_something |= vcard_set_one_prop_iff_different(v,"uid","%s",uuid[0]);
		if (o) changed_something |= vcard_set_one_prop_iff_different(v,"org","%s",o[0]);
		if (cn) changed_something |= vcard_set_one_prop_iff_different(v,"fn","%s",cn[0]);
		if (title) changed_something |= vcard_set_one_prop_iff_different(v,"title","%s",title[0]);
		
		if (givenName) ldap_value_free(givenName);
		if (initials) ldap_value_free(initials);
		if (sn) ldap_value_free(sn);
		if (cn) ldap_value_free(cn);
		if (o) ldap_value_free(o);
		if (street) ldap_value_free(street);
		if (l) ldap_value_free(l);
		if (st) ldap_value_free(st);
		if (postalCode) ldap_value_free(postalCode);
		if (telephoneNumber) ldap_value_free(telephoneNumber);
		if (mobile) ldap_value_free(mobile);
		if (homePhone) ldap_value_free(homePhone);
		if (facsimileTelephoneNumber) ldap_value_free(facsimileTelephoneNumber);
		if (mail) ldap_value_free(mail);
		if (uid) ldap_value_free(uid);
		if (homeDirectory) ldap_value_free(homeDirectory);
		if (uidNumber) ldap_value_free(uidNumber);
		if (loginShell) ldap_value_free(loginShell);
		if (gidNumber) ldap_value_free(gidNumber);
		if (c) ldap_value_free(c);
		if (title) ldap_value_free(title);
		if (uuid) ldap_value_free(uuid);
	}
	/* free the results */
	ldap_msgfree(search_result);

	/* unbind so we can go back in as the authenticating user */
	ldap_unbind(ldserver);
	
	return(changed_something);	/* tell the caller whether we made any changes */
}

#endif /* HAVE_LDAP */
