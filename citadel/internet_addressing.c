/*
 * This file contains functions which handle the mapping of Internet addresses
 * to users on the Citadel system.
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
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
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "msgbase.h"
#include "internet_addressing.h"
#include "user_ops.h"
#include "room_ops.h"
#include "parsedate.h"
#include "database.h"
#include "ctdl_module.h"
#ifdef HAVE_ICONV
#include <iconv.h>

#if 0
/* This is the non-define version in case of s.b. needing to debug */
inline void FindNextEnd (char *bptr, char *end)
{
	/* Find the next ?Q? */
	end = strchr(bptr + 2, '?');
	if (end == NULL) return NULL;
	if (((*(end + 1) == 'B') || (*(end + 1) == 'Q')) && 
	    (*(end + 2) == '?')) {
		/* skip on to the end of the cluster, the next ?= */
		end = strstr(end + 3, "?=");
	}
	else
		/* sort of half valid encoding, try to find an end. */
		end = strstr(bptr, "?=");
}
#endif

#define FindNextEnd(bptr, end) { \
	end = strchr(bptr + 2, '?'); \
	if (end != NULL) { \
		if (((*(end + 1) == 'B') || (*(end + 1) == 'Q')) && (*(end + 2) == '?')) { \
			end = strstr(end + 3, "?="); \
		} else end = strstr(bptr, "?="); \
	} \
}

/*
 * Handle subjects with RFC2047 encoding such as:
 * =?koi8-r?B?78bP0s3Mxc7JxSDXz9rE1dvO2c3JINvB0sHNySDP?=
 */
void utf8ify_rfc822_string(char *buf) {
	char *start, *end, *next, *nextend, *ptr;
	char newbuf[1024];
	char charset[128];
	char encoding[16];
	char istr[1024];
	iconv_t ic = (iconv_t)(-1) ;
	char *ibuf;			/**< Buffer of characters to be converted */
	char *obuf;			/**< Buffer for converted characters */
	size_t ibuflen;			/**< Length of input buffer */
	size_t obuflen;			/**< Length of output buffer */
	char *isav;			/**< Saved pointer to input buffer */
	char *osav;			/**< Saved pointer to output buffer */
	int passes = 0;
	int i, len, delta;
	int illegal_non_rfc2047_encoding = 0;

	/* Sometimes, badly formed messages contain strings which were simply
	 *  written out directly in some foreign character set instead of
	 *  using RFC2047 encoding.  This is illegal but we will attempt to
	 *  handle it anyway by converting from a user-specified default
	 *  charset to UTF-8 if we see any nonprintable characters.
	 */
	len = strlen(buf);
	for (i=0; i<len; ++i) {
		if ((buf[i] < 32) || (buf[i] > 126)) {
			illegal_non_rfc2047_encoding = 1;
			i = len; ///< take a shortcut, it won't be more than one.
		}
	}
	if (illegal_non_rfc2047_encoding) {
		const char *default_header_charset = "iso-8859-1";
		if ( (strcasecmp(default_header_charset, "UTF-8")) && (strcasecmp(default_header_charset, "us-ascii")) ) {
			ctdl_iconv_open("UTF-8", default_header_charset, &ic);
			if (ic != (iconv_t)(-1) ) {
				ibuf = malloc(1024);
				isav = ibuf;
				safestrncpy(ibuf, buf, 1024);
				ibuflen = strlen(ibuf);
				obuflen = 1024;
				obuf = (char *) malloc(obuflen);
				osav = obuf;
				iconv(ic, &ibuf, &ibuflen, &obuf, &obuflen);
				osav[1024-obuflen] = 0;
				strcpy(buf, osav);
				free(osav);
				iconv_close(ic);
				free(isav);
			}
		}
	}

	/* pre evaluate the first pair */
	nextend = end = NULL;
	len = strlen(buf);
	start = strstr(buf, "=?");
	if (start != NULL) 
		FindNextEnd (start, end);

	while ((start != NULL) && (end != NULL))
	{
		next = strstr(end, "=?");
		if (next != NULL)
			FindNextEnd(next, nextend);
		if (nextend == NULL)
			next = NULL;

		/* did we find two partitions */
		if ((next != NULL) && 
		    ((next - end) > 2))
		{
			ptr = end + 2;
			while ((ptr < next) && 
			       (isspace(*ptr) ||
				(*ptr == '\r') ||
				(*ptr == '\n') || 
				(*ptr == '\t')))
				ptr ++;
			/* did we find a gab just filled with blanks? */
			if (ptr == next)
			{
				memmove (end + 2,
					 next,
					 len - (next - start));

				/* now terminate the gab at the end */
				delta = (next - end) - 2;
				len -= delta;
				buf[len] = '\0';

				/* move next to its new location. */
				next -= delta;
				nextend -= delta;
			}
		}
		/* our next-pair is our new first pair now. */
		start = next;
		end = nextend;
	}

	/* Now we handle foreign character sets properly encoded
	 * in RFC2047 format.
	 */
	start = strstr(buf, "=?");
	FindNextEnd((start != NULL)? start : buf, end);
	while (start != NULL && end != NULL && end > start)
	{
		extract_token(charset, start, 1, '?', sizeof charset);
		extract_token(encoding, start, 2, '?', sizeof encoding);
		extract_token(istr, start, 3, '?', sizeof istr);

		ibuf = malloc(1024);
		isav = ibuf;
		if (!strcasecmp(encoding, "B")) {	/**< base64 */
			ibuflen = CtdlDecodeBase64(ibuf, istr, strlen(istr));
		}
		else if (!strcasecmp(encoding, "Q")) {	/**< quoted-printable */
			size_t len;
			long pos;
			
			len = strlen(istr);
			pos = 0;
			while (pos < len)
			{
				if (istr[pos] == '_') istr[pos] = ' ';
				pos++;
			}

			ibuflen = CtdlDecodeQuotedPrintable(ibuf, istr, len);
		}
		else {
			strcpy(ibuf, istr);		/**< unknown encoding */
			ibuflen = strlen(istr);
		}

		ctdl_iconv_open("UTF-8", charset, &ic);
		if (ic != (iconv_t)(-1) ) {
			obuflen = 1024;
			obuf = (char *) malloc(obuflen);
			osav = obuf;
			iconv(ic, &ibuf, &ibuflen, &obuf, &obuflen);
			osav[1024-obuflen] = 0;

			end = start;
			end++;
			strcpy(start, "");
			remove_token(end, 0, '?');
			remove_token(end, 0, '?');
			remove_token(end, 0, '?');
			remove_token(end, 0, '?');
			strcpy(end, &end[1]);

			snprintf(newbuf, sizeof newbuf, "%s%s%s", buf, osav, end);
			strcpy(buf, newbuf);
			free(osav);
			iconv_close(ic);
		}
		else {
			end = start;
			end++;
			strcpy(start, "");
			remove_token(end, 0, '?');
			remove_token(end, 0, '?');
			remove_token(end, 0, '?');
			remove_token(end, 0, '?');
			strcpy(end, &end[1]);

			snprintf(newbuf, sizeof newbuf, "%s(unreadable)%s", buf, end);
			strcpy(buf, newbuf);
		}

		free(isav);

		/*
		 * Since spammers will go to all sorts of absurd lengths to get their
		 * messages through, there are LOTS of corrupt headers out there.
		 * So, prevent a really badly formed RFC2047 header from throwing
		 * this function into an infinite loop.
		 */
		++passes;
		if (passes > 20) return;

		start = strstr(buf, "=?");
		FindNextEnd((start != NULL)? start : buf, end);
	}

}
#else
inline void utf8ify_rfc822_string(char *a){};

