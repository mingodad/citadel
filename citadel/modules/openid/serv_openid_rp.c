/*
 * $Id$
 *
 * OpenID 1.1 "relying party" implementation
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


struct associate_handle {
	char claimed_id[256];
	char assoc_type[32];
	time_t expiration_time;
	char assoc_handle[128];
	char mac_key[128];
};

HashList *HL = NULL;		// hash table of assoc_handle

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

		char work_buffer[1024];
		char *link_tag_start = NULL;
		char *link_tag_end = NULL;

		char rel_tag[1024];
		char href_tag[1024];

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

	return got_bytes;
}



/*
 * Begin an HTTP fetch (returns number of bytes actually fetched, or -1 for error)
 * We first try 'curl' or 'wget' because they have more robust HTTP handling, and also
 * support HTTPS.  If neither one works, we fall back to a built in mini HTTP client.
 */
int fetch_http(char *url, char *target_buf, int maxbytes)
{
	CURL *curl;
	CURLcode res;
	char errmsg[1024] = "";
	struct fh_data fh = {
		target_buf,
		0,
		maxbytes
	};

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
	res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	return fh.total_bytes_received;
}


#define ASSOCIATE_RESPONSE_SIZE	4096

/*
 * libcurl callback function for prepare_openid_associate_request()
 */
size_t associate_callback(void *ptr, size_t size, size_t nmemb, void *stream)
{
	char *response = (char *) stream;
	int got_bytes = (size * nmemb);
	int len = strlen(response);

	if ((len + got_bytes + 1) < ASSOCIATE_RESPONSE_SIZE) {
		memcpy(&response[len], ptr, got_bytes);
		response[len+got_bytes] = 0;
	}

	return got_bytes;
}


/*
 * Helper function for process_associate_response()
 * (Delete function for hash table)
 */
void delete_assoc_handle(void *data) {
	if (data) free(data);
}


/*
 * Process the response from an "associate" request
 */
struct associate_handle *process_associate_response(char *claimed_id, char *associate_response)
{
	struct associate_handle *h = NULL;
	char *ptr = associate_response;
	char thisline[256];
	char thiskey[256];
	char thisdata[256];

	h = (struct associate_handle *) malloc(sizeof(struct associate_handle));
	safestrncpy(h->claimed_id, claimed_id, sizeof h->claimed_id);

	do {
		ptr = memreadline(ptr, thisline, sizeof thisline);
		extract_token(thiskey, thisline, 0, ':', sizeof thiskey);
		extract_token(thisdata, thisline, 1, ':', sizeof thisdata);

		if (!strcasecmp(thiskey, "assoc_type")) {
			safestrncpy(h->assoc_type, thisdata, sizeof h->assoc_type);
		}
		else if (!strcasecmp(thiskey, "expires_in")) {
			h->expiration_time = time(NULL) + atol(thisdata);
		}
		else if (!strcasecmp(thiskey, "assoc_handle")) {
			safestrncpy(h->assoc_handle, thisdata, sizeof h->assoc_handle);
		}
		else if (!strcasecmp(thiskey, "mac_key")) {
			safestrncpy(h->mac_key, thisdata, sizeof h->mac_key);
		}

	} while (*ptr);

	/* Add this data structure into the hash table */
	Put(HL, h->assoc_handle, strlen(h->assoc_handle), h, delete_assoc_handle);

	/* FIXME periodically purge the hash table of expired handles */

	return h;
}



/*
 * Establish a shared secret with an OpenID Identity Provider by sending
 * an "associate" request.
 */
struct associate_handle *prepare_openid_associate_request(
		char *claimed_id, char *openid_server, char *openid_delegate)
{
	CURL *curl;
	CURLcode res;
	struct curl_httppost *formpost=NULL;
	struct curl_httppost *lastptr=NULL;
	char associate_response[ASSOCIATE_RESPONSE_SIZE];
	struct associate_handle *h = NULL;

	memset(associate_response, 0, ASSOCIATE_RESPONSE_SIZE);

	curl_formadd(&formpost,
			&lastptr,
			CURLFORM_COPYNAME,	"openid.mode",
			CURLFORM_COPYCONTENTS,	"associate",
			CURLFORM_END
	);

	curl_formadd(&formpost,
			&lastptr,
			CURLFORM_COPYNAME,	"openid.session_type",
			CURLFORM_COPYCONTENTS,	"",
			CURLFORM_END
	);

	curl = curl_easy_init();
	if (curl) {
		curl_easy_setopt(curl, CURLOPT_URL, openid_server);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, associate_response);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, associate_callback);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
			
		curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
		res = curl_easy_perform(curl);
		h = process_associate_response(claimed_id, associate_response);
		curl_easy_cleanup(curl);
	}
	curl_formfree(formpost);

	return h;
}





