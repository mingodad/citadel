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
#include "citserver.h"
#include "user_ops.h"

struct ctdl_openid {
	char claimed_id[1024];
	char server[1024];
	int verified;
	HashList *sreg_keys;
};





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
int attach_openid(struct ctdluser *who, char *claimed_id)
{
	struct cdbdata *cdboi;
	long fetched_usernum;
	char *data;
	int data_len;
	char buf[2048];

	if (!who) return(1);
	if (!claimed_id) return(1);
	if (IsEmptyStr(claimed_id)) return(1);

	/* Check to see if this OpenID is already in the database */

	cdboi = cdb_fetch(CDB_OPENID, claimed_id, strlen(claimed_id));
	if (cdboi != NULL) {
		memcpy(&fetched_usernum, cdboi->ptr, sizeof(long));
		cdb_free(cdboi);

		if (fetched_usernum == who->usernum) {
			CtdlLogPrintf(CTDL_INFO, "%s already associated; no action is taken\n", claimed_id);
			return(0);
		}
		else {
			CtdlLogPrintf(CTDL_INFO, "%s already belongs to another user\n", claimed_id);
			return(3);
		}
	}

	/* Not already in the database, so attach it now */

	data_len = sizeof(long) + strlen(claimed_id) + 1;
	data = malloc(data_len);

	memcpy(data, &who->usernum, sizeof(long));
	memcpy(&data[sizeof(long)], claimed_id, strlen(claimed_id) + 1);

	cdb_store(CDB_OPENID, claimed_id, strlen(claimed_id), data, data_len);
	free(data);

	snprintf(buf, sizeof buf, "User <%s> (#%ld) has claimed the OpenID URL %s\n",
		who->fullname, who->usernum, claimed_id);
	aide_message(buf, "OpenID claim");
	CtdlLogPrintf(CTDL_INFO, "%s", buf);
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

	keys = NewHash(1, NULL);
	if (!keys) return;


	cdb_rewind(CDB_OPENID);
	while (cdboi = cdb_next_item(CDB_OPENID), cdboi != NULL) {
		if (cdboi->len > sizeof(long)) {
			if (((long)*(cdboi->ptr)) == usbuf->usernum) {
				deleteme = strdup(cdboi->ptr + sizeof(long)),
				Put(keys, deleteme, strlen(deleteme), deleteme, generic_free_handler);
			}
		}
		cdb_free(cdboi);
	}

	/* Go through the hash list, deleting keys we stored in it */

	HashPos = GetNewHashPos(keys, 0);
	while (GetNextHashPos(keys, HashPos, &len, &Key, &Value)!=0)
	{
		CtdlLogPrintf(CTDL_DEBUG, "Deleting associated OpenID <%s>\n", Value);
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

	if (CtdlAccessCheck(ac_logged_in)) return;
	cdb_rewind(CDB_OPENID);
	cprintf("%d Associated OpenIDs:\n", LISTING_FOLLOWS);

	while (cdboi = cdb_next_item(CDB_OPENID), cdboi != NULL) {
		if (cdboi->len > sizeof(long)) {
			if (((long)*(cdboi->ptr)) == CC->user.usernum) {
				cprintf("%s\n", cdboi->ptr + sizeof(long));
			}
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
	struct ctdl_openid *oiddata = (struct ctdl_openid *) CC->openid_data;

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

	if (CtdlAccessCheck(ac_logged_in)) return;
	extract_token(id_to_detach, argbuf, 0, '|', sizeof id_to_detach);
	if (IsEmptyStr(id_to_detach)) {
		cprintf("%d An empty OpenID URL is not allowed.\n", ERROR + ILLEGAL_VALUE);
	}

	cdb_rewind(CDB_OPENID);
	while (cdboi = cdb_next_item(CDB_OPENID), cdboi != NULL) {
		if (cdboi->len > sizeof(long)) {
			if (((long)*(cdboi->ptr)) == CC->user.usernum) {
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
int openid_create_user_via_sreg(char *claimed_id, HashList *sreg_keys)
{
	char *desired_name = NULL;
	char new_password[32];

	if (config.c_auth_mode != AUTHMODE_NATIVE) return(1);
	if (config.c_disable_newu) return(2);
	if (CC->logged_in) return(3);
	if (!GetHash(sreg_keys, "sreg.nickname", 13, (void *) &desired_name)) return(4);

	CtdlLogPrintf(CTDL_DEBUG, "The desired account name is <%s>\n", desired_name);

	if (!getuser(&CC->user, desired_name)) {
		CtdlLogPrintf(CTDL_DEBUG, "<%s> is already taken by another user.\n", desired_name);
		memset(&CC->user, 0, sizeof(struct ctdluser));
		return(5);
	}

	/* The desired account name is available.  Create the account and log it in! */
	if (create_user(desired_name, 1)) return(6);

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
int login_via_openid(char *claimed_id)
{
	struct cdbdata *cdboi;
	long usernum = 0;

	cdboi = cdb_fetch(CDB_OPENID, claimed_id, strlen(claimed_id));
	if (cdboi == NULL) {
		return(-1);
	}

	memcpy(&usernum, cdboi->ptr, sizeof(long));
	cdb_free(cdboi);

	if (!getuserbynumber(&CC->user, usernum)) {
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
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 180);		/* die after 180 seconds */
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

	oiddata = (struct ctdl_openid *) CCC->openid_data;

	if (oiddata != NULL) {
		if (oiddata->sreg_keys != NULL) {
			DeleteHash(&oiddata->sreg_keys);
			oiddata->sreg_keys = NULL;
		}
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
	oiddata->verified = 0;

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
			"nickname,email,fullname,postcode,country,dob,gender");

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
 * Finalize an OpenID authentication
 */
void cmd_oidf(char *argbuf) {
	char buf[2048];
	char thiskey[1024];
	char thisdata[1024];
	HashList *keys = NULL;
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
		Put(keys, thiskey, strlen(thiskey), strdup(thisdata), generic_free_handler);
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
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 180);		/* die after 180 seconds */
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

	if (bmstrcasestr(valbuf, "is_valid:true")) {
		oiddata->verified = 1;
	}

	CtdlLogPrintf(CTDL_DEBUG, "Authentication %s.\n", (oiddata->verified ? "succeeded" : "failed") );

	/* Respond to the client */

	if (oiddata->verified) {

		/* If we were already logged in, attach the OpenID to the user's account */
		if (CC->logged_in) {
			if (attach_openid(&CC->user, oiddata->claimed_id) == 0) {
				cprintf("attach\n");
			}
			else {
				cprintf("fail\n");
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
			}

			/*
			 * If this system does not allow self-service new user registration, the
			 * remaining modes do not apply, so fail here and now.
			 */
			else if (config.c_disable_newu) {
				cprintf("fail\n");
			}

			/*
			 * New user whose OpenID is verified and Simple Registration Extension is in use?
			 */
			else if (openid_create_user_via_sreg(oiddata->claimed_id, keys) == 0) {
				cprintf("authenticate\n%s\n%s\n", CC->user.fullname, CC->user.password);
				logged_in_response();
			}

			/*
			 * OpenID is verified, but the desired username either was not specified or
			 * conflicts with an existing user.  Manual account creation is required.
			 */
			else {
				char *desired_name = NULL;
				cprintf("verify_only\n");
				cprintf("%s\n", oiddata->claimed_id);
				if (GetHash(keys, "sreg.nickname", 13, (void *) &desired_name)) {
					cprintf("%s\n", desired_name);
				}
				else {
					cprintf("\n");
				}
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


/*
 * This cleanup function blows away the temporary memory used by this module.
 */
void openid_cleanup_function(void) {
	struct ctdl_openid *oiddata = (struct ctdl_openid *) CC->openid_data;

	if (oiddata != NULL) {
		CtdlLogPrintf(CTDL_DEBUG, "Clearing OpenID session state\n");
		if (oiddata->sreg_keys != NULL) {
			DeleteHash(&oiddata->sreg_keys);
			oiddata->sreg_keys = NULL;
		}
		free(oiddata);
	}
}


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
		}
		CtdlRegisterSessionHook(openid_cleanup_function, EVT_LOGOUT);
		CtdlRegisterUserHook(openid_purge, EVT_PURGEUSER);
	}

	/* return our Subversion id for the Log */
	return "$Id$";
}