#endif



struct trynamebuf {
	char buffer1[SIZ];
	char buffer2[SIZ];
};

char *inetcfg = NULL;
struct spamstrings_t *spamstrings = NULL;


/*
 * Return nonzero if the supplied name is an alias for this host.
 */
int CtdlHostAlias(char *fqdn) {
	int config_lines;
	int i;
	char buf[256];
	char host[256], type[256];
	int found = 0;

	if (fqdn == NULL) return(hostalias_nomatch);
	if (IsEmptyStr(fqdn)) return(hostalias_nomatch);
	if (!strcasecmp(fqdn, "localhost")) return(hostalias_localhost);
	if (!strcasecmp(fqdn, config.c_fqdn)) return(hostalias_localhost);
	if (!strcasecmp(fqdn, config.c_nodename)) return(hostalias_localhost);
	if (inetcfg == NULL) return(hostalias_nomatch);

	config_lines = num_tokens(inetcfg, '\n');
	for (i=0; i<config_lines; ++i) {
		extract_token(buf, inetcfg, i, '\n', sizeof buf);
		extract_token(host, buf, 0, '|', sizeof host);
		extract_token(type, buf, 1, '|', sizeof type);

		found = 0;

		/* Process these in a specific order, in case there are multiple matches.
		 * We want directory to override masq, for example.
		 */

		if ( (!strcasecmp(type, "masqdomain")) && (!strcasecmp(fqdn, host))) {
			found = hostalias_masq;
		}
		if ( (!strcasecmp(type, "localhost")) && (!strcasecmp(fqdn, host))) {
			found = hostalias_localhost;
		}
		if ( (!strcasecmp(type, "directory")) && (!strcasecmp(fqdn, host))) {
			found = hostalias_directory;
		}

		if (found) return(found);
	}

	return(hostalias_nomatch);
}



/*
 * Determine whether a given Internet address belongs to the current user
 */
int CtdlIsMe(char *addr, int addr_buf_len)
{
	recptypes *recp;
	int i;

	recp = validate_recipients(addr, NULL, 0);
	if (recp == NULL) return(0);

	if (recp->num_local == 0) {
		free_recipients(recp);
		return(0);
	}

	for (i=0; i<recp->num_local; ++i) {
		extract_token(addr, recp->recp_local, i, '|', addr_buf_len);
		if (!strcasecmp(addr, CC->user.fullname)) {
			free_recipients(recp);
			return(1);
		}
	}

	free_recipients(recp);
	return(0);
}


/* If the last item in a list of recipients was truncated to a partial address,
 * remove it completely in order to avoid choking libSieve
 */
void sanitize_truncated_recipient(char *str)
{
	if (!str) return;
	if (num_tokens(str, ',') < 2) return;

	int len = strlen(str);
	if (len < 900) return;
	if (len > 998) str[998] = 0;

	char *cptr = strrchr(str, ',');
	if (!cptr) return;

	char *lptr = strchr(cptr, '<');
	char *rptr = strchr(cptr, '>');

	if ( (lptr) && (rptr) && (rptr > lptr) ) return;

	*cptr = 0;
}





/*
 * This function is self explanatory.
 * (What can I say, I'm in a weird mood today...)
 */
void remove_any_whitespace_to_the_left_or_right_of_at_symbol(char *name)
{
	int i;

	for (i = 0; i < strlen(name); ++i) {
		if (name[i] == '@') {
			while (isspace(name[i - 1]) && i > 0) {
				strcpy(&name[i - 1], &name[i]);
				--i;
			}
			while (isspace(name[i + 1])) {
				strcpy(&name[i + 1], &name[i + 2]);
			}
		}
	}
}


/*
 * Aliasing for network mail.
 * (Error messages have been commented out, because this is a server.)
 */
