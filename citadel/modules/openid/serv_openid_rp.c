/*
 * This is an implementation of OpenID 2.0 relying party support in stateless mode.
 *
 * Copyright (c) 2007-2012 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
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
#include <curl/curl.h>
#include <expat.h>
#include "ctdl_module.h"
#include "config.h"
#include "citserver.h"
#include "user_ops.h"

typedef struct _ctdl_openid {
	StrBuf *op_url;			/* OpenID Provider Endpoint URL */
	StrBuf *claimed_id;		/* Claimed Identifier */
	int verified;
	HashList *sreg_keys;
} ctdl_openid;

enum {
	openid_disco_none,
	openid_disco_xrds,
	openid_disco_html
};


void Free_ctdl_openid(ctdl_openid **FreeMe)
{
	if (*FreeMe == NULL) {
		return;
	}
	FreeStrBuf(&(*FreeMe)->op_url);
	FreeStrBuf(&(*FreeMe)->claimed_id);
	DeleteHash(&(*FreeMe)->sreg_keys);
	free(*FreeMe);
	*FreeMe = NULL;
}


/*
 * This cleanup function blows away the temporary memory used by this module.
 */
void openid_cleanup_function(void) {
	struct CitContext *CCC = CC;	/* CachedCitContext - performance boost */

	if (CCC->openid_data != NULL) {
		syslog(LOG_DEBUG, "Clearing OpenID session state");
		Free_ctdl_openid((ctdl_openid **) &CCC->openid_data);
	}
}


/**************************************************************************/
/*                                                                        */
/* Functions in this section handle Citadel internal OpenID mapping stuff */
/*                                                                        */
/**************************************************************************/


/*
 * The structure of an openid record *key* is:
 *
 * |--------------claimed_id-------------|
 *     (actual length of claimed id)
 *
 *
 * The structure of an openid record *value* is:
 *
 * |-----user_number----|------------claimed_id---------------|
 *    (sizeof long)          (actual length of claimed id)
 *
 */



/*
 * Attach an OpenID to a Citadel account
 */
int attach_openid(struct ctdluser *who, StrBuf *claimed_id)
{
	struct cdbdata *cdboi;
	long fetched_usernum;
	char *data;
	int data_len;
	char buf[2048];

	if (!who) return(1);
	if (StrLength(claimed_id)==0) return(1);

	/* Check to see if this OpenID is already in the database */

	cdboi = cdb_fetch(CDB_OPENID, ChrPtr(claimed_id), StrLength(claimed_id));
	if (cdboi != NULL) {
		memcpy(&fetched_usernum, cdboi->ptr, sizeof(long));
		cdb_free(cdboi);

		if (fetched_usernum == who->usernum) {
			syslog(LOG_INFO, "%s already associated; no action is taken", ChrPtr(claimed_id));
			return(0);
		}
		else {
			syslog(LOG_INFO, "%s already belongs to another user", ChrPtr(claimed_id));
			return(3);
		}
	}

	/* Not already in the database, so attach it now */

	data_len = sizeof(long) + StrLength(claimed_id) + 1;
	data = malloc(data_len);

	memcpy(data, &who->usernum, sizeof(long));
	memcpy(&data[sizeof(long)], ChrPtr(claimed_id), StrLength(claimed_id) + 1);

	cdb_store(CDB_OPENID, ChrPtr(claimed_id), StrLength(claimed_id), data, data_len);
	free(data);

	snprintf(buf, sizeof buf, "User <%s> (#%ld) has claimed the OpenID URL %s\n",
		 who->fullname, who->usernum, ChrPtr(claimed_id));
	CtdlAideMessage(buf, "OpenID claim");
	syslog(LOG_INFO, "%s", buf);
	return(0);
}



/*
 * When a user is being deleted, we have to delete any OpenID associations
 */
void openid_purge(struct ctdluser *usbuf) {
	struct cdbdata *cdboi;
	HashList *keys = NULL;
	HashPos *HashPos;
	char *deleteme = NULL;
	long len;
	void *Value;
	const char *Key;
	long usernum = 0L;

	keys = NewHash(1, NULL);
	if (!keys) return;

	cdb_rewind(CDB_OPENID);
	while (cdboi = cdb_next_item(CDB_OPENID), cdboi != NULL) {
		if (cdboi->len > sizeof(long)) {
			memcpy(&usernum, cdboi->ptr, sizeof(long));
			if (usernum == usbuf->usernum) {
				deleteme = strdup(cdboi->ptr + sizeof(long)),
				Put(keys, deleteme, strlen(deleteme), deleteme, NULL);
			}
		}
		cdb_free(cdboi);
	}

	/* Go through the hash list, deleting keys we stored in it */

	HashPos = GetNewHashPos(keys, 0);
	while (GetNextHashPos(keys, HashPos, &len, &Key, &Value)!=0)
	{
		syslog(LOG_DEBUG, "Deleting associated OpenID <%s>", (char*)Value);
		cdb_delete(CDB_OPENID, Value, strlen(Value));
		/* note: don't free(Value) -- deleting the hash list will handle this for us */
	}
	DeleteHashPos(&HashPos);
	DeleteHash(&keys);
}


