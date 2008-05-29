/*
 * $Id$
 *
 * This is an implementation of OpenID 1.1 Relying Party support, in stateless mode.
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
#include <curl/curl.h>
#include "ctdl_module.h"
#include "config.h"

struct ctdl_openid {
	char claimed_id[1024];
	char server[1024];
};





/**************************************************************************/
/*                                                                        */
/* Functions in this section handle Citadel internal OpenID mapping stuff */
/*                                                                        */
/**************************************************************************/


/*
 * Attach or detach an OpenID to a Citadel account
 */

enum {
	moa_detach,
	moa_attach
};

int modify_openid_associations(struct ctdluser *who, char *claimed_id, int operation)
{
	if (!who) return(1);
	if (!claimed_id) return(1);
	if (IsEmptyStr(claimed_id)) return(1);

	return(2);		// error because we are not done yet FIXME
}


/*
 * When a user is being deleted, we have to delete any OpenID associations
 */
void openid_purge(struct ctdluser *usbuf) {
	/* FIXME finish this */
}







/**************************************************************************/
/*                                                                        */
/* Functions in this section handle OpenID protocol                       */
/*                                                                        */
/**************************************************************************/


/* 
 * Locate a <link> tag and, given its 'rel=' parameter, return its 'href' parameter
 */
void extract_link(char *target_buf, int target_size, char *rel, char *source_buf)
{
	char *ptr = source_buf;

	if (!target_buf) return;
	if (!rel) return;
	if (!source_buf) return;

	target_buf[0] = 0;

	while (ptr = bmstrcasestr(ptr, "<link"), ptr != NULL) {

		char work_buffer[2048];
		char *link_tag_start = NULL;
		char *link_tag_end = NULL;

		char rel_tag[2048];
		char href_tag[2048];

		link_tag_start = ptr;
		link_tag_end = strchr(ptr, '>');
		rel_tag[0] = 0;
		href_tag[0] = 0;

		if ((link_tag_end) && (link_tag_end > link_tag_start)) {
			int len;
			len = link_tag_end - link_tag_start;
			if (len > sizeof work_buffer) len = sizeof work_buffer;
			memcpy(work_buffer, link_tag_start, len);
		
			char *rel_start = NULL;
			char *rel_end = NULL;
			rel_start = bmstrcasestr(work_buffer, "rel=");
			if (rel_start) {
				rel_start = strchr(rel_start, '\"');
				if (rel_start) {
					++rel_start;
					rel_end = strchr(rel_start, '\"');
					if ((rel_end) && (rel_end > rel_start)) {
						safestrncpy(rel_tag, rel_start, rel_end - rel_start + 1);
					}
				}
			}

			char *href_start = NULL;
			char *href_end = NULL;
			href_start = bmstrcasestr(work_buffer, "href=");
			if (href_start) {
				href_start = strchr(href_start, '\"');
				if (href_start) {
					++href_start;
					href_end = strchr(href_start, '\"');
					if ((href_end) && (href_end > href_start)) {
						safestrncpy(href_tag, href_start, href_end - href_start + 1);
					}
				}
			}

			if (!strcasecmp(rel, rel_tag)) {
				safestrncpy(target_buf, href_tag, target_size);
				return;
			}

		}

	++ptr;
	}


}



struct fh_data {
	char *buf;
	int total_bytes_received;
	int maxbytes;
};


size_t fh_callback(void *ptr, size_t size, size_t nmemb, void *stream)
{
	struct fh_data *fh = (struct fh_data *) stream;
	int got_bytes = (size * nmemb);

	if (fh->total_bytes_received + got_bytes > fh->maxbytes) {
		got_bytes = fh->maxbytes - fh->total_bytes_received;
	}
	if (got_bytes > 0) {
		memcpy(&fh->buf[fh->total_bytes_received], ptr, got_bytes);
		fh->total_bytes_received += got_bytes;
	}

	return (size * nmemb);	/* always succeed; libcurl doesn't need to know if we truncated it */
}