/*
 * Setup an OpenID authentication
 */
void cmd_oids(char *argbuf) {
	char openid_url[1024];
	char return_to[1024];
	char trust_root[1024];
	int i;
	char buf[SIZ];
	struct associate_handle *h = NULL;

	if (CC->logged_in) {
		cprintf("%d Already logged in.\n", ERROR + ALREADY_LOGGED_IN);
		return;
	}

	extract_token(openid_url, argbuf, 0, '|', sizeof openid_url);
	extract_token(return_to, argbuf, 1, '|', sizeof return_to);
	extract_token(trust_root, argbuf, 2, '|', sizeof trust_root);

	i = fetch_http(openid_url, buf, sizeof buf - 1);
	buf[sizeof buf - 1] = 0;
	if (i > 0) {
		char openid_server[1024];
		char openid_delegate[1024];

		extract_link(openid_server, sizeof openid_server, "openid.server", buf);
		extract_link(openid_delegate, sizeof openid_delegate, "openid.delegate", buf);

		if (IsEmptyStr(openid_server)) {
			cprintf("%d There is no OpenID identity provider at this URL.\n", ERROR);
			return;
		}

		/* Empty delegate is legal; we just use the openid_url instead */
		if (IsEmptyStr(openid_delegate)) {
			safestrncpy(openid_delegate, openid_url, sizeof openid_delegate);
		}

		/*
		 * Prepare an "associate" request.  This contacts the IdP and fetches
		 * a data structure containing an assoc_handle plus a shared secret.
		 */
		h = prepare_openid_associate_request(openid_url, openid_server, openid_delegate);

		/* Assemble a URL to which the user-agent will be redirected. */
		char redirect_string[4096];
		char escaped_identity[1024];
		char escaped_return_to[1024];
		char escaped_trust_root[1024];
		char escaped_sreg_optional[256];
		char escaped_assoc_handle[256];

		urlesc(escaped_identity, sizeof escaped_identity, openid_delegate);
		urlesc(escaped_assoc_handle, sizeof escaped_assoc_handle, h->assoc_handle);
		urlesc(escaped_return_to, sizeof escaped_return_to, return_to);
		urlesc(escaped_trust_root, sizeof escaped_trust_root, trust_root);
		urlesc(escaped_sreg_optional, sizeof escaped_sreg_optional,
			"nickname,email,fullname,postcode,country");

		snprintf(redirect_string, sizeof redirect_string,
			"%s"
			"?openid.mode=checkid_setup"
			"&openid.identity=%s"
			"&openid.assoc_handle=%s"
			"&openid.return_to=%s"
			"&openid.trust_root=%s"
			"&openid.sreg.optional=%s"
			,
			openid_server,
			escaped_identity,
			escaped_assoc_handle,
			escaped_return_to,
			escaped_trust_root,
			escaped_sreg_optional
		);
		CtdlLogPrintf(CTDL_DEBUG, "Telling client about assoc_handle <%s>\n", h->assoc_handle);
		cprintf("%d %s\n", CIT_OK, redirect_string);
		return;
	}

	cprintf("%d Unable to fetch OpenID URL\n", ERROR);
}



/*
 * Finalize an OpenID authentication
 */
void cmd_oidf(char *argbuf) {
	char assoc_handle[256];
	struct associate_handle *h = NULL;

	extract_token(assoc_handle, argbuf, 0, '|', sizeof assoc_handle);

	if (GetHash(HL, assoc_handle, strlen(assoc_handle), (void *)&h)) {
		cprintf("%d handle %s is good\n", CIT_OK, assoc_handle);

		// FIXME now do something with it

	}
	else {
		cprintf("%d handle %s not found\n", ERROR, assoc_handle);
	}
}




CTDL_MODULE_INIT(openid_rp)
{
	if (!threading)
	{
		curl_global_init(CURL_GLOBAL_ALL);
		HL = NewHash(1, NULL);
	        CtdlRegisterProtoHook(cmd_oids, "OIDS", "Setup OpenID authentication");
	        CtdlRegisterProtoHook(cmd_oidf, "OIDF", "Finalize OpenID authentication");
	}

	/* return our Subversion id for the Log */
	return "$Id$";
}