/*
 * List the OpenIDs associated with the currently logged in account
 */
void cmd_oidl(char *argbuf) {
	struct cdbdata *cdboi;
	long usernum = 0L;

	if (CtdlAccessCheck(ac_logged_in)) return;
	cdb_rewind(CDB_OPENID);
	cprintf("%d Associated OpenIDs:\n", LISTING_FOLLOWS);

	while (cdboi = cdb_next_item(CDB_OPENID), cdboi != NULL) {
		if (cdboi->len > sizeof(long)) {
			memcpy(&usernum, cdboi->ptr, sizeof(long));
			if (usernum == CC->user.usernum) {
				cprintf("%s\n", cdboi->ptr + sizeof(long));
			}
		}
		cdb_free(cdboi);
	}
	cprintf("000\n");
}


/*
 * List ALL OpenIDs in the database
 */
void cmd_oida(char *argbuf) {
	struct cdbdata *cdboi;
	long usernum;
	struct ctdluser usbuf;

	if (CtdlAccessCheck(ac_aide)) return;
	cdb_rewind(CDB_OPENID);
	cprintf("%d List of all OpenIDs in the database:\n", LISTING_FOLLOWS);

	while (cdboi = cdb_next_item(CDB_OPENID), cdboi != NULL) {
		if (cdboi->len > sizeof(long)) {
			memcpy(&usernum, cdboi->ptr, sizeof(long));
			if (CtdlGetUserByNumber(&usbuf, usernum) != 0) {
				usbuf.fullname[0] = 0;
			} 
			cprintf("%s|%ld|%s\n",
				cdboi->ptr + sizeof(long),
				usernum,
				usbuf.fullname
			);
		}
		cdb_free(cdboi);
	}
	cprintf("000\n");
}


/*
 * Create a new user account, manually specifying the name, after successfully
 * verifying an OpenID (which will of course be attached to the account)
 */
void cmd_oidc(char *argbuf) {
	ctdl_openid *oiddata = (ctdl_openid *) CC->openid_data;

	if ( (!oiddata) || (!oiddata->verified) ) {
		cprintf("%d You have not verified an OpenID yet.\n", ERROR);
		return;
	}

	/* We can make the semantics of OIDC exactly the same as NEWU, simply
	 * by _calling_ cmd_newu() and letting it run.  Very clever!
	 */
	cmd_newu(argbuf);

	/* Now, if this logged us in, we have to attach the OpenID */
	if (CC->logged_in) {
		attach_openid(&CC->user, oiddata->claimed_id);
	}

}


/*
 * Detach an OpenID from the currently logged in account
 */
void cmd_oidd(char *argbuf) {
	struct cdbdata *cdboi;
	char id_to_detach[1024];
	int this_is_mine = 0;
	long usernum = 0L;

	if (CtdlAccessCheck(ac_logged_in)) return;
	extract_token(id_to_detach, argbuf, 0, '|', sizeof id_to_detach);
	if (IsEmptyStr(id_to_detach)) {
		cprintf("%d An empty OpenID URL is not allowed.\n", ERROR + ILLEGAL_VALUE);
	}

	cdb_rewind(CDB_OPENID);
	while (cdboi = cdb_next_item(CDB_OPENID), cdboi != NULL) {
		if (cdboi->len > sizeof(long)) {
			memcpy(&usernum, cdboi->ptr, sizeof(long));
			if (usernum == CC->user.usernum) {
				this_is_mine = 1;
			}
		}
		cdb_free(cdboi);
	}

	if (!this_is_mine) {
		cprintf("%d That OpenID was not found or not associated with your account.\n",
			ERROR + ILLEGAL_VALUE);
		return;
	}

	cdb_delete(CDB_OPENID, id_to_detach, strlen(id_to_detach));
	cprintf("%d %s detached from your account.\n", CIT_OK, id_to_detach);
}


/*
 * Attempt to auto-create a new Citadel account using the nickname from Attribute Exchange
 */