/*
 * Begin an HTTP fetch (returns number of bytes actually fetched, or -1 for error) using libcurl.
 *
 * If 'normalize_len' is nonzero, the caller is specifying the buffer size of 'url', and is
 * requesting that the effective (normalized) URL be copied back to it.
 */
int fetch_http(char *url, char *target_buf, int maxbytes, int normalize_len)
{
	CURL *curl;
	CURLcode res;
	char errmsg[1024] = "";
	struct fh_data fh = {
		target_buf,
		0,
		maxbytes
	};
	char *effective_url = NULL;

	if (!url) return(-1);
	if (!target_buf) return(-1);
	memset(target_buf, 0, maxbytes);

	curl = curl_easy_init();
	if (!curl) {
		CtdlLogPrintf(CTDL_ALERT, "Unable to initialize libcurl.\n");
		return(-1);
	}

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &fh);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fh_callback);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errmsg);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, CITADEL);
	if (!IsEmptyStr(config.c_ip_addr)) {
		curl_easy_setopt(curl, CURLOPT_INTERFACE, config.c_ip_addr);
	}
	res = curl_easy_perform(curl);
	if (res) {
		CtdlLogPrintf(CTDL_DEBUG, "fetch_http() libcurl error %d: %s\n", res, errmsg);
	}
	if (normalize_len > 0) {
		curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective_url);
		safestrncpy(url, effective_url, normalize_len);
	}
	curl_easy_cleanup(curl);
	return fh.total_bytes_received;
}


/*
 * Setup an OpenID authentication
 */
void cmd_oids(char *argbuf) {
	char return_to[1024];
	char trust_root[1024];
	int i;
	char buf[SIZ];
	struct CitContext *CCC = CC;	/* CachedCitContext - performance boost */
	struct ctdl_openid *oiddata;

	/* commented out because we may be attempting to attach an OpenID to
	 * an existing account that is logged in
	 *
	if (CCC->logged_in) {
		cprintf("%d Already logged in.\n", ERROR + ALREADY_LOGGED_IN);
		return;
	}
	 */

	if (CCC->openid_data != NULL) {
		free(CCC->openid_data);
	}
	oiddata = malloc(sizeof(struct ctdl_openid));
	if (oiddata == NULL) {
		cprintf("%d malloc failed\n", ERROR + INTERNAL_ERROR);
		return;
	}
	memset(oiddata, 0, sizeof(struct ctdl_openid));
	CCC->openid_data = (void *) oiddata;

	extract_token(oiddata->claimed_id, argbuf, 0, '|', sizeof oiddata->claimed_id);
	extract_token(return_to, argbuf, 1, '|', sizeof return_to);
	extract_token(trust_root, argbuf, 2, '|', sizeof trust_root);

	i = fetch_http(oiddata->claimed_id, buf, sizeof buf - 1, sizeof oiddata->claimed_id);
	CtdlLogPrintf(CTDL_DEBUG, "Normalized URL and Claimed ID is: %s\n", oiddata->claimed_id);
	buf[sizeof buf - 1] = 0;
	if (i > 0) {
		char openid_delegate[1024];

		extract_link(oiddata->server, sizeof oiddata->server, "openid.server", buf);
		extract_link(openid_delegate, sizeof openid_delegate, "openid.delegate", buf);

		if (IsEmptyStr(oiddata->server)) {
			cprintf("%d There is no OpenID identity provider at this URL.\n", ERROR);
			return;
		}

		/* Empty delegate is legal; we just use the openid_url instead */
		if (IsEmptyStr(openid_delegate)) {
			safestrncpy(openid_delegate, oiddata->claimed_id, sizeof openid_delegate);
		}

		/* Assemble a URL to which the user-agent will be redirected. */
		char redirect_string[4096];
		char escaped_identity[512];
		char escaped_return_to[2048];
		char escaped_trust_root[1024];
		char escaped_sreg_optional[256];

		urlesc(escaped_identity, sizeof escaped_identity, openid_delegate);
		urlesc(escaped_return_to, sizeof escaped_return_to, return_to);
		urlesc(escaped_trust_root, sizeof escaped_trust_root, trust_root);
		urlesc(escaped_sreg_optional, sizeof escaped_sreg_optional,
			"nickname,email,fullname,postcode,country");

		snprintf(redirect_string, sizeof redirect_string,
			"%s"
			"?openid.mode=checkid_setup"
			"&openid.identity=%s"
			"&openid.return_to=%s"
			"&openid.trust_root=%s"
			"&openid.sreg.optional=%s"
			,
			oiddata->server,
			escaped_identity,
			escaped_return_to,
			escaped_trust_root,
			escaped_sreg_optional
		);
		cprintf("%d %s\n", CIT_OK, redirect_string);
		return;
	}

	cprintf("%d Unable to fetch OpenID URL\n", ERROR);
}