int alias(char *name)
{				/* process alias and routing info for mail */
	struct CitContext *CCC = CC;
	FILE *fp;
	int a, i;
	char aaa[SIZ], bbb[SIZ];
	char *ignetcfg = NULL;
	char *ignetmap = NULL;
	int at = 0;
	char node[64];
	char testnode[64];
	char buf[SIZ];

	char original_name[256];
	safestrncpy(original_name, name, sizeof original_name);

	striplt(name);
	remove_any_whitespace_to_the_left_or_right_of_at_symbol(name);
	stripallbut(name, '<', '>');

	fp = fopen(file_mail_aliases, "r");
	if (fp == NULL) {
		fp = fopen("/dev/null", "r");
	}
	if (fp == NULL) {
		return (MES_ERROR);
	}
	strcpy(aaa, "");
	strcpy(bbb, "");
	while (fgets(aaa, sizeof aaa, fp) != NULL) {
		while (isspace(name[0]))
			strcpy(name, &name[1]);
		aaa[strlen(aaa) - 1] = 0;
		strcpy(bbb, "");
		for (a = 0; a < strlen(aaa); ++a) {
			if (aaa[a] == ',') {
				strcpy(bbb, &aaa[a + 1]);
				aaa[a] = 0;
			}
		}
		if (!strcasecmp(name, aaa))
			strcpy(name, bbb);
	}
	fclose(fp);

	/* Hit the Global Address Book */
	if (CtdlDirectoryLookup(aaa, name, sizeof aaa) == 0) {
		strcpy(name, aaa);
	}

	if (strcasecmp(original_name, name)) {
		MSG_syslog(LOG_INFO, "%s is being forwarded to %s\n", original_name, name);
	}

	/* Change "user @ xxx" to "user" if xxx is an alias for this host */
	for (a=0; a<strlen(name); ++a) {
		if (name[a] == '@') {
			if (CtdlHostAlias(&name[a+1]) == hostalias_localhost) {
				name[a] = 0;
				MSG_syslog(LOG_INFO, "Changed to <%s>\n", name);
			}
		}
	}

	/* determine local or remote type, see citadel.h */
	at = haschar(name, '@');
	if (at == 0) return(MES_LOCAL);		/* no @'s - local address */
	if (at > 1) return(MES_ERROR);		/* >1 @'s - invalid address */
	remove_any_whitespace_to_the_left_or_right_of_at_symbol(name);

	/* figure out the delivery mode */
	extract_token(node, name, 1, '@', sizeof node);

	/* If there are one or more dots in the nodename, we assume that it
	 * is an FQDN and will attempt SMTP delivery to the Internet.
	 */
	if (haschar(node, '.') > 0) {
		return(MES_INTERNET);
	}

	/* Otherwise we look in the IGnet maps for a valid Citadel node.
	 * Try directly-connected nodes first...
	 */
	ignetcfg = CtdlGetSysConfig(IGNETCFG);
	for (i=0; i<num_tokens(ignetcfg, '\n'); ++i) {
		extract_token(buf, ignetcfg, i, '\n', sizeof buf);
		extract_token(testnode, buf, 0, '|', sizeof testnode);
		if (!strcasecmp(node, testnode)) {
			free(ignetcfg);
			return(MES_IGNET);
		}
	}
	free(ignetcfg);

	/*
	 * Then try nodes that are two or more hops away.
	 */
	ignetmap = CtdlGetSysConfig(IGNETMAP);
	for (i=0; i<num_tokens(ignetmap, '\n'); ++i) {
		extract_token(buf, ignetmap, i, '\n', sizeof buf);
		extract_token(testnode, buf, 0, '|', sizeof testnode);
		if (!strcasecmp(node, testnode)) {
			free(ignetmap);
			return(MES_IGNET);
		}
	}
	free(ignetmap);

	/* If we get to this point it's an invalid node name */
	return (MES_ERROR);
}



/*
 * Validate recipients, count delivery types and errors, and handle aliasing
 * FIXME check for dupes!!!!!
 *
 * Returns 0 if all addresses are ok, ret->num_error = -1 if no addresses 
 * were specified, or the number of addresses found invalid.
 *
 * Caller needs to free the result using free_recipients()
 */