int openid_create_user_via_ax(StrBuf *claimed_id, HashList *sreg_keys)
{
	char *nickname = NULL;
	char *firstname = NULL;
	char *lastname = NULL;
	char new_password[32];
	long len;
	const char *Key;
	void *Value;

	if (config.c_auth_mode != AUTHMODE_NATIVE) return(1);
	if (config.c_disable_newu) return(2);
	if (CC->logged_in) return(3);

	HashPos *HashPos = GetNewHashPos(sreg_keys, 0);
	while (GetNextHashPos(sreg_keys, HashPos, &len, &Key, &Value) != 0) {
		syslog(LOG_DEBUG, "%s = %s", Key, (char *)Value);

		if (cbmstrcasestr(Key, "value.nickname") != NULL) {
			nickname = (char *)Value;
		}
		else if ( (nickname == NULL) && (cbmstrcasestr(Key, "value.nickname") != NULL)) {
			nickname = (char *)Value;
		}
		else if (cbmstrcasestr(Key, "value.firstname") != NULL) {
			firstname = (char *)Value;
		}
		else if (cbmstrcasestr(Key, "value.lastname") != NULL) {
			lastname = (char *)Value;
		}

	}
	DeleteHashPos(&HashPos);

	if (nickname == NULL) {
		if ((firstname != NULL) || (lastname != NULL)) {
			char fullname[1024] = "";
			if (firstname) strcpy(fullname, firstname);
			if (firstname && lastname) strcat(fullname, " ");
			if (lastname) strcat(fullname, lastname);
			nickname = fullname;
		}
	}

	if (nickname == NULL) {
		return(4);
	}
	syslog(LOG_DEBUG, "The desired account name is <%s>", nickname);

	len = cutuserkey(nickname);
	if (!CtdlGetUser(&CC->user, nickname)) {
		syslog(LOG_DEBUG, "<%s> is already taken by another user.", nickname);
		memset(&CC->user, 0, sizeof(struct ctdluser));
		return(5);
	}

	/* The desired account name is available.  Create the account and log it in! */
	if (create_user(nickname, len, 1)) return(6);

	/* Generate a random password.
	 * The user doesn't care what the password is since he is using OpenID.
	 */
	snprintf(new_password, sizeof new_password, "%08lx%08lx", random(), random());
	CtdlSetPassword(new_password);

	/* Now attach the verified OpenID to this account. */
	attach_openid(&CC->user, claimed_id);

	return(0);
}


/*
 * If a user account exists which is associated with the Claimed ID, log it in and return zero.
 * Otherwise it returns nonzero.
 */
int login_via_openid(StrBuf *claimed_id)
{
	struct cdbdata *cdboi;
	long usernum = 0;

	cdboi = cdb_fetch(CDB_OPENID, ChrPtr(claimed_id), StrLength(claimed_id));
	if (cdboi == NULL) {
		return(-1);
	}

	memcpy(&usernum, cdboi->ptr, sizeof(long));
	cdb_free(cdboi);

	if (!CtdlGetUserByNumber(&CC->user, usernum)) {
		/* Now become the user we just created */
		safestrncpy(CC->curr_user, CC->user.fullname, sizeof CC->curr_user);
		do_login();
		return(0);
	}
	else {
		memset(&CC->user, 0, sizeof(struct ctdluser));
		return(-1);
	}
}




/**************************************************************************/
/*                                                                        */
/* Functions in this section handle OpenID protocol                       */
/*                                                                        */
/**************************************************************************/


/* 
 * Locate a <link> tag and, given its 'rel=' parameter, return its 'href' parameter
 */
void extract_link(StrBuf *target_buf, const char *rel, long repllen, StrBuf *source_buf)
{
	int i;
	const char *ptr;
	const char *href_start = NULL;
	const char *href_end = NULL;
	const char *link_tag_start = NULL;
	const char *link_tag_end = NULL;
	const char *rel_start = NULL;
	const char *rel_end = NULL;

	if (!target_buf) return;
	if (!rel) return;
	if (!source_buf) return;

	ptr = ChrPtr(source_buf);

	FlushStrBuf(target_buf);
	while (ptr = cbmstrcasestr(ptr, "<link"), ptr != NULL) {

		link_tag_start = ptr;
		link_tag_end = strchr(ptr, '>');
		if (link_tag_end == NULL)
			break;
		for (i=0; i < 1; i++ ){
			rel_start = cbmstrcasestr(link_tag_start, "rel=");
			if ((rel_start == NULL) ||
			    (rel_start > link_tag_end)) 
				continue;

			rel_start = strchr(rel_start, '\"');
			if ((rel_start == NULL) ||
			    (rel_start > link_tag_end)) 
				continue;
			++rel_start;
			rel_end = strchr(rel_start, '\"');
			if ((rel_end == NULL) ||
			    (rel_end == rel_start) ||
			    (rel_end >= link_tag_end) ) 
				continue;
			if (strncasecmp(rel, rel_start, repllen)!= 0)
				continue; /* didn't match? never mind... */
			
			href_start = cbmstrcasestr(link_tag_start, "href=");
			if ((href_start == NULL) || 
			    (href_start >= link_tag_end)) 
				continue;
			href_start = strchr(href_start, '\"');
			if ((href_start == NULL) |
			    (href_start >= link_tag_end)) 
				continue;
			++href_start;
			href_end = strchr(href_start, '\"');
			if ((href_end == NULL) || 
			    (href_end == href_start) ||
			    (href_start >= link_tag_end)) 
				continue;
			StrBufPlain(target_buf, href_start, href_end - href_start);
		}
		ptr = link_tag_end;	
	}
}


/*
 * Wrapper for curl_easy_init() that includes the options common to all calls
 * used in this module. 
 */
