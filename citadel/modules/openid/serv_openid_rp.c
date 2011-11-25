/*
 * This is an implementation of OpenID 2.0 RELYING PARTY SUPPORT CURRENTLY B0RKEN AND BEING DEVEL0PZ0RED
 *
 * Copyright (c) 2007-2011 by the citadel.org team
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
	StrBuf *claimed_id;
	StrBuf *server;
	int verified;
	HashList *sreg_keys;
} ctdl_openid;

void Free_ctdl_openid(ctdl_openid **FreeMe)
{
	if (*FreeMe == NULL)
		return;
	FreeStrBuf(&(*FreeMe)->claimed_id);
	FreeStrBuf(&(*FreeMe)->server);
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
 * Attempt to register (populate the vCard) the currently-logged-in user
 * using the data from Simple Registration Extension, if present.
 */
void populate_vcard_from_sreg(HashList *sreg_keys) {

	struct vCard *v;
	int pop = 0;			/* number of fields populated */
	char *data = NULL;
	char *postcode = NULL;
	char *country = NULL;

	if (!sreg_keys) return;
	v = vcard_new();
	if (!v) return;

	if (GetHash(sreg_keys, "identity", 8, (void *) &data)) {
		vcard_add_prop(v, "url;type=openid", data);
		++pop;
	}

	if (GetHash(sreg_keys, "sreg.email", 10, (void *) &data)) {
		vcard_add_prop(v, "email;internet", data);
		++pop;
	}

	if (GetHash(sreg_keys, "sreg.nickname", 13, (void *) &data)) {
		vcard_add_prop(v, "nickname", data);
		++pop;
	}

	if (GetHash(sreg_keys, "sreg.fullname", 13, (void *) &data)) {
		char n[256];
		vcard_add_prop(v, "fn", data);
		vcard_fn_to_n(n, data, sizeof n);
		vcard_add_prop(v, "n", n);
		++pop;
	}

	if (!GetHash(sreg_keys, "sreg.postcode", 13, (void *) &postcode)) {
		postcode = NULL;
	}

	if (!GetHash(sreg_keys, "sreg.country", 12, (void *) &country)) {
		country = NULL;
	}

	if (postcode || country) {
		char adr[256];
		snprintf(adr, sizeof adr, ";;;;;%s;%s",
			(postcode ? postcode : ""),
			(country ? country : "")
		);
		vcard_add_prop(v, "adr", adr);
		++pop;
	}

	if (GetHash(sreg_keys, "sreg.dob", 8, (void *) &data)) {
		vcard_add_prop(v, "bday", data);
		++pop;
	}

	if (GetHash(sreg_keys, "sreg.gender", 11, (void *) &data)) {
		vcard_add_prop(v, "x-funambol-gender", data);
		++pop;
	}

	/* Only save the vCard if there is some useful data in it */
	if (pop > 0) {
		char *ser;
		ser = vcard_serialize(v);
		if (ser) {
			CtdlWriteObject(USERCONFIGROOM,	"text/x-vcard",
				ser, strlen(ser)+1, &CC->user, 0, 0, 0
			);
			free(ser);
		}
	}
	vcard_free(v);
}


/*
 * Create a new user account, manually specifying the name, after successfully
 * verifying an OpenID (which will of course be attached to the account)
 */