/*
 * Callback function to free a pointer (used below in the hash list)
 */
void free_oid_key(void *ptr) {
	free(ptr);
}


/*
 * Finalize an OpenID authentication
 */
void cmd_oidf(char *argbuf) {
	char buf[2048];
	char thiskey[1024];
	char thisdata[1024];
	HashList *keys = NULL;
	HashPos *HashPos;
	struct ctdl_openid *oiddata = (struct ctdl_openid *) CC->openid_data;

	keys = NewHash(1, NULL);
	if (!keys) {
		cprintf("%d NewHash() failed\n", ERROR + INTERNAL_ERROR);
		return;
	}
	
	cprintf("%d Transmit OpenID data now\n", START_CHAT_MODE);

	while (client_getln(buf, sizeof buf), strcmp(buf, "000")) {
		extract_token(thiskey, buf, 0, '|', sizeof thiskey);
		extract_token(thisdata, buf, 1, '|', sizeof thisdata);
		CtdlLogPrintf(CTDL_DEBUG, "%s: [%d] %s\n", thiskey, strlen(thisdata), thisdata);
		Put(keys, thiskey, strlen(thiskey), strdup(thisdata), free_oid_key);
	}


	/* Now that we have all of the parameters, we have to validate the signature against the server */
	CtdlLogPrintf(CTDL_DEBUG, "About to validate the signature...\n");

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
	char valbuf[1024];

	struct fh_data fh = {
		valbuf,
		0,
		sizeof valbuf
	};

	curl_formadd(&formpost, &lastptr,
		CURLFORM_COPYNAME,	"openid.mode",
		CURLFORM_COPYCONTENTS,	"check_authentication",
		CURLFORM_END);
	CtdlLogPrintf(CTDL_DEBUG, "%25s : %s\n", "openid.mode", "check_authentication");

	if (GetHash(keys, "assoc_handle", 12, (void *) &o_assoc_handle)) {
		curl_formadd(&formpost, &lastptr,
			CURLFORM_COPYNAME,	"openid.assoc_handle",
			CURLFORM_COPYCONTENTS,	o_assoc_handle,
			CURLFORM_END);
		CtdlLogPrintf(CTDL_DEBUG, "%25s : %s\n", "openid.assoc_handle", o_assoc_handle);
	}

	if (GetHash(keys, "sig", 3, (void *) &o_sig)) {
		curl_formadd(&formpost, &lastptr,
			CURLFORM_COPYNAME,	"openid.sig",
			CURLFORM_COPYCONTENTS,	o_sig,
			CURLFORM_END);
			CtdlLogPrintf(CTDL_DEBUG, "%25s : %s\n", "openid.sig", o_sig);
	}

	if (GetHash(keys, "signed", 6, (void *) &o_signed)) {
		curl_formadd(&formpost, &lastptr,
			CURLFORM_COPYNAME,	"openid.signed",
			CURLFORM_COPYCONTENTS,	o_signed,
			CURLFORM_END);
		CtdlLogPrintf(CTDL_DEBUG, "%25s : %s\n", "openid.signed", o_signed);

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
					CtdlLogPrintf(CTDL_DEBUG, "%25s : %s\n", k_o_keyname, k_value);
				}
				else {
					CtdlLogPrintf(CTDL_INFO, "OpenID: signed field '%s' is missing\n",
						k_keyname);
				}
			}
		}
	}

	curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_URL, oiddata->server);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &fh);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fh_callback);
	curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errmsg);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, CITADEL);
	if (!IsEmptyStr(config.c_ip_addr)) {
		curl_easy_setopt(curl, CURLOPT_INTERFACE, config.c_ip_addr);
	}

	res = curl_easy_perform(curl);
	if (res) {
		CtdlLogPrintf(CTDL_DEBUG, "cmd_oidf() libcurl error %d: %s\n", res, errmsg);
	}
	curl_easy_cleanup(curl);
	curl_formfree(formpost);

	valbuf[fh.total_bytes_received] = 0;
	int success = 0;

	if (bmstrcasestr(valbuf, "is_valid:true")) {
		success = 1;
	}

	CtdlLogPrintf(CTDL_DEBUG, "Authentication %s.\n", (success ? "succeeded" : "failed") );

	/* Respond to the client */

	if (success) {

		/* If we were already logged in, attach the OpenID to the user's account */
		if (CC->logged_in) {
			if (modify_openid_associations(&CC->user, oiddata->claimed_id, moa_attach) == 0) {
				cprintf("attach\n");
			}
			else {
				cprintf("fail\n");
			}
		}

		/* Otherwise, a user is attempting to log in using the validated OpenID */	
		else {
			cprintf("fail\n");		// FIXME do the login here!!
		}

	}
	else {
		cprintf("fail\n");
	}
	cprintf("000\n");

	/* Free the hash list */
	long len;
	void *Value;
	char *Key;

	HashPos = GetNewHashPos();
	while (GetNextHashPos(keys, HashPos, &len, &Key, &Value)!=0)
	{
		free(Value);
	}
	DeleteHashPos(&HashPos);
}