CURL *ctdl_openid_curl_easy_init(char *errmsg) {
	CURL *curl;

	curl = curl_easy_init();
	if (!curl) {
		return(curl);
	}

	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);

	if (errmsg) {
		curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errmsg);
	}
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
#ifdef CURLOPT_HTTP_CONTENT_DECODING
	curl_easy_setopt(curl, CURLOPT_HTTP_CONTENT_DECODING, 1);
	curl_easy_setopt(curl, CURLOPT_ENCODING, "");
#endif
	curl_easy_setopt(curl, CURLOPT_USERAGENT, CITADEL);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30);		/* die after 30 seconds */

	if (
		(!IsEmptyStr(config.c_ip_addr))
		&& (strcmp(config.c_ip_addr, "*"))
		&& (strcmp(config.c_ip_addr, "::"))
		&& (strcmp(config.c_ip_addr, "0.0.0.0"))
	) {
		curl_easy_setopt(curl, CURLOPT_INTERFACE, config.c_ip_addr);
	}

	return(curl);
}


struct xrds {
	StrBuf *CharData;
	int nesting_level;
	int in_xrd;
	int current_service_priority;
	int selected_service_priority;
	StrBuf *current_service_uri;
	StrBuf *selected_service_uri;
	int current_service_is_oid2auth;
};


void xrds_xml_start(void *data, const char *supplied_el, const char **attr) {
	struct xrds *xrds = (struct xrds *) data;
	int i;

	++xrds->nesting_level;

	if (!strcasecmp(supplied_el, "XRD")) {
		++xrds->in_xrd;
	}

	else if (!strcasecmp(supplied_el, "service")) {
		xrds->current_service_priority = 0;
		xrds->current_service_is_oid2auth = 0;
		for (i=0; attr[i] != NULL; i+=2) {
			if (!strcasecmp(attr[i], "priority")) {
				xrds->current_service_priority = atoi(attr[i+1]);
			}
		}
	}

	FlushStrBuf(xrds->CharData);
}


void xrds_xml_end(void *data, const char *supplied_el) {
	struct xrds *xrds = (struct xrds *) data;

	--xrds->nesting_level;

	if (!strcasecmp(supplied_el, "XRD")) {
		--xrds->in_xrd;
	}

	else if (!strcasecmp(supplied_el, "type")) {
		if (	(xrds->in_xrd)
			&& (!strcasecmp(ChrPtr(xrds->CharData), "http://specs.openid.net/auth/2.0/server"))
		) {
			xrds->current_service_is_oid2auth = 1;
		}
		if (	(xrds->in_xrd)
			&& (!strcasecmp(ChrPtr(xrds->CharData), "http://specs.openid.net/auth/2.0/signon"))
		) {
			xrds->current_service_is_oid2auth = 1;
			/* FIXME in this case, the Claimed ID should be considered immutable */
		}
	}

	else if (!strcasecmp(supplied_el, "uri")) {
		if (xrds->in_xrd) {
			FlushStrBuf(xrds->current_service_uri);
			StrBufAppendBuf(xrds->current_service_uri, xrds->CharData, 0);
		}
	}

	else if (!strcasecmp(supplied_el, "service")) {
		if (	(xrds->in_xrd)
			&& (xrds->current_service_priority < xrds->selected_service_priority)
			&& (xrds->current_service_is_oid2auth)
		) {
			xrds->selected_service_priority = xrds->current_service_priority;
			FlushStrBuf(xrds->selected_service_uri);
			StrBufAppendBuf(xrds->selected_service_uri, xrds->current_service_uri, 0);
		}

	}

	FlushStrBuf(xrds->CharData);
}


void xrds_xml_chardata(void *data, const XML_Char *s, int len) {
	struct xrds *xrds = (struct xrds *) data;

	StrBufAppendBufPlain (xrds->CharData, s, len, 0);
}


/*
 * Parse an XRDS document.
 * If an OpenID Provider URL is discovered, op_url to that value and return nonzero.
 * If nothing useful happened, return 0.
 */
int parse_xrds_document(StrBuf *ReplyBuf) {
	ctdl_openid *oiddata = (ctdl_openid *) CC->openid_data;
	struct xrds xrds;
	int return_value = 0;

	memset(&xrds, 0, sizeof (struct xrds));
	xrds.selected_service_priority = INT_MAX;
	xrds.CharData = NewStrBuf();
	xrds.current_service_uri = NewStrBuf();
	xrds.selected_service_uri = NewStrBuf();
	XML_Parser xp = XML_ParserCreate(NULL);
	if (xp) {
		XML_SetUserData(xp, &xrds);
		XML_SetElementHandler(xp, xrds_xml_start, xrds_xml_end);
		XML_SetCharacterDataHandler(xp, xrds_xml_chardata);
		XML_Parse(xp, ChrPtr(ReplyBuf), StrLength(ReplyBuf), 0);
		XML_Parse(xp, "", 0, 1);
		XML_ParserFree(xp);
	}
	else {
		syslog(LOG_ALERT, "Cannot create XML parser");
	}

	if (xrds.selected_service_priority < INT_MAX) {
		if (oiddata->op_url == NULL) {
			oiddata->op_url = NewStrBuf();
		}
		FlushStrBuf(oiddata->op_url);
		StrBufAppendBuf(oiddata->op_url, xrds.selected_service_uri, 0);
		return_value = openid_disco_xrds;
	}

	FreeStrBuf(&xrds.CharData);
	FreeStrBuf(&xrds.current_service_uri);
	FreeStrBuf(&xrds.selected_service_uri);

	return(return_value);
}