void cmd_oidc(char *argbuf) {
	ctdl_openid *oiddata = (ctdl_openid *) CC->openid_data;

	if (!oiddata) {
		cprintf("%d You have not verified an OpenID yet.\n", ERROR);
		return;
	}

	if (!oiddata->verified) {
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
		if (oiddata->sreg_keys != NULL) {
			populate_vcard_from_sreg(oiddata->sreg_keys);
		}
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
 * Attempt to auto-create a new Citadel account using the nickname from Simple Registration Extension
 */
int openid_create_user_via_sreg(StrBuf *claimed_id, HashList *sreg_keys)
{
	char *desired_name = NULL;
	char new_password[32];
	long len;

	if (config.c_auth_mode != AUTHMODE_NATIVE) return(1);
	if (config.c_disable_newu) return(2);
	if (CC->logged_in) return(3);
	if (!GetHash(sreg_keys, "sreg.nickname", 13, (void *) &desired_name)) return(4);

	syslog(LOG_DEBUG, "The desired account name is <%s>", desired_name);

	len = cutuserkey(desired_name);
	if (!CtdlGetUser(&CC->user, desired_name)) {
		syslog(LOG_DEBUG, "<%s> is already taken by another user.", desired_name);
		memset(&CC->user, 0, sizeof(struct ctdluser));
		return(5);
	}

	/* The desired account name is available.  Create the account and log it in! */
	if (create_user(desired_name, len, 1)) return(6);

	snprintf(new_password, sizeof new_password, "%08lx%08lx", random(), random());
	CtdlSetPassword(new_password);
	attach_openid(&CC->user, claimed_id);
	populate_vcard_from_sreg(sreg_keys);
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
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 180);		/* die after 180 seconds */

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


/*
 * Begin an HTTP fetch (returns number of bytes actually fetched, or -1 for error) using libcurl.
 */
int fetch_http(StrBuf *url, StrBuf **target_buf)
{
	StrBuf *ReplyBuf;
	CURL *curl;
	CURLcode result;
	char *effective_url = NULL;
	char errmsg[1024] = "";

	if (StrLength(url) <=0 ) return(-1);
	ReplyBuf = *target_buf = NewStrBuf ();
	if (ReplyBuf == 0) return(-1);

	curl = ctdl_openid_curl_easy_init(errmsg);
	if (!curl) return(-1);

	curl_easy_setopt(curl, CURLOPT_URL, ChrPtr(url));
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, ReplyBuf);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlFillStrBuf_callback);

	result = curl_easy_perform(curl);
	if (result) {
		syslog(LOG_DEBUG, "libcurl error %d: %s", result, errmsg);
	}
	curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective_url);
	StrBufPlain(url, effective_url, -1);
	
	curl_easy_cleanup(curl);
	return StrLength(ReplyBuf);
}



struct xrds {
	int nesting_level;
	int in_xrd;
	int current_service_priority;
	int selected_service_priority;	/* more here later */
};


void xrds_xml_start(void *data, const char *supplied_el, const char **attr) {
	struct xrds *xrds = (struct xrds *) data;
	int i;

	++xrds->nesting_level;

	if (!strcasecmp(supplied_el, "XRD")) {
		++xrds->in_xrd;
		syslog(LOG_DEBUG, "*** XRD CONTAINER BEGIN ***");
	}

	else if (!strcasecmp(supplied_el, "service")) {
		xrds->current_service_priority = 0;
		for (i=0; attr[i] != NULL; i+=2) {
			if (!strcasecmp(attr[i], "priority")) {
				xrds->current_service_priority = atoi(attr[i+1]);
			}
		}
	}
}


void xrds_xml_end(void *data, const char *supplied_el) {
	struct xrds *xrds = (struct xrds *) data;

	--xrds->nesting_level;

	if (!strcasecmp(supplied_el, "XRD")) {
		--xrds->in_xrd;
		syslog(LOG_DEBUG, "*** XRD CONTAINER END ***");
	}

	else if (!strcasecmp(supplied_el, "service")) {
		/* this is where we should evaluate the service and do stuff */
		xrds->current_service_priority = 0;
	}
}


void xrds_xml_chardata(void *data, const XML_Char *s, int len) {
	struct xrds *xrds = (struct xrds *) data;

	if (xrds) ;	/* this is only here to silence the warning for now */
	
	/* StrBufAppendBufPlain (xrds->CData, s, len, 0); */
}


/*
 * Parse an XRDS document.
 * If OpenID stuff is discovered, populate FIXME something and return nonzero
 * If nothing useful happened, return 0.
 */
int parse_xrds_document(StrBuf *ReplyBuf) {
	struct xrds xrds;

	memset(&xrds, 0, sizeof (struct xrds));
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

	return(0);	/* FIXME return nonzero if something wonderful happened */
}


/* Attempt to perform Yadis discovery as specified in Yadis 1.0 section 6.2.5.
 * If successful, returns nonzero and calls parse_xrds_document() to act upon the received data.
 * If Yadis fails, returns 0 and does nothing else.
 */