recptypes *validate_recipients(const char *supplied_recipients, 
			       const char *RemoteIdentifier, 
			       int Flags) {
	struct CitContext *CCC = CC;
	recptypes *ret;
	char *recipients = NULL;
	char *org_recp;
	char this_recp[256];
	char this_recp_cooked[256];
	char append[SIZ];
	long len;
	int num_recps = 0;
	int i, j;
	int mailtype;
	int invalid;
	struct ctdluser tempUS;
	struct ctdlroom tempQR;
	struct ctdlroom tempQR2;
	int err = 0;
	char errmsg[SIZ];
	int in_quotes = 0;

	/* Initialize */
	ret = (recptypes *) malloc(sizeof(recptypes));
	if (ret == NULL) return(NULL);

	/* Set all strings to null and numeric values to zero */
	memset(ret, 0, sizeof(recptypes));

	if (supplied_recipients == NULL) {
		recipients = strdup("");
	}
	else {
		recipients = strdup(supplied_recipients);
	}

	/* Allocate some memory.  Yes, this allocates 500% more memory than we will
	 * actually need, but it's healthier for the heap than doing lots of tiny
	 * realloc() calls instead.
	 */
	len = strlen(recipients) + 1024;
	ret->errormsg = malloc(len);
	ret->recp_local = malloc(len);
	ret->recp_internet = malloc(len);
	ret->recp_ignet = malloc(len);
	ret->recp_room = malloc(len);
	ret->display_recp = malloc(len);
	ret->recp_orgroom = malloc(len);
	org_recp = malloc(len);

	ret->errormsg[0] = 0;
	ret->recp_local[0] = 0;
	ret->recp_internet[0] = 0;
	ret->recp_ignet[0] = 0;
	ret->recp_room[0] = 0;
	ret->recp_orgroom[0] = 0;
	ret->display_recp[0] = 0;

	ret->recptypes_magic = RECPTYPES_MAGIC;

	/* Change all valid separator characters to commas */
	for (i=0; !IsEmptyStr(&recipients[i]); ++i) {
		if ((recipients[i] == ';') || (recipients[i] == '|')) {
			recipients[i] = ',';
		}
	}

	/* Now start extracting recipients... */

	while (!IsEmptyStr(recipients)) {
		for (i=0; i<=strlen(recipients); ++i) {
			if (recipients[i] == '\"') in_quotes = 1 - in_quotes;
			if ( ( (recipients[i] == ',') && (!in_quotes) ) || (recipients[i] == 0) ) {
				safestrncpy(this_recp, recipients, i+1);
				this_recp[i] = 0;
				if (recipients[i] == ',') {
					strcpy(recipients, &recipients[i+1]);
				}
				else {
					strcpy(recipients, "");
				}
				break;
			}
		}

		striplt(this_recp);
		if (IsEmptyStr(this_recp))
			break;
		MSG_syslog(LOG_DEBUG, "Evaluating recipient #%d: %s\n", num_recps, this_recp);
		++num_recps;

		strcpy(org_recp, this_recp);
		alias(this_recp);
		alias(this_recp);
		mailtype = alias(this_recp);

		for (j = 0; !IsEmptyStr(&this_recp[j]); ++j) {
			if (this_recp[j]=='_') {
				this_recp_cooked[j] = ' ';
			}
			else {
				this_recp_cooked[j] = this_recp[j];
			}
		}
		this_recp_cooked[j] = '\0';
		invalid = 0;
		errmsg[0] = 0;
		switch(mailtype) {
		case MES_LOCAL:
			if (!strcasecmp(this_recp, "sysop")) {
				++ret->num_room;
				strcpy(this_recp, config.c_aideroom);
				if (!IsEmptyStr(ret->recp_room)) {
					strcat(ret->recp_room, "|");
				}
				strcat(ret->recp_room, this_recp);
			}
			else if ( (!strncasecmp(this_recp, "room_", 5))
				  && (!CtdlGetRoom(&tempQR, &this_recp_cooked[5])) ) {

				/* Save room so we can restore it later */
				tempQR2 = CCC->room;
				CCC->room = tempQR;
					
				/* Check permissions to send mail to this room */
				err = CtdlDoIHavePermissionToPostInThisRoom(
					errmsg, 
					sizeof errmsg, 
					RemoteIdentifier,
					Flags,
					0			/* 0 = not a reply */
					);
				if (err)
				{
					++ret->num_error;
					invalid = 1;
				} 
				else {
					++ret->num_room;
					if (!IsEmptyStr(ret->recp_room)) {
						strcat(ret->recp_room, "|");
					}
					strcat(ret->recp_room, &this_recp_cooked[5]);

					if (!IsEmptyStr(ret->recp_orgroom)) {
						strcat(ret->recp_orgroom, "|");
					}
					strcat(ret->recp_orgroom, org_recp);

				}
					
				/* Restore room in case something needs it */
				CCC->room = tempQR2;

			}
			else if (CtdlGetUser(&tempUS, this_recp) == 0) {
				++ret->num_local;
				strcpy(this_recp, tempUS.fullname);
				if (!IsEmptyStr(ret->recp_local)) {
					strcat(ret->recp_local, "|");
				}
				strcat(ret->recp_local, this_recp);
			}
			else if (CtdlGetUser(&tempUS, this_recp_cooked) == 0) {
				++ret->num_local;
				strcpy(this_recp, tempUS.fullname);
				if (!IsEmptyStr(ret->recp_local)) {
					strcat(ret->recp_local, "|");
				}
				strcat(ret->recp_local, this_recp);
			}
			else {
				++ret->num_error;
				invalid = 1;
			}
			break;
		case MES_INTERNET:
			/* Yes, you're reading this correctly: if the target
			 * domain points back to the local system or an attached
			 * Citadel directory, the address is invalid.  That's
			 * because if the address were valid, we would have
			 * already translated it to a local address by now.
			 */
			if (IsDirectory(this_recp, 0)) {
				++ret->num_error;
				invalid = 1;
			}
			else {
				++ret->num_internet;
				if (!IsEmptyStr(ret->recp_internet)) {
					strcat(ret->recp_internet, "|");
				}
				strcat(ret->recp_internet, this_recp);
			}
			break;
		case MES_IGNET:
			++ret->num_ignet;
			if (!IsEmptyStr(ret->recp_ignet)) {
				strcat(ret->recp_ignet, "|");
			}
			strcat(ret->recp_ignet, this_recp);
			break;
		case MES_ERROR:
			++ret->num_error;
			invalid = 1;
			break;
		}
		if (invalid) {
			if (IsEmptyStr(errmsg)) {
				snprintf(append, sizeof append, "Invalid recipient: %s", this_recp);
			}
			else {
				snprintf(append, sizeof append, "%s", errmsg);
			}
			if ( (strlen(ret->errormsg) + strlen(append) + 3) < SIZ) {
				if (!IsEmptyStr(ret->errormsg)) {
					strcat(ret->errormsg, "; ");
				}
				strcat(ret->errormsg, append);
			}
		}
		else {
			if (IsEmptyStr(ret->display_recp)) {
				strcpy(append, this_recp);
			}
			else {
				snprintf(append, sizeof append, ", %s", this_recp);
			}
			if ( (strlen(ret->display_recp)+strlen(append)) < SIZ) {
				strcat(ret->display_recp, append);
			}
		}
	}
	free(org_recp);

	if ((ret->num_local + ret->num_internet + ret->num_ignet +
	     ret->num_room + ret->num_error) == 0) {
		ret->num_error = (-1);
		strcpy(ret->errormsg, "No recipients specified.");
	}

	MSGM_syslog(LOG_DEBUG, "validate_recipients()\n");
	MSG_syslog(LOG_DEBUG, " local: %d <%s>\n", ret->num_local, ret->recp_local);
	MSG_syslog(LOG_DEBUG, "  room: %d <%s>\n", ret->num_room, ret->recp_room);
	MSG_syslog(LOG_DEBUG, "  inet: %d <%s>\n", ret->num_internet, ret->recp_internet);
	MSG_syslog(LOG_DEBUG, " ignet: %d <%s>\n", ret->num_ignet, ret->recp_ignet);
	MSG_syslog(LOG_DEBUG, " error: %d <%s>\n", ret->num_error, ret->errormsg);

	free(recipients);
	return(ret);
}


/*
 * Destructor for recptypes
 */
void free_recipients(recptypes *valid) {

	if (valid == NULL) {
		return;
	}

	if (valid->recptypes_magic != RECPTYPES_MAGIC) {
		struct CitContext *CCC = CC;
		MSGM_syslog(LOG_EMERG, "Attempt to call free_recipients() on some other data type!\n");
		abort();
	}

	if (valid->errormsg != NULL)		free(valid->errormsg);
	if (valid->recp_local != NULL)		free(valid->recp_local);
	if (valid->recp_internet != NULL)	free(valid->recp_internet);
	if (valid->recp_ignet != NULL)		free(valid->recp_ignet);
	if (valid->recp_room != NULL)		free(valid->recp_room);
	if (valid->recp_orgroom != NULL)        free(valid->recp_orgroom);
	if (valid->display_recp != NULL)	free(valid->display_recp);
	if (valid->bounce_to != NULL)		free(valid->bounce_to);
	if (valid->envelope_from != NULL)	free(valid->envelope_from);
	if (valid->sending_room != NULL)	free(valid->sending_room);
	free(valid);
}