/*
 * Callback function for perform_openid2_discovery()
 * We're interested in the X-XRDS-Location: header.
 */
size_t yadis_headerfunction(void *ptr, size_t size, size_t nmemb, void *userdata) {
	char hdr[1024];
	StrBuf **x_xrds_location = (StrBuf **) userdata;

	memcpy(hdr, ptr, (size*nmemb));
	hdr[size*nmemb] = 0;

	if (!strncasecmp(hdr, "X-XRDS-Location:", 16)) {
		*x_xrds_location = NewStrBufPlain(&hdr[16], ((size*nmemb)-16));
		StrBufTrim(*x_xrds_location);
	}

	return(size * nmemb);
}


/* Attempt to perform Yadis discovery as specified in Yadis 1.0 section 6.2.5.
 * 
 * If Yadis fails, we then attempt HTML discovery using the same document.
 *
 * If successful, returns nonzero and calls parse_xrds_document() to act upon the received data.
 * If fails, returns 0 and does nothing else.
 */
int perform_openid2_discovery(StrBuf *SuppliedURL) {
	ctdl_openid *oiddata = (ctdl_openid *) CC->openid_data;
	int docbytes = (-1);
	StrBuf *ReplyBuf = NULL;
	int return_value = 0;
	CURL *curl;
	CURLcode result;
	char errmsg[1024] = "";
	struct curl_slist *my_headers = NULL;
	StrBuf *x_xrds_location = NULL;

	if (!SuppliedURL) return(0);
	syslog(LOG_DEBUG, "perform_openid2_discovery(%s)", ChrPtr(SuppliedURL));
	if (StrLength(SuppliedURL) == 0) return(0);

	ReplyBuf = NewStrBuf();
	if (!ReplyBuf) return(0);

	curl = ctdl_openid_curl_easy_init(errmsg);
	if (!curl) return(0);

	curl_easy_setopt(curl, CURLOPT_URL, ChrPtr(SuppliedURL));
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, ReplyBuf);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlFillStrBuf_callback);

	my_headers = curl_slist_append(my_headers, "Accept:");	/* disable the default Accept: header */
	my_headers = curl_slist_append(my_headers, "Accept: application/xrds+xml");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, my_headers);

	curl_easy_setopt(curl, CURLOPT_WRITEHEADER, &x_xrds_location);
	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, yadis_headerfunction);

	result = curl_easy_perform(curl);
	if (result) {
		syslog(LOG_DEBUG, "libcurl error %d: %s", result, errmsg);
	}
	curl_slist_free_all(my_headers);
	curl_easy_cleanup(curl);
	docbytes = StrLength(ReplyBuf);

	/*
	 * The response from the server will be one of:
	 * 
	 * Option 1: An HTML document with a <head> element that includes a <meta> element with http-equiv
	 * attribute, X-XRDS-Location,
	 *
	 * Does any provider actually do this?  If so then we will implement it in the future.
	 */

	/*
	 * Option 2: HTTP response-headers that include an X-XRDS-Location response-header,
	 *           together with a document.
	 * Option 3: HTTP response-headers only, which MAY include an X-XRDS-Location response-header,
	 *           a contenttype response-header specifying MIME media type,
	 *           application/xrds+xml, or both.
	 *
	 * If the X-XRDS-Location header was delivered, we know about it at this point...
	 */
	if (	(x_xrds_location)
		&& (strcmp(ChrPtr(x_xrds_location), ChrPtr(SuppliedURL)))
	) {
		syslog(LOG_DEBUG, "X-XRDS-Location: %s ... recursing!", ChrPtr(x_xrds_location));
		return_value = perform_openid2_discovery(x_xrds_location);
		FreeStrBuf(&x_xrds_location);
	}

	/*
	 * Option 4: the returned web page may *be* an XRDS document.  Try to parse it.
	 */
	if ( (return_value == 0) && (docbytes >= 0)) {
		return_value = parse_xrds_document(ReplyBuf);
	}

	/*
	 * Option 5: if all else fails, attempt HTML based discovery.
	 */
	if ( (return_value == 0) && (docbytes >= 0)) {
		if (oiddata->op_url == NULL) {
			oiddata->op_url = NewStrBuf();
		}
		extract_link(oiddata->op_url, HKEY("openid2.provider"), ReplyBuf);
		if (StrLength(oiddata->op_url) > 0) {
			return_value = openid_disco_html;
		}
	}

	if (ReplyBuf != NULL) {
		FreeStrBuf(&ReplyBuf);
	}
	return(return_value);
}