int perform_yadis_discovery(StrBuf *YadisURL) {
	int docbytes = (-1);
	StrBuf *ReplyBuf = NULL;
	int r;
	CURL *curl;
	CURLcode result;
	char errmsg[1024] = "";
	struct curl_slist *my_headers = NULL;

	if (YadisURL == NULL) return(0);
	if (StrLength(YadisURL) == 0) return(0);

	ReplyBuf = NewStrBuf ();
	if (ReplyBuf == 0) return(0);

	curl = ctdl_openid_curl_easy_init(errmsg);
	if (!curl) return(0);

	curl_easy_setopt(curl, CURLOPT_URL, ChrPtr(YadisURL));
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, ReplyBuf);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlFillStrBuf_callback);

	my_headers = curl_slist_append(my_headers, "Accept:");	/* disable the default Accept: header */
	my_headers = curl_slist_append(my_headers, "Accept: application/xrds+xml");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, my_headers);

	result = curl_easy_perform(curl);
	if (result) {
		syslog(LOG_DEBUG, "libcurl error %d: %s", result, errmsg);
	}
	curl_slist_free_all(my_headers);
	curl_easy_cleanup(curl);

	docbytes = StrLength(ReplyBuf);

	/* FIXME here we need to handle Yadis 1.0 section 6.2.5.
	 *
	 * The response from the server will be one of:
	 * 
	 * Option 1: An HTML document with a <head> element that includes a <meta> element with http-equiv
	 * attribute, X-XRDS-Location,
	 * 
	 * Option 2: HTTP response-headers that include an X-XRDS-Location response-header, together with a
	 * document (NOTE: we can probably recurse for this)
	 * 
	 * Option 3:. HTTP response-headers only, which MAY include an X-XRDS-Location response-header,
	 * a contenttype response-header specifying MIME media type, application/xrds+xml, or both.
	 * 
	 */

	/*
	 * Option 4: the returned web page may *be* an XRDS document.  Try to parse it.
	 */
	r = 0;
	if (docbytes >= 0) {
		r = parse_xrds_document(ReplyBuf);
		FreeStrBuf(&ReplyBuf);
	}

	return(r);
}


/*
 * Setup an OpenID authentication
 */