char *qp_encode_email_addrs(char *source)
{
	struct CitContext *CCC = CC;
	char *user, *node, *name;
	const char headerStr[] = "=?UTF-8?Q?";
	char *Encoded;
	char *EncodedName;
	char *nPtr;
	int need_to_encode = 0;
	long SourceLen;
	long EncodedMaxLen;
	long nColons = 0;
	long *AddrPtr;
	long *AddrUtf8;
	long nAddrPtrMax = 50;
	long nmax;
	int InQuotes = 0;
	int i, n;

	if (source == NULL) return source;
	if (IsEmptyStr(source)) return source;
	if (MessageDebugEnabled != 0) cit_backtrace();
	MSG_syslog(LOG_DEBUG, "qp_encode_email_addrs: [%s]\n", source);

	AddrPtr = malloc (sizeof (long) * nAddrPtrMax);
	AddrUtf8 = malloc (sizeof (long) * nAddrPtrMax);
	memset(AddrUtf8, 0, sizeof (long) * nAddrPtrMax);
	*AddrPtr = 0;
	i = 0;
	while (!IsEmptyStr (&source[i])) {
		if (nColons >= nAddrPtrMax){
			long *ptr;

			ptr = (long *) malloc(sizeof (long) * nAddrPtrMax * 2);
			memcpy (ptr, AddrPtr, sizeof (long) * nAddrPtrMax);
			free (AddrPtr), AddrPtr = ptr;

			ptr = (long *) malloc(sizeof (long) * nAddrPtrMax * 2);
			memset(&ptr[nAddrPtrMax], 0, 
			       sizeof (long) * nAddrPtrMax);

			memcpy (ptr, AddrUtf8, sizeof (long) * nAddrPtrMax);
			free (AddrUtf8), AddrUtf8 = ptr;
			nAddrPtrMax *= 2;				
		}
		if (((unsigned char) source[i] < 32) || 
		    ((unsigned char) source[i] > 126)) {
			need_to_encode = 1;
			AddrUtf8[nColons] = 1;
		}
		if (source[i] == '"')
			InQuotes = !InQuotes;
		if (!InQuotes && source[i] == ',') {
			AddrPtr[nColons] = i;
			nColons++;
		}
		i++;
	}
	if (need_to_encode == 0) {
		free(AddrPtr);
		free(AddrUtf8);
		return source;
	}

	SourceLen = i;
	EncodedMaxLen = nColons * (sizeof(headerStr) + 3) + SourceLen * 3;
	Encoded = (char*) malloc (EncodedMaxLen);

	for (i = 0; i < nColons; i++)
		source[AddrPtr[i]++] = '\0';
	/* TODO: if libidn, this might get larger*/
	user = malloc(SourceLen + 1);
	node = malloc(SourceLen + 1);
	name = malloc(SourceLen + 1);

	nPtr = Encoded;
	*nPtr = '\0';
	for (i = 0; i < nColons && nPtr != NULL; i++) {
		nmax = EncodedMaxLen - (nPtr - Encoded);
		if (AddrUtf8[i]) {
			process_rfc822_addr(&source[AddrPtr[i]], 
					    user,
					    node,
					    name);
			/* TODO: libIDN here ! */
			if (IsEmptyStr(name)) {
				n = snprintf(nPtr, nmax, 
					     (i==0)?"%s@%s" : ",%s@%s",
					     user, node);
			}
			else {
				EncodedName = rfc2047encode(name, strlen(name));			
				n = snprintf(nPtr, nmax, 
					     (i==0)?"%s <%s@%s>" : ",%s <%s@%s>",
					     EncodedName, user, node);
				free(EncodedName);
			}
		}
		else { 
			n = snprintf(nPtr, nmax, 
				     (i==0)?"%s" : ",%s",
				     &source[AddrPtr[i]]);
		}
		if (n > 0 )
			nPtr += n;
		else { 
			char *ptr, *nnPtr;
			ptr = (char*) malloc(EncodedMaxLen * 2);
			memcpy(ptr, Encoded, EncodedMaxLen);
			nnPtr = ptr + (nPtr - Encoded), nPtr = nnPtr;
			free(Encoded), Encoded = ptr;
			EncodedMaxLen *= 2;
			i--; /* do it once more with properly lengthened buffer */
		}
	}
	for (i = 0; i < nColons; i++)
		source[--AddrPtr[i]] = ',';

	free(user);
	free(node);
	free(name);
	free(AddrUtf8);
	free(AddrPtr);
	return Encoded;
}


/*
 * Return 0 if a given string fuzzy-matches a Citadel user account
 *
 * FIXME ... this needs to be updated to handle aliases.
 */
int fuzzy_match(struct ctdluser *us, char *matchstring) {
	int a;
	long len;

	if ( (!strncasecmp(matchstring, "cit", 3)) 
	   && (atol(&matchstring[3]) == us->usernum)) {
		return 0;
	}

	len = strlen(matchstring);
	for (a=0; !IsEmptyStr(&us->fullname[a]); ++a) {
		if (!strncasecmp(&us->fullname[a],
		   matchstring, len)) {
			return 0;
		}
	}
	return -1;
}


/*
 * Unfold a multi-line field into a single line, removing multi-whitespaces
 */
void unfold_rfc822_field(char **field, char **FieldEnd) 
{
	int quote = 0;
	char *pField = *field;
	char *sField;
	char *pFieldEnd = *FieldEnd;

	while (isspace(*pField))
		pField++;
	/* remove leading/trailing whitespace */
	;

	while (isspace(*pFieldEnd))
		pFieldEnd --;

	*FieldEnd = pFieldEnd;
	/* convert non-space whitespace to spaces, and remove double blanks */
	for (sField = *field = pField; 
	     sField < pFieldEnd; 
	     pField++, sField++)
	{
		if ((*sField=='\r') || (*sField=='\n'))
		{
			int Offset = 1;
			while (((*(sField + Offset) == '\r') ||
				(*(sField + Offset) == '\n') ||
				(isspace(*(sField + Offset)))) && 
			       (sField + Offset < pFieldEnd))
				Offset ++;
			sField += Offset;
			*pField = *sField;
		}
		else {
			if (*sField=='\"') quote = 1 - quote;
			if (!quote) {
				if (isspace(*sField))
				{
					*pField = ' ';
					pField++;
					sField++;
					
					while ((sField < pFieldEnd) && 
					       isspace(*sField))
						sField++;
					*pField = *sField;
				}
				else *pField = *sField;
			}
			else *pField = *sField;
		}
	}
	*pField = '\0';
	*FieldEnd = pField - 1;
}



/*
 * Split an RFC822-style address into userid, host, and full name
 *
 */