/*
 * Setup an OpenID authentication
 */
void cmd_oids(char *argbuf) {
	struct CitContext *CCC = CC;	/* CachedCitContext - performance boost */
	const char *Pos = NULL;
	StrBuf *ArgBuf = NULL;
	StrBuf *ReplyBuf = NULL;
	StrBuf *return_to = NULL;
	StrBuf *RedirectUrl = NULL;
	ctdl_openid *oiddata;
	int discovery_succeeded = 0;

	Free_ctdl_openid ((ctdl_openid**)&CCC->openid_data);

	CCC->openid_data = oiddata = malloc(sizeof(ctdl_openid));
	if (oiddata == NULL) {
		syslog(LOG_ALERT, "malloc() failed: %s", strerror(errno));
		cprintf("%d malloc failed\n", ERROR + INTERNAL_ERROR);
		return;
	}
	memset(oiddata, 0, sizeof(ctdl_openid));

	ArgBuf = NewStrBufPlain(argbuf, -1);

	oiddata->verified = 0;
	oiddata->claimed_id = NewStrBufPlain(NULL, StrLength(ArgBuf));
	return_to = NewStrBufPlain(NULL, StrLength(ArgBuf));

	StrBufExtract_NextToken(oiddata->claimed_id, ArgBuf, &Pos, '|');
	StrBufExtract_NextToken(return_to, ArgBuf, &Pos, '|');

	syslog(LOG_DEBUG, "User-Supplied Identifier is: %s", ChrPtr(oiddata->claimed_id));

	/********** OpenID 2.0 section 7.3 - Discovery **********/

	/* Section 7.3.1 says we have to attempt XRI based discovery.
	 * No one is using this, no one is asking for it, no one wants it.
	 * So we're not even going to bother attempting this mode.
	 */

	/* Attempt section 7.3.2 (Yadis discovery) and section 7.3.3 (HTML discovery);
	 */
	discovery_succeeded = perform_openid2_discovery(oiddata->claimed_id);

	if (discovery_succeeded == 0) {
		cprintf("%d There is no OpenID identity provider at this location.\n", ERROR);
	}

	else {
		/*
		 * If we get to this point we are in possession of a valid OpenID Provider URL.
		 */
		syslog(LOG_DEBUG, "OP URI '%s' discovered using method %d",
			ChrPtr(oiddata->op_url),
			discovery_succeeded
		);

		/* We have to "normalize" our Claimed ID otherwise it will cause some OP's to barf */
		if (cbmstrcasestr(ChrPtr(oiddata->claimed_id), "://") == NULL) {
			StrBuf *cid = oiddata->claimed_id;
			oiddata->claimed_id = NewStrBufPlain(HKEY("http://"));
			StrBufAppendBuf(oiddata->claimed_id, cid, 0);
			FreeStrBuf(&cid);
		}

		/*
		 * OpenID 2.0 section 9: request authentication
		 * Assemble a URL to which the user-agent will be redirected.
		 */
	
		RedirectUrl = NewStrBufDup(oiddata->op_url);

		StrBufAppendBufPlain(RedirectUrl, HKEY("?openid.ns="), 0);
		StrBufUrlescAppend(RedirectUrl, NULL, "http://specs.openid.net/auth/2.0");

		StrBufAppendBufPlain(RedirectUrl, HKEY("&openid.mode=checkid_setup"), 0);

		StrBufAppendBufPlain(RedirectUrl, HKEY("&openid.claimed_id="), 0);
		StrBufUrlescAppend(RedirectUrl, oiddata->claimed_id, NULL);

		StrBufAppendBufPlain(RedirectUrl, HKEY("&openid.identity="), 0);
		StrBufUrlescAppend(RedirectUrl, oiddata->claimed_id, NULL);

		/* return_to tells the provider how to complete the round trip back to our site */
		StrBufAppendBufPlain(RedirectUrl, HKEY("&openid.return_to="), 0);
		StrBufUrlescAppend(RedirectUrl, return_to, NULL);

		/* Attribute Exchange
		 * See:
		 *	http://openid.net/specs/openid-attribute-exchange-1_0.html
		 *	http://code.google.com/apis/accounts/docs/OpenID.html#endpoint
		 * 	http://test-id.net/OP/AXFetch.aspx
		 */

		StrBufAppendBufPlain(RedirectUrl, HKEY("&openid.ns.ax="), 0);
		StrBufUrlescAppend(RedirectUrl, NULL, "http://openid.net/srv/ax/1.0");

		StrBufAppendBufPlain(RedirectUrl, HKEY("&openid.ax.mode=fetch_request"), 0);

		StrBufAppendBufPlain(RedirectUrl, HKEY("&openid.ax.required=firstname,lastname,friendly,nickname"), 0);

		StrBufAppendBufPlain(RedirectUrl, HKEY("&openid.ax.type.firstname="), 0);
		StrBufUrlescAppend(RedirectUrl, NULL, "http://axschema.org/namePerson/first");

		StrBufAppendBufPlain(RedirectUrl, HKEY("&openid.ax.type.lastname="), 0);
		StrBufUrlescAppend(RedirectUrl, NULL, "http://axschema.org/namePerson/last");

		StrBufAppendBufPlain(RedirectUrl, HKEY("&openid.ax.type.friendly="), 0);
		StrBufUrlescAppend(RedirectUrl, NULL, "http://axschema.org/namePerson/friendly");

		StrBufAppendBufPlain(RedirectUrl, HKEY("&openid.ax.type.nickname="), 0);
		StrBufUrlescAppend(RedirectUrl, NULL, "http://axschema.org/namePerson/nickname");

		syslog(LOG_DEBUG, "OpenID: redirecting client to %s", ChrPtr(RedirectUrl));
		cprintf("%d %s\n", CIT_OK, ChrPtr(RedirectUrl));
	}
	
	FreeStrBuf(&ArgBuf);
	FreeStrBuf(&ReplyBuf);
	FreeStrBuf(&return_to);
	FreeStrBuf(&RedirectUrl);
}