// mode = [6]  id_res
// identity = [50]  http://uncensored.citadel.org/~ajc/MyID.config.php
// assoc_handle = [26]  6ekac3ju181tgepk7v4h9r7ui7
// return_to = [42]  http://jemcaterers.net/finish_openid_login
// sreg.nickname = [17]  IGnatius T Foobar
// sreg.email = [26]  ajc@uncensored.citadel.org
// sreg.fullname = [10]  Art Cancro
// sreg.postcode = [5]  10549
// sreg.country = [2]  US
// signed = [102]  mode,identity,assoc_handle,return_to,sreg.nickname,sreg.email,sreg.fullname,sreg.postcode,sreg.country
// sig = [28]  vixxxU4MAqWfxxxxCfrHv3TxxxhEw=




/**************************************************************************/
/*                                                                        */
/* Functions in this section handle module initialization and shutdown    */
/*                                                                        */
/**************************************************************************/


/*
 * This cleanup function blows away the temporary memory used by this module.
 */
void openid_cleanup_function(void) {

	if (CC->openid_data != NULL) {
		free(CC->openid_data);
	}
}


CTDL_MODULE_INIT(openid_rp)
{
	if (!threading)
	{
		curl_global_init(CURL_GLOBAL_ALL);
		CtdlRegisterProtoHook(cmd_oids, "OIDS", "Setup OpenID authentication");
		CtdlRegisterProtoHook(cmd_oidf, "OIDF", "Finalize OpenID authentication");
		CtdlRegisterSessionHook(openid_cleanup_function, EVT_STOP);
		CtdlRegisterUserHook(openid_purge, EVT_PURGEUSER);
	}

	/* return our Subversion id for the Log */
	return "$Id$";
}