void process_rfc822_addr(const char *rfc822, char *user, char *node, char *name)
{
	int a;

	strcpy(user, "");
	strcpy(node, config.c_fqdn);
	strcpy(name, "");

	if (rfc822 == NULL) return;

	/* extract full name - first, it's From minus <userid> */
	strcpy(name, rfc822);
	stripout(name, '<', '>');

	/* strip anything to the left of a bang */
	while ((!IsEmptyStr(name)) && (haschar(name, '!') > 0))
		strcpy(name, &name[1]);

	/* and anything to the right of a @ or % */
	for (a = 0; a < strlen(name); ++a) {
		if (name[a] == '@')
			name[a] = 0;
		if (name[a] == '%')
			name[a] = 0;
	}

	/* but if there are parentheses, that changes the rules... */
	if ((haschar(rfc822, '(') == 1) && (haschar(rfc822, ')') == 1)) {
		strcpy(name, rfc822);
		stripallbut(name, '(', ')');
	}

	/* but if there are a set of quotes, that supersedes everything */
	if (haschar(rfc822, 34) == 2) {
		strcpy(name, rfc822);
		while ((!IsEmptyStr(name)) && (name[0] != 34)) {
			strcpy(&name[0], &name[1]);
		}
		strcpy(&name[0], &name[1]);
		for (a = 0; a < strlen(name); ++a)
			if (name[a] == 34)
				name[a] = 0;
	}
	/* extract user id */
	strcpy(user, rfc822);

	/* first get rid of anything in parens */
	stripout(user, '(', ')');

	/* if there's a set of angle brackets, strip it down to that */
	if ((haschar(user, '<') == 1) && (haschar(user, '>') == 1)) {
		stripallbut(user, '<', '>');
	}

	/* strip anything to the left of a bang */
	while ((!IsEmptyStr(user)) && (haschar(user, '!') > 0))
		strcpy(user, &user[1]);

	/* and anything to the right of a @ or % */
	for (a = 0; a < strlen(user); ++a) {
		if (user[a] == '@')
			user[a] = 0;
		if (user[a] == '%')
			user[a] = 0;
	}


	/* extract node name */
	strcpy(node, rfc822);

	/* first get rid of anything in parens */
	stripout(node, '(', ')');

	/* if there's a set of angle brackets, strip it down to that */
	if ((haschar(node, '<') == 1) && (haschar(node, '>') == 1)) {
		stripallbut(node, '<', '>');
	}

	/* If no node specified, tack ours on instead */
	if (
		(haschar(node, '@')==0)
		&& (haschar(node, '%')==0)
		&& (haschar(node, '!')==0)
	) {
		strcpy(node, config.c_nodename);
	}

	else {

		/* strip anything to the left of a @ */
		while ((!IsEmptyStr(node)) && (haschar(node, '@') > 0))
			strcpy(node, &node[1]);
	
		/* strip anything to the left of a % */
		while ((!IsEmptyStr(node)) && (haschar(node, '%') > 0))
			strcpy(node, &node[1]);
	
		/* reduce multiple system bang paths to node!user */
		while ((!IsEmptyStr(node)) && (haschar(node, '!') > 1))
			strcpy(node, &node[1]);
	
		/* now get rid of the user portion of a node!user string */
		for (a = 0; a < strlen(node); ++a)
			if (node[a] == '!')
				node[a] = 0;
	}

	/* strip leading and trailing spaces in all strings */
	striplt(user);
	striplt(node);
	striplt(name);

	/* If we processed a string that had the address in angle brackets
	 * but no name outside the brackets, we now have an empty name.  In
	 * this case, use the user portion of the address as the name.
	 */
	if ((IsEmptyStr(name)) && (!IsEmptyStr(user))) {
		strcpy(name, user);
	}
}



/*
 * convert_field() is a helper function for convert_internet_message().
 * Given start/end positions for an rfc822 field, it converts it to a Citadel
 * field if it wants to, and unfolds it if necessary.
 *
 * Returns 1 if the field was converted and inserted into the Citadel message
 * structure, implying that the source field should be removed from the
 * message text.
 */
int convert_field(struct CtdlMessage *msg, const char *beg, const char *end) {
	char *key, *value, *valueend;
	long len;
	const char *pos;
	int i;
	const char *colonpos = NULL;
	int processed = 0;
	char user[1024];
	char node[1024];
	char name[1024];
	char addr[1024];
	time_t parsed_date;
	long valuelen;

	for (pos = end; pos >= beg; pos--) {
		if (*pos == ':') colonpos = pos;
	}

	if (colonpos == NULL) return(0);	/* no colon? not a valid header line */

	len = end - beg;
	key = malloc(len + 2);
	memcpy(key, beg, len + 1);
	key[len] = '\0';
	valueend = key + len;
	* ( key + (colonpos - beg) ) = '\0';
	value = &key[(colonpos - beg) + 1];
/*	printf("Header: [%s]\nValue: [%s]\n", key, value); */
	unfold_rfc822_field(&value, &valueend);
	valuelen = valueend - value + 1;
/*	printf("UnfoldedValue: [%s]\n", value); */

	/*
	 * Here's the big rfc822-to-citadel loop.
	 */

	/* Date/time is converted into a unix timestamp.  If the conversion
	 * fails, we replace it with the time the message arrived locally.
	 */
	if (!strcasecmp(key, "Date")) {
		parsed_date = parsedate(value);
		if (parsed_date < 0L) parsed_date = time(NULL);

		if (CM_IsEmpty(msg, eTimestamp))
			CM_SetFieldLONG(msg, eTimestamp, parsed_date);
		processed = 1;
	}

	else if (!strcasecmp(key, "From")) {
		process_rfc822_addr(value, user, node, name);
		syslog(LOG_DEBUG, "Converted to <%s@%s> (%s)\n", user, node, name);
		snprintf(addr, sizeof(addr), "%s@%s", user, node);
		if (CM_IsEmpty(msg, eAuthor))
			CM_SetField(msg, eAuthor, name, strlen(name));
		if (CM_IsEmpty(msg, erFc822Addr))
			CM_SetField(msg, erFc822Addr, addr, strlen(addr));
		processed = 1;
	}

	else if (!strcasecmp(key, "Subject")) {
		if (CM_IsEmpty(msg, eMsgSubject))
			CM_SetField(msg, eMsgSubject, value, valuelen);
		processed = 1;
	}

	else if (!strcasecmp(key, "List-ID")) {
		if (CM_IsEmpty(msg, eListID))
			CM_SetField(msg, eListID, value, valuelen);
		processed = 1;
	}

	else if (!strcasecmp(key, "To")) {
		if (CM_IsEmpty(msg, eRecipient))
			CM_SetField(msg, eRecipient, value, valuelen);
		processed = 1;
	}

	else if (!strcasecmp(key, "CC")) {
		if (CM_IsEmpty(msg, eCarbonCopY))
			CM_SetField(msg, eCarbonCopY, value, valuelen);
		processed = 1;
	}

	else if (!strcasecmp(key, "Message-ID")) {
		if (!CM_IsEmpty(msg, emessageId)) {
			syslog(LOG_WARNING, "duplicate message id\n");
		}
		else {
			char *pValue;
			long pValueLen;

			pValue = value;
			pValueLen = valuelen;
			/* Strip angle brackets */
			while (haschar(pValue, '<') > 0) {
				pValue ++;
				pValueLen --;
			}

			for (i = 0; i <= pValueLen; ++i)
				if (pValue[i] == '>') {
					pValueLen = i;
					break;
				}

			CM_SetField(msg, emessageId, pValue, pValueLen);
		}

		processed = 1;
	}

	else if (!strcasecmp(key, "Return-Path")) {
		if (CM_IsEmpty(msg, eMessagePath))
			CM_SetField(msg, eMessagePath, value, valuelen);
		processed = 1;
	}

	else if (!strcasecmp(key, "Envelope-To")) {
		if (CM_IsEmpty(msg, eenVelopeTo))
			CM_SetField(msg, eenVelopeTo, value, valuelen);
		processed = 1;
	}

	else if (!strcasecmp(key, "References")) {
		CM_SetField(msg, eWeferences, value, valuelen);
		processed = 1;
	}

	else if (!strcasecmp(key, "Reply-To")) {
		CM_SetField(msg, eReplyTo, value, valuelen);
		processed = 1;
	}

	else if (!strcasecmp(key, "In-reply-to")) {
		if (CM_IsEmpty(msg, eWeferences)) /* References: supersedes In-reply-to: */
			CM_SetField(msg, eWeferences, value, valuelen);
		processed = 1;
	}



	/* Clean up and move on. */
	free(key);	/* Don't free 'value', it's actually the same buffer */
	return processed;
}