/*
 * Finalize an OpenID authentication
 */
void cmd_oidf(char *argbuf) {
	long len;
	char buf[2048];
	char thiskey[1024];
	char thisdata[1024];
	HashList *keys = NULL;
	const char *Key;
	void *Value;
	ctdl_openid *oiddata = (ctdl_openid *) CC->openid_data;

	if (oiddata == NULL) {
		cprintf("%d run OIDS first.\n", ERROR + INTERNAL_ERROR);
		return;
	}
	if (StrLength(oiddata->op_url) == 0){
		cprintf("%d No OpenID Endpoint URL has been obtained.\n", ERROR + ILLEGAL_VALUE);
		return;
	}
	keys = NewHash(1, NULL);
	if (!keys) {
		cprintf("%d NewHash() failed\n", ERROR + INTERNAL_ERROR);
		return;
	}
	cprintf("%d Transmit OpenID data now\n", START_CHAT_MODE);

	while (client_getln(buf, sizeof buf), strcmp(buf, "000")) {
		len = extract_token(thiskey, buf, 0, '|', sizeof thiskey);
		if (len < 0) {
			len = sizeof(thiskey) - 1;
		}
		extract_token(thisdata, buf, 1, '|', sizeof thisdata);
		Put(keys, thiskey, len, strdup(thisdata), NULL);
	}

	/* Check to see if this is a correct response.
	 * Start with verified=1 but then set it to 0 if anything looks wrong.
	 */
	oiddata->verified = 1;

	char *openid_ns = NULL;
	if (	(!GetHash(keys, "ns", 2, (void *) &openid_ns))
		|| (strcasecmp(openid_ns, "http://specs.openid.net/auth/2.0"))
	) {
		syslog(LOG_DEBUG, "This is not an an OpenID assertion");
		oiddata->verified = 0;
	}

	char *openid_mode = NULL;
	if (	(!GetHash(keys, "mode", 4, (void *) &openid_mode))
		|| (strcasecmp(openid_mode, "id_res"))
	) {
		oiddata->verified = 0;
	}

	char *openid_claimed_id = NULL;
	if (GetHash(keys, "claimed_id", 10, (void *) &openid_claimed_id)) {
		FreeStrBuf(&oiddata->claimed_id);
		oiddata->claimed_id = NewStrBufPlain(openid_claimed_id, -1);
		syslog(LOG_DEBUG, "Provider is asserting the Claimed ID '%s'", ChrPtr(oiddata->claimed_id));
	}

	/* Validate the assertion against the server */
	syslog(LOG_DEBUG, "Validating...");

	CURL *curl;
	CURLcode res;
	struct curl_httppost *formpost = NULL;
	struct curl_httppost *lastptr = NULL;
	char errmsg[1024] = "";
	StrBuf *ReplyBuf = NewStrBuf();

	curl_formadd(&formpost, &lastptr,
		CURLFORM_COPYNAME,	"openid.mode",
		CURLFORM_COPYCONTENTS,	"check_authentication",
		CURLFORM_END
	);

	HashPos *HashPos = GetNewHashPos(keys, 0);
	while (GetNextHashPos(keys, HashPos, &len, &Key, &Value) != 0) {
		if (strcasecmp(Key, "mode")) {
			char k_o_keyname[1024];
			snprintf(k_o_keyname, sizeof k_o_keyname, "openid.%s", (const char *)Key);
			curl_formadd(&formpost, &lastptr,
				CURLFORM_COPYNAME,	k_o_keyname,
				CURLFORM_COPYCONTENTS,	(char *)Value,
				CURLFORM_END
			);
		}
	}
	DeleteHashPos(&HashPos);

	curl = ctdl_openid_curl_easy_init(errmsg);
	curl_easy_setopt(curl, CURLOPT_URL, ChrPtr(oiddata->op_url));
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, ReplyBuf);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlFillStrBuf_callback);
	curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);

	res = curl_easy_perform(curl);
	if (res) {
		syslog(LOG_DEBUG, "cmd_oidf() libcurl error %d: %s", res, errmsg);
		oiddata->verified = 0;
	}
	curl_easy_cleanup(curl);
	curl_formfree(formpost);

	/* syslog(LOG_DEBUG, "Validation reply: \n%s", ChrPtr(ReplyBuf)); */
	if (cbmstrcasestr(ChrPtr(ReplyBuf), "is_valid:true") == NULL) {
		oiddata->verified = 0;
	}
	FreeStrBuf(&ReplyBuf);

	syslog(LOG_DEBUG, "OpenID authentication %s", (oiddata->verified ? "succeeded" : "failed") );

	/* Respond to the client */

	if (oiddata->verified) {

		/* If we were already logged in, attach the OpenID to the user's account */
		if (CC->logged_in) {
			if (attach_openid(&CC->user, oiddata->claimed_id) == 0) {
				cprintf("attach\n");
				syslog(LOG_DEBUG, "OpenID attach succeeded");
			}
			else {
				cprintf("fail\n");
				syslog(LOG_DEBUG, "OpenID attach failed");
			}
		}

		/* Otherwise, a user is attempting to log in using the verified OpenID */	
		else {
			/*
			 * Existing user who has claimed this OpenID?
			 *
			 * Note: if you think that sending the password back over the wire is insecure,
			 * check your assumptions.  If someone has successfully asserted an OpenID that
			 * is associated with the account, they already have password equivalency and can
			 * login, so they could just as easily change the password, etc.
			 */
			if (login_via_openid(oiddata->claimed_id) == 0) {
				cprintf("authenticate\n%s\n%s\n", CC->user.fullname, CC->user.password);
				logged_in_response();
				syslog(LOG_DEBUG, "Logged in using previously claimed OpenID");
			}

			/*
			 * If this system does not allow self-service new user registration, the
			 * remaining modes do not apply, so fail here and now.
			 */
			else if (config.c_disable_newu) {
				cprintf("fail\n");
				syslog(LOG_DEBUG, "Creating user failed due to local policy");
			}

			/*
			 * New user whose OpenID is verified and Attribute Exchange gave us a name?
			 */
			else if (openid_create_user_via_ax(oiddata->claimed_id, keys) == 0) {
				cprintf("authenticate\n%s\n%s\n", CC->user.fullname, CC->user.password);
				logged_in_response();
				syslog(LOG_DEBUG, "Successfully auto-created new user");
			}

			/*
			 * OpenID is verified, but the desired username either was not specified or
			 * conflicts with an existing user.  Manual account creation is required.
			 */
			else {
				char *desired_name = NULL;
				cprintf("verify_only\n");
				cprintf("%s\n", ChrPtr(oiddata->claimed_id));
				if (GetHash(keys, "sreg.nickname", 13, (void *) &desired_name)) {
					cprintf("%s\n", desired_name);
				}
				else {
					cprintf("\n");
				}
				syslog(LOG_DEBUG, "The desired display name is already taken.");
			}
		}
	}
	else {
		cprintf("fail\n");
	}
	cprintf("000\n");

	if (oiddata->sreg_keys != NULL) {
		DeleteHash(&oiddata->sreg_keys);
		oiddata->sreg_keys = NULL;
	}
	oiddata->sreg_keys = keys;
}