void cmd_oids(char *argbuf) {
	const char *Pos = NULL;
	StrBuf *ArgBuf = NULL;
	StrBuf *ReplyBuf = NULL;
	StrBuf *return_to = NULL;
	StrBuf *trust_root = NULL;
	StrBuf *openid_delegate = NULL;
	StrBuf *RedirectUrl = NULL;
	struct CitContext *CCC = CC;	/* CachedCitContext - performance boost */
	ctdl_openid *oiddata;

	Free_ctdl_openid ((ctdl_openid**)&CCC->openid_data);

	CCC->openid_data = oiddata = malloc(sizeof(ctdl_openid));
	if (oiddata == NULL) {
		cprintf("%d malloc failed\n", ERROR + INTERNAL_ERROR);
		return;
	}
	memset(oiddata, 0, sizeof(ctdl_openid));
	CCC->openid_data = (void *) oiddata;

	ArgBuf = NewStrBufPlain(argbuf, -1);

	oiddata->verified = 0;
	oiddata->claimed_id = NewStrBufPlain(NULL, StrLength(ArgBuf));
	trust_root = NewStrBufPlain(NULL, StrLength(ArgBuf));
	return_to = NewStrBufPlain(NULL, StrLength(ArgBuf));

	StrBufExtract_NextToken(oiddata->claimed_id, ArgBuf, &Pos, '|');
	StrBufExtract_NextToken(return_to, ArgBuf, &Pos, '|');
	StrBufExtract_NextToken(trust_root, ArgBuf, &Pos, '|');

	syslog(LOG_DEBUG, "User-Supplied Identifier is: %s", ChrPtr(oiddata->claimed_id));


	/********** OpenID 2.0 section 7.3 - Discovery **********/

	/* First we're supposed to attempt XRI based resolution.
	 * No one is using this, no one is asking for it, no one wants it.
	 * So we're not even going to bother attempting this mode.
	 */

	/* Second we attempt Yadis.
	 * Google uses this so we'd better do our best to implement it.
	 */
	int yadis_succeeded = perform_yadis_discovery(oiddata->claimed_id);

	/* Third we attempt HTML-based discovery.  Here we go! */
	if (	(yadis_succeeded == 0)
		&& (fetch_http(oiddata->claimed_id, &ReplyBuf) > 0)
		&& (StrLength(ReplyBuf) > 0)
	) {
		openid_delegate = NewStrBuf();
		oiddata->server = NewStrBuf();
		extract_link(oiddata->server, HKEY("openid.server"), ReplyBuf);
		extract_link(openid_delegate, HKEY("openid.delegate"), ReplyBuf);

		if (StrLength(oiddata->server) == 0) {
			cprintf("%d There is no OpenID identity provider at this URL.\n", ERROR);
			FreeStrBuf(&ArgBuf);
			FreeStrBuf(&ReplyBuf);
			FreeStrBuf(&return_to);
			FreeStrBuf(&trust_root);
			FreeStrBuf(&openid_delegate);
			FreeStrBuf(&RedirectUrl);
			return;
		}

		/* Empty delegate is legal; we just use the openid_url instead */
		if (StrLength(openid_delegate) == 0) {
			StrBufPlain(openid_delegate, SKEY(oiddata->claimed_id));
		}

		/* Assemble a URL to which the user-agent will be redirected. */

		RedirectUrl = NewStrBufDup(oiddata->server);

		StrBufAppendBufPlain(RedirectUrl, HKEY("?openid.mode=checkid_setup"
						       "&openid.identity="), 0);
		StrBufUrlescAppend(RedirectUrl, openid_delegate, NULL);

		StrBufAppendBufPlain(RedirectUrl, HKEY("&openid.return_to="), 0);
		StrBufUrlescAppend(RedirectUrl, return_to, NULL);

		StrBufAppendBufPlain(RedirectUrl, HKEY("&openid.trust_root="), 0);
		StrBufUrlescAppend(RedirectUrl, trust_root, NULL);

		StrBufAppendBufPlain(RedirectUrl, HKEY("&openid.sreg.optional="), 0);
		StrBufUrlescAppend(RedirectUrl, NULL, "nickname,email,fullname,postcode,country,dob,gender");

		cprintf("%d %s\n", CIT_OK, ChrPtr(RedirectUrl));
		
		FreeStrBuf(&ArgBuf);
		FreeStrBuf(&ReplyBuf);
		FreeStrBuf(&return_to);
		FreeStrBuf(&trust_root);
		FreeStrBuf(&openid_delegate);
		FreeStrBuf(&RedirectUrl);

		return;
	}

	FreeStrBuf(&ArgBuf);
	FreeStrBuf(&ReplyBuf);
	FreeStrBuf(&return_to);
	FreeStrBuf(&trust_root);
	FreeStrBuf(&openid_delegate);
	FreeStrBuf(&RedirectUrl);

	cprintf("%d Unable to fetch OpenID URL\n", ERROR);
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
	ctdl_openid *oiddata = (ctdl_openid *) CC->openid_data;

	if (oiddata == NULL) {
		cprintf("%d run OIDS first.\n", ERROR + INTERNAL_ERROR);
		return;
	}
	if (StrLength(oiddata->server) == 0){
		cprintf("%d need a remote server to authenticate against\n", ERROR + ILLEGAL_VALUE);
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
		if (len < 0)
			len = sizeof(thiskey) - 1;
		extract_token(thisdata, buf, 1, '|', sizeof thisdata);
		syslog(LOG_DEBUG, "%s: ["SIZE_T_FMT"] %s", thiskey, strlen(thisdata), thisdata);
		Put(keys, thiskey, len, strdup(thisdata), NULL);
	}


	/* Now that we have all of the parameters, we have to validate the signature against the server */
	syslog(LOG_DEBUG, "About to validate the signature...");

	CURL *curl;
	CURLcode res;
	struct curl_httppost *formpost = NULL;
	struct curl_httppost *lastptr = NULL;
	char errmsg[1024] = "";
	char *o_assoc_handle = NULL;
	char *o_sig = NULL;
	char *o_signed = NULL;
	int num_signed_values;
	int i;
	char k_keyname[128];
	char k_o_keyname[128];
	char *k_value = NULL;
	StrBuf *ReplyBuf;

	curl_formadd(&formpost, &lastptr,
		CURLFORM_COPYNAME,	"openid.mode",
		CURLFORM_COPYCONTENTS,	"check_authentication",
		CURLFORM_END);
	syslog(LOG_DEBUG, "%25s : %s", "openid.mode", "check_authentication");

	if (GetHash(keys, "assoc_handle", 12, (void *) &o_assoc_handle)) {
		curl_formadd(&formpost, &lastptr,
			CURLFORM_COPYNAME,	"openid.assoc_handle",
			CURLFORM_COPYCONTENTS,	o_assoc_handle,
			CURLFORM_END);
		syslog(LOG_DEBUG, "%25s : %s", "openid.assoc_handle", o_assoc_handle);
	}

	if (GetHash(keys, "sig", 3, (void *) &o_sig)) {
		curl_formadd(&formpost, &lastptr,
			CURLFORM_COPYNAME,	"openid.sig",
			CURLFORM_COPYCONTENTS,	o_sig,
			CURLFORM_END);
			syslog(LOG_DEBUG, "%25s : %s", "openid.sig", o_sig);
	}

	if (GetHash(keys, "signed", 6, (void *) &o_signed)) {
		curl_formadd(&formpost, &lastptr,
			CURLFORM_COPYNAME,	"openid.signed",
			CURLFORM_COPYCONTENTS,	o_signed,
			CURLFORM_END);
		syslog(LOG_DEBUG, "%25s : %s", "openid.signed", o_signed);

		num_signed_values = num_tokens(o_signed, ',');
		for (i=0; i<num_signed_values; ++i) {
			extract_token(k_keyname, o_signed, i, ',', sizeof k_keyname);
			if (strcasecmp(k_keyname, "mode")) {	// work around phpMyID bug
				if (GetHash(keys, k_keyname, strlen(k_keyname), (void *) &k_value)) {
					snprintf(k_o_keyname, sizeof k_o_keyname, "openid.%s", k_keyname);
					curl_formadd(&formpost, &lastptr,
						CURLFORM_COPYNAME,	k_o_keyname,
						CURLFORM_COPYCONTENTS,	k_value,
						CURLFORM_END);
					syslog(LOG_DEBUG, "%25s : %s", k_o_keyname, k_value);
				}
				else {
					syslog(LOG_INFO, "OpenID: signed field '%s' is missing",
						k_keyname);
				}
			}
		}
	}
	
	ReplyBuf = NewStrBuf();

	curl = ctdl_openid_curl_easy_init(errmsg);
	curl_easy_setopt(curl, CURLOPT_URL, ChrPtr(oiddata->server));
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, ReplyBuf);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlFillStrBuf_callback);
	curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);

	res = curl_easy_perform(curl);
	if (res) {
		syslog(LOG_DEBUG, "cmd_oidf() libcurl error %d: %s", res, errmsg);
	}
	curl_easy_cleanup(curl);
	curl_formfree(formpost);

	if (cbmstrcasestr(ChrPtr(ReplyBuf), "is_valid:true")) {
		oiddata->verified = 1;
	}
	FreeStrBuf(&ReplyBuf);

	syslog(LOG_DEBUG, "Authentication %s.", (oiddata->verified ? "succeeded" : "failed") );

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
			 * New user whose OpenID is verified and Simple Registration Extension is in use?
			 */
			else if (openid_create_user_via_sreg(oiddata->claimed_id, keys) == 0) {
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
				syslog(LOG_DEBUG, "The desired Simple Registration name is already taken.");
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
		curl_global_init(CURL_GLOBAL_ALL);

		/* Only enable the OpenID command set when native mode authentication is in use. */
		if (config.c_auth_mode == AUTHMODE_NATIVE) {
			CtdlRegisterProtoHook(cmd_oids, "OIDS", "Setup OpenID authentication");
			CtdlRegisterProtoHook(cmd_oidf, "OIDF", "Finalize OpenID authentication");
			CtdlRegisterProtoHook(cmd_oidl, "OIDL", "List OpenIDs associated with an account");
			CtdlRegisterProtoHook(cmd_oidd, "OIDD", "Detach an OpenID from an account");
			CtdlRegisterProtoHook(cmd_oidc, "OIDC", "Create new user after validating OpenID");
			CtdlRegisterProtoHook(cmd_oida, "OIDA", "List all OpenIDs in the database");
		}
		CtdlRegisterSessionHook(openid_cleanup_function, EVT_LOGOUT);
		CtdlRegisterUserHook(openid_purge, EVT_PURGEUSER);
		openid_level_supported = 1;	/* This module supports OpenID 1.0 only */
	}

	/* return our module name for the log */
	return "openid_rp";
}