/*
 * Convert RFC822 references format (References) to Citadel references format (Weferences)
 */
void convert_references_to_wefewences(char *str) {
	int bracket_nesting = 0;
	char *ptr = str;
	char *moveptr = NULL;
	char ch;

	while(*ptr) {
		ch = *ptr;
		if (ch == '>') {
			--bracket_nesting;
			if (bracket_nesting < 0) bracket_nesting = 0;
		}
		if ((ch == '>') && (bracket_nesting == 0) && (*(ptr+1)) && (ptr>str) ) {
			*ptr = '|';
			++ptr;
		}
		else if (bracket_nesting > 0) {
			++ptr;
		}
		else {
			moveptr = ptr;
			while (*moveptr) {
				*moveptr = *(moveptr+1);
				++moveptr;
			}
		}
		if (ch == '<') ++bracket_nesting;
	}

}


/*
 * Convert an RFC822 message (headers + body) to a CtdlMessage structure.
 * NOTE: the supplied buffer becomes part of the CtdlMessage structure, and
 * will be deallocated when CM_Free() is called.  Therefore, the
 * supplied buffer should be DEREFERENCED.  It should not be freed or used
 * again.
 */
struct CtdlMessage *convert_internet_message(char *rfc822) {
	StrBuf *RFCBuf = NewStrBufPlain(rfc822, -1);
	free (rfc822);
	return convert_internet_message_buf(&RFCBuf);
}



struct CtdlMessage *convert_internet_message_buf(StrBuf **rfc822)
{
	struct CtdlMessage *msg;
	const char *pos, *beg, *end, *totalend;
	int done, alldone = 0;
	int converted;
	StrBuf *OtherHeaders;

	msg = malloc(sizeof(struct CtdlMessage));
	if (msg == NULL) return msg;

	memset(msg, 0, sizeof(struct CtdlMessage));
	msg->cm_magic = CTDLMESSAGE_MAGIC;	/* self check */
	msg->cm_anon_type = 0;			/* never anonymous */
	msg->cm_format_type = FMT_RFC822;	/* internet message */

	pos = ChrPtr(*rfc822);
	totalend = pos + StrLength(*rfc822);
	done = 0;
	OtherHeaders = NewStrBufPlain(NULL, StrLength(*rfc822));

	while (!alldone) {

		/* Locate beginning and end of field, keeping in mind that
		 * some fields might be multiline
		 */
		end = beg = pos;

		while ((end < totalend) && 
		       (end == beg) && 
		       (done == 0) ) 
		{

			if ( (*pos=='\n') && ((*(pos+1))!=0x20) && ((*(pos+1))!=0x09) )
			{
				end = pos;
			}

			/* done with headers? */
			if ((*pos=='\n') &&
			    ( (*(pos+1)=='\n') ||
			      (*(pos+1)=='\r')) ) 
			{
				alldone = 1;
			}

			if (pos >= (totalend - 1) )
			{
				end = pos;
				done = 1;
			}

			++pos;

		}

		/* At this point we have a field.  Are we interested in it? */
		converted = convert_field(msg, beg, end);

		/* Strip the field out of the RFC822 header if we used it */
		if (!converted) {
			StrBufAppendBufPlain(OtherHeaders, beg, end - beg, 0);
			StrBufAppendBufPlain(OtherHeaders, HKEY("\n"), 0);
		}

		/* If we've hit the end of the message, bail out */
		if (pos >= totalend)
			alldone = 1;
	}
	StrBufAppendBufPlain(OtherHeaders, HKEY("\n"), 0);
	if (pos < totalend)
		StrBufAppendBufPlain(OtherHeaders, pos, totalend - pos, 0);
	FreeStrBuf(rfc822);
	CM_SetAsFieldSB(msg, eMesageText, &OtherHeaders);

	/* Follow-up sanity checks... */

	/* If there's no timestamp on this message, set it to now. */
	if (CM_IsEmpty(msg, eTimestamp)) {
		CM_SetFieldLONG(msg, eTimestamp, time(NULL));
	}

	/* If a W (references, or rather, Wefewences) field is present, we
	 * have to convert it from RFC822 format to Citadel format.
	 */
	if (!CM_IsEmpty(msg, eWeferences)) {
		/// todo: API!
		convert_references_to_wefewences(msg->cm_fields[eWeferences]);
	}

	return msg;
}



/*
 * Look for a particular header field in an RFC822 message text.  If the
 * requested field is found, it is unfolded (if necessary) and returned to
 * the caller.  The field name is stripped out, leaving only its contents.
 * The caller is responsible for freeing the returned buffer.  If the requested
 * field is not present, or anything else goes wrong, it returns NULL.
 */