/**************************************************************************/
/*                                                                        */
/* Functions in this section handle module initialization and shutdown    */
/*                                                                        */
/**************************************************************************/




CTDL_MODULE_INIT(openid_rp)
{
	if (!threading) {
// evcurl call this for us. curl_global_init(CURL_GLOBAL_ALL);

		/* Only enable the OpenID command set when native mode authentication is in use. */
		if (config.c_auth_mode == AUTHMODE_NATIVE) {
			CtdlRegisterProtoHook(cmd_oids, "OIDS", "Setup OpenID authentication");
			CtdlRegisterProtoHook(cmd_oidf, "OIDF", "Finalize OpenID authentication");
			CtdlRegisterProtoHook(cmd_oidl, "OIDL", "List OpenIDs associated with an account");
			CtdlRegisterProtoHook(cmd_oidd, "OIDD", "Detach an OpenID from an account");
			CtdlRegisterProtoHook(cmd_oidc, "OIDC", "Create new user after validating OpenID");
			CtdlRegisterProtoHook(cmd_oida, "OIDA", "List all OpenIDs in the database");
		}
		CtdlRegisterSessionHook(openid_cleanup_function, EVT_LOGOUT, PRIO_LOGOUT + 10);
		CtdlRegisterUserHook(openid_purge, EVT_PURGEUSER);
		openid_level_supported = 1;	/* This module supports OpenID 1.0 only */
	}

	/* return our module name for the log */
	return "openid_rp";
}