char *rfc822_fetch_field(const char *rfc822, const char *fieldname) {
	char *fieldbuf = NULL;
	const char *end_of_headers;
	const char *field_start;
	const char *ptr;
	char *cont;
	char fieldhdr[SIZ];

	/* Should never happen, but sometimes we get stupid */
	if (rfc822 == NULL) return(NULL);
	if (fieldname == NULL) return(NULL);

	snprintf(fieldhdr, sizeof fieldhdr, "%s:", fieldname);

	/* Locate the end of the headers, so we don't run past that point */
	end_of_headers = cbmstrcasestr(rfc822, "\n\r\n");
	if (end_of_headers == NULL) {
		end_of_headers = cbmstrcasestr(rfc822, "\n\n");
	}
	if (end_of_headers == NULL) return (NULL);

	field_start = cbmstrcasestr(rfc822, fieldhdr);
	if (field_start == NULL) return(NULL);
	if (field_start > end_of_headers) return(NULL);

	fieldbuf = malloc(SIZ);
	strcpy(fieldbuf, "");

	ptr = field_start;
	ptr = cmemreadline(ptr, fieldbuf, SIZ-strlen(fieldbuf) );
	while ( (isspace(ptr[0])) && (ptr < end_of_headers) ) {
		strcat(fieldbuf, " ");
		cont = &fieldbuf[strlen(fieldbuf)];
		ptr = cmemreadline(ptr, cont, SIZ-strlen(fieldbuf) );
		striplt(cont);
	}

	strcpy(fieldbuf, &fieldbuf[strlen(fieldhdr)]);
	striplt(fieldbuf);

	return(fieldbuf);
}



/*****************************************************************************
 *                      DIRECTORY MANAGEMENT FUNCTIONS                       *
 *****************************************************************************/

/*
 * Generate the index key for an Internet e-mail address to be looked up
 * in the database.
 */
void directory_key(char *key, char *addr) {
	int i;
	int keylen = 0;

	for (i=0; !IsEmptyStr(&addr[i]); ++i) {
		if (!isspace(addr[i])) {
			key[keylen++] = tolower(addr[i]);
		}
	}
	key[keylen++] = 0;

	syslog(LOG_DEBUG, "Directory key is <%s>\n", key);
}



/* Return nonzero if the supplied address is in a domain we keep in
 * the directory
 */
int IsDirectory(char *addr, int allow_masq_domains) {
	char domain[256];
	int h;

	extract_token(domain, addr, 1, '@', sizeof domain);
	striplt(domain);

	h = CtdlHostAlias(domain);

	if ( (h == hostalias_masq) && allow_masq_domains)
		return(1);
	
	if ( (h == hostalias_localhost) || (h == hostalias_directory) ) {
		return(1);
	}
	else {
		return(0);
	}
}


/*
 * Initialize the directory database (erasing anything already there)
 */
void CtdlDirectoryInit(void) {
	cdb_trunc(CDB_DIRECTORY);
}


/*
 * Add an Internet e-mail address to the directory for a user
 */
int CtdlDirectoryAddUser(char *internet_addr, char *citadel_addr) {
	char key[SIZ];

	if (IsDirectory(internet_addr, 0) == 0) 
		return 0;
	syslog(LOG_DEBUG, "Create directory entry: %s --> %s\n", internet_addr, citadel_addr);
	directory_key(key, internet_addr);
	cdb_store(CDB_DIRECTORY, key, strlen(key), citadel_addr, strlen(citadel_addr)+1 );
	return 1;
}


/*
 * Delete an Internet e-mail address from the directory.
 *
 * (NOTE: we don't actually use or need the citadel_addr variable; it's merely
 * here because the callback API expects to be able to send it.)
 */
int CtdlDirectoryDelUser(char *internet_addr, char *citadel_addr) {
	char key[SIZ];

	syslog(LOG_DEBUG, "Delete directory entry: %s --> %s\n", internet_addr, citadel_addr);
	directory_key(key, internet_addr);
	return cdb_delete(CDB_DIRECTORY, key, strlen(key) ) == 0;
}


/*
 * Look up an Internet e-mail address in the directory.
 * On success: returns 0, and Citadel address stored in 'target'
 * On failure: returns nonzero
 */
int CtdlDirectoryLookup(char *target, char *internet_addr, size_t targbuflen) {
	struct cdbdata *cdbrec;
	char key[SIZ];

	/* Dump it in there unchanged, just for kicks */
	safestrncpy(target, internet_addr, targbuflen);

	/* Only do lookups for addresses with hostnames in them */
	if (num_tokens(internet_addr, '@') != 2) return(-1);

	/* Only do lookups for domains in the directory */
	if (IsDirectory(internet_addr, 0) == 0) return(-1);

	directory_key(key, internet_addr);
	cdbrec = cdb_fetch(CDB_DIRECTORY, key, strlen(key) );
	if (cdbrec != NULL) {
		safestrncpy(target, cdbrec->ptr, targbuflen);
		cdb_free(cdbrec);
		return(0);
	}

	return(-1);
}


/*
 * Harvest any email addresses that someone might want to have in their
 * "collected addresses" book.
 */
char *harvest_collected_addresses(struct CtdlMessage *msg) {
	char *coll = NULL;
	char addr[256];
	char user[256], node[256], name[256];
	int is_harvestable;
	int i, j, h;
	eMsgField field = 0;

	if (msg == NULL) return(NULL);

	is_harvestable = 1;
	strcpy(addr, "");	
	if (!CM_IsEmpty(msg, eAuthor)) {
		strcat(addr, msg->cm_fields[eAuthor]);
	}
	if (!CM_IsEmpty(msg, erFc822Addr)) {
		strcat(addr, " <");
		strcat(addr, msg->cm_fields[erFc822Addr]);
		strcat(addr, ">");
		if (IsDirectory(msg->cm_fields[erFc822Addr], 0)) {
			is_harvestable = 0;
		}
	}

	if (is_harvestable) {
		coll = strdup(addr);
	}
	else {
		coll = strdup("");
	}

	if (coll == NULL) return(NULL);

	/* Scan both the R (To) and Y (CC) fields */
	for (i = 0; i < 2; ++i) {
		if (i == 0) field = eRecipient;
		if (i == 1) field = eCarbonCopY;

		if (!CM_IsEmpty(msg, field)) {
			for (j=0; j<num_tokens(msg->cm_fields[field], ','); ++j) {
				extract_token(addr, msg->cm_fields[field], j, ',', sizeof addr);
				if (strstr(addr, "=?") != NULL)
					utf8ify_rfc822_string(addr);
				process_rfc822_addr(addr, user, node, name);
				h = CtdlHostAlias(node);
				if ( (h != hostalias_localhost) && (h != hostalias_directory) ) {
					coll = realloc(coll, strlen(coll) + strlen(addr) + 4);
					if (coll == NULL) return(NULL);
					if (!IsEmptyStr(coll)) {
						strcat(coll, ",");
					}
					striplt(addr);
					strcat(coll, addr);
				}
			}
		}
	}

	if (IsEmptyStr(coll)) {
		free(coll);
		return(NULL);
	}
	return(coll);
}
