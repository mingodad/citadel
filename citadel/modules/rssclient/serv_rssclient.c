/*
 * $Id: serv_rssclient.c 5652 2007-10-29 20:14:48Z ajc $
 *
 * Bring external RSS feeds into rooms.
 *
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

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

#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "tools.h"
#include "room_ops.h"
#include "ctdl_module.h"
#include "clientsocket.h"
#include "msgbase.h"
#include "database.h"
#include "citadel_dirs.h"
#include "md5.h"

#ifdef HAVE_EXPAT
#include <expat.h>


struct rssnetcfg {
	struct rssnetcfg *next;
	char url[256];
	char *rooms;
};

struct rss_item {
	char *chardata;
	int chardata_len;
	char *roomlist;
	int done_parsing;
	char *guid;
	char *title;
	char *link;
	char *description;
	time_t pubdate;
	char channel_title[256];
	int item_tag_nesting;
};

struct rssnetcfg *rnclist = NULL;


/*
 * Commit a fetched and parsed RSS item to disk
 */
void rss_save_item(struct rss_item *ri) {

	struct MD5Context md5context;
	u_char rawdigest[MD5_DIGEST_LEN];
	int i;
	char utmsgid[SIZ];
	struct cdbdata *cdbut;
	struct UseTable ut;
	struct CtdlMessage *msg;
	struct recptypes *recp = NULL;
	int msglen = 0;

	recp = (struct recptypes *) malloc(sizeof(struct recptypes));
	if (recp == NULL) return;
	memset(recp, 0, sizeof(struct recptypes));
	recp->recp_room = strdup(ri->roomlist);
	recp->num_room = num_tokens(ri->roomlist, '|');
	recp->recptypes_magic = RECPTYPES_MAGIC;
   
	/* Construct a GUID to use in the S_USETABLE table.
	 * If one is not present in the item itself, make one up.
	 */
	if (ri->guid != NULL) {
		snprintf(utmsgid, sizeof utmsgid, "rss/%s", ri->guid);
	}
	else {
		MD5Init(&md5context);
		if (ri->title != NULL) {
			MD5Update(&md5context, ri->title, strlen(ri->title));
		}
		if (ri->link != NULL) {
			MD5Update(&md5context, ri->link, strlen(ri->link));
		}
		MD5Final(rawdigest, &md5context);
		for (i=0; i<MD5_DIGEST_LEN; i++) {
			sprintf(&utmsgid[i*2], "%02X", (unsigned char) (rawdigest[i] & 0xff));
			utmsgid[i*2] = tolower(utmsgid[i*2]);
			utmsgid[(i*2)+1] = tolower(utmsgid[(i*2)+1]);
		}
		strcat(utmsgid, "_rss2ctdl");
	}

	/* Find out if we've already seen this item */
	cdbut = cdb_fetch(CDB_USETABLE, utmsgid, strlen(utmsgid));
	if (cdbut != NULL) {
		/* Item has already been seen */
		lprintf(CTDL_DEBUG, "%s has already been seen\n", utmsgid);
		cdb_free(cdbut);

		/* rewrite the record anyway, to update the timestamp */
		strcpy(ut.ut_msgid, utmsgid);
		ut.ut_timestamp = time(NULL);
		cdb_store(CDB_USETABLE, utmsgid, strlen(utmsgid), &ut, sizeof(struct UseTable) );
	}
	else {
		/* Item has not been seen, so save it. */

		for (i=strlen(ri->description); i>=0; --i) {
			if (isspace(ri->description[i])) {
				ri->description[i] = ' ';
			}
		}

		msg = malloc(sizeof(struct CtdlMessage));
		memset(msg, 0, sizeof(struct CtdlMessage));
		msg->cm_magic = CTDLMESSAGE_MAGIC;
		msg->cm_anon_type = MES_NORMAL;
		msg->cm_format_type = FMT_RFC822;
		msg->cm_fields['A'] = strdup("rss");
		msg->cm_fields['N'] = strdup(NODENAME);
		msg->cm_fields['U'] = strdup(ri->title);
		msg->cm_fields['T'] = malloc(64);
		snprintf(msg->cm_fields['T'], 64, "%ld", ri->pubdate);
		if (!IsEmptyStr(ri->channel_title)) {
			msg->cm_fields['O'] = strdup(ri->channel_title);
		}

		msglen = 1024 + strlen(ri->link) + strlen(ri->description) ;
		msg->cm_fields['M'] = malloc(msglen);
		snprintf(msg->cm_fields['M'], msglen,
			"Content-type: text/html\r\n\r\n"
			"<html><body>\n"
			"%s<br><br>\n"
			"<a href=\"%s\">%s</a>\n"
			"</body></html>\n"
			,
			ri->description,
			ri->link, ri->link
		);

		CtdlSubmitMsg(msg, recp, NULL);
		CtdlFreeMessage(msg);
		free_recipients(recp);

		/* write the uidl to the use table so we don't store this item again */
		strcpy(ut.ut_msgid, utmsgid);
		ut.ut_timestamp = time(NULL);
		cdb_store(CDB_USETABLE, utmsgid, strlen(utmsgid), &ut, sizeof(struct UseTable) );
	}
}



/*
 * Convert an RDF/RSS datestamp into a time_t
 */
time_t rdf_parsedate(char *p)
{
	struct tm tm;

	if (!p) return 0L;
	if (strlen(p) < 10) return 0L;

	memset(&tm, 0, sizeof tm);

	/* YYYY-MM-DDTHH:MM format...
	 */
	if ( (p[4] == '-') && (p[7] == '-') ) {
		tm.tm_year = atoi(&p[0]) - 1900;
		tm.tm_mon = atoi(&p[5]) - 1;
		tm.tm_mday = atoi(&p[8]);
		if ( (p[10] == 'T') && (p[13] == ':') ) {
			tm.tm_hour = atoi(&p[11]);
			tm.tm_min = atoi(&p[14]);
		}
	}

	else {
		/* FIXME try an imap timestamp conversion */
	}

	return mktime(&tm);
}



void rss_xml_start(void *data, const char *supplied_el, const char **attr) {
	struct rss_item *ri = (struct rss_item *) data;
	char el[256];
	char *sep = NULL;

	/* Axe the namespace, we don't care about it */
	safestrncpy(el, supplied_el, sizeof el);
	while (sep = strchr(el, ':'), sep) {
		strcpy(el, ++sep);
	}

	if (!strcasecmp(el, "item")) {
		++ri->item_tag_nesting;

		/* Initialize the feed item data structure */
		if (ri->guid != NULL) free(ri->guid);
		ri->guid = NULL;
		if (ri->title != NULL) free(ri->title);
		ri->title = NULL;
		if (ri->link != NULL) free(ri->link);
		ri->link = NULL;
		if (ri->description != NULL) free(ri->description);
		ri->description = NULL;

		/* Throw away any existing character data */
		if (ri->chardata_len > 0) {
			free(ri->chardata);
			ri->chardata = 0;
			ri->chardata_len = 0;
		}
	}



}

void rss_xml_end(void *data, const char *supplied_el) {
	struct rss_item *ri = (struct rss_item *) data;
	char el[256];
	char *sep = NULL;

	/* Axe the namespace, we don't care about it */
	safestrncpy(el, supplied_el, sizeof el);
	while (sep = strchr(el, ':'), sep) {
		strcpy(el, ++sep);
	}

	if ( (!strcasecmp(el, "title")) && (ri->item_tag_nesting == 0) ) {
		safestrncpy(ri->channel_title, ri->chardata, sizeof ri->channel_title);
		striplt(ri->channel_title);
	}

	if (!strcasecmp(el, "guid")) {
		if (ri->guid != NULL) free(ri->guid);
		striplt(ri->chardata);
		ri->guid = strdup(ri->chardata);
	}

	if (!strcasecmp(el, "title")) {
		if (ri->title != NULL) free(ri->title);
		striplt(ri->chardata);
		ri->title = strdup(ri->chardata);
	}

	if (!strcasecmp(el, "link")) {
		if (ri->link != NULL) free(ri->link);
		striplt(ri->chardata);
		ri->link = strdup(ri->chardata);
	}

	if (!strcasecmp(el, "description")) {
		if (ri->description != NULL) free(ri->description);
		ri->description = strdup(ri->chardata);
	}

	if ( (!strcasecmp(el, "pubdate")) || (!strcasecmp(el, "date")) ) {
		striplt(ri->chardata);
		ri->pubdate = rdf_parsedate(ri->chardata);
	}

	if (!strcasecmp(el, "item")) {
		--ri->item_tag_nesting;
		rss_save_item(ri);
	}

	if ( (!strcasecmp(el, "rss")) || (!strcasecmp(el, "rdf")) ) {
		lprintf(CTDL_DEBUG, "End of feed detected.  Closing parser.\n");
		ri->done_parsing = 1;
	}

	if (ri->chardata_len > 0) {
		free(ri->chardata);
		ri->chardata = 0;
		ri->chardata_len = 0;
	}

}


/*
 * This callback stores up the data which appears in between tags.
 */
void rss_xml_chardata(void *data, const XML_Char *s, int len) {
	struct rss_item *ri = (struct rss_item *) data;
	int old_len;
	int new_len;
	char *new_buffer;

	old_len = ri->chardata_len;
	new_len = old_len + len;
	new_buffer = realloc(ri->chardata, new_len + 1);
	if (new_buffer != NULL) {
		memcpy(&new_buffer[old_len], s, len);
		new_buffer[new_len] = 0;
		ri->chardata = new_buffer;
		ri->chardata_len = new_len;
	}
}



/* 
 * Parse a URL into host, port number, and resource identifier.
 */
int parse_url(char *url, char *hostname, int *port, char *identifier)
{
	char protocol[1024];
	char scratch[1024];
	char *ptr = NULL;
	char *nptr = NULL;
	
	strcpy(scratch, url);
	ptr = (char *)strchr(scratch, ':');
	if (!ptr) {
		return(1);	/* no protocol specified */
	}

	strcpy(ptr, "");
	strcpy(protocol, scratch);
	if (strcmp(protocol, "http")) {
		return(2);	/* not HTTP */
	}

	strcpy(scratch, url);
	ptr = (char *) strstr(scratch, "//");
	if (!ptr) {
		return(3);	/* no server specified */
	}
	ptr += 2;

	strcpy(hostname, ptr);
	nptr = (char *)strchr(ptr, ':');
	if (!nptr) {
		*port = 80;	/* default */
		nptr = (char *)strchr(hostname, '/');
	}
	else {
		sscanf(nptr, ":%d", port);
		nptr = (char *)strchr(hostname, ':');
	}

	if (nptr) {
		*nptr = '\0';
	}

	nptr = (char *)strchr(ptr, '/');
	
	if (!nptr) {
		return(4);	/* no url specified */
	}
	
	strcpy(identifier, nptr);
	return(0);
}


/*
 * Begin a feed parse
 */
void rss_do_fetching(char *url, char *rooms) {
	char buf[1024];
	char rsshost[1024];
	int rssport = 80;
	char rssurl[1024];
	struct rss_item ri;
	XML_Parser xp;
	int sock = (-1);
	int got_bytes = (-1);
	int redirect_count = 0;

	/* Parse the URL */
	if (parse_url(url, rsshost, &rssport, rssurl) != 0) {
		lprintf(CTDL_ALERT, "Invalid URL: %s\n", url);
	}

	xp = XML_ParserCreateNS("UTF-8", ':');
	if (!xp) {
		lprintf(CTDL_ALERT, "Cannot create XML parser!\n");
		return;
	}

	memset(&ri, 0, sizeof(struct rss_item));
	ri.roomlist = rooms;
	XML_SetElementHandler(xp, rss_xml_start, rss_xml_end);
	XML_SetCharacterDataHandler(xp, rss_xml_chardata);
	XML_SetUserData(xp, &ri);

retry:	lprintf(CTDL_NOTICE, "Connecting to <%s>\n", rsshost);
	sprintf(buf, "%d", rssport);
	sock = sock_connect(rsshost, buf, "tcp");
	if (sock >= 0) {
		lprintf(CTDL_DEBUG, "Connected!\n");

		snprintf(buf, sizeof buf, "GET %s HTTP/1.0", rssurl);
		lprintf(CTDL_DEBUG, "<%s\n", buf);
		sock_puts(sock, buf);

		snprintf(buf, sizeof buf, "Host: %s", rsshost);
		lprintf(CTDL_DEBUG, "<%s\n", buf);
		sock_puts(sock, buf);

		sock_puts(sock, "");

		if (sock_getln(sock, buf, sizeof buf) >= 0) {
		lprintf(CTDL_DEBUG, ">%s\n", buf);
			remove_token(buf, 0, ' ');

			/* 200 OK */
			if (buf[0] == '2') {

			        while (got_bytes = sock_getln(sock, buf, sizeof buf),
				      (got_bytes >= 0 && (strcmp(buf, "")) && (strcmp(buf, "\r"))) ) {
					/* discard headers */
				}

				while (got_bytes = sock_read(sock, buf, sizeof buf, 0),
				      ((got_bytes>=0) && (ri.done_parsing == 0)) ) {
					XML_Parse(xp, buf, got_bytes, 0);
				}
				if (ri.done_parsing == 0) XML_Parse(xp, "", 0, 1);
			}

			/* 30X redirect */
			else if ( (!strncmp(buf, "30", 2)) && (redirect_count < 16) ) {
			        while (got_bytes = sock_getln(sock, buf, sizeof buf),
				      (got_bytes >= 0 && (strcmp(buf, "")) && (strcmp(buf, "\r"))) ) {
					if (!strncasecmp(buf, "Location:", 9)) {
						++redirect_count;
						strcpy(buf, &buf[9]);
						striplt(buf);
						if (parse_url(buf, rsshost, &rssport, rssurl) == 0) {
							goto retry;
						}
						else {
							lprintf(CTDL_ALERT, "Invalid URL: %s\n", buf);
						}
					}
				}
			}

		}
		sock_close(sock);
	}
	else {
		lprintf(CTDL_ERR, "Could not connect: %s\n", strerror(errno));
	}

	XML_ParserFree(xp);

	/* Free the feed item data structure */
	if (ri.guid != NULL) free(ri.guid);
	ri.guid = NULL;
	if (ri.title != NULL) free(ri.title);
	ri.title = NULL;
	if (ri.link != NULL) free(ri.link);
	ri.link = NULL;
	if (ri.description != NULL) free(ri.description);
	ri.description = NULL;
	if (ri.chardata_len > 0) {
		free(ri.chardata);
		ri.chardata = 0;
		ri.chardata_len = 0;
	}
}


/*
 * Scan a room's netconfig to determine whether it is requesting any RSS feeds
 */
void rssclient_scan_room(struct ctdlroom *qrbuf, void *data)
{
	char filename[PATH_MAX];
	char buf[1024];
	char instr[32];
	FILE *fp;
	char feedurl[256];
	struct rssnetcfg *rncptr = NULL;
	struct rssnetcfg *use_this_rncptr = NULL;
	int len = 0;
	char *ptr = NULL;

	assoc_file_name(filename, sizeof filename, qrbuf, ctdl_netcfg_dir);

	/* Only do net processing for rooms that have netconfigs */
	fp = fopen(filename, "r");
	if (fp == NULL) {
		return;
	}

	while (fgets(buf, sizeof buf, fp) != NULL) {
		buf[strlen(buf)-1] = 0;

		extract_token(instr, buf, 0, '|', sizeof instr);
		if (!strcasecmp(instr, "rssclient")) {
			extract_token(feedurl, buf, 1, '|', sizeof feedurl);

			/* If any other rooms have requested the same feed, then we will just add this
			 * room to the target list for that client request.
			 */
			for (rncptr=rnclist; rncptr!=NULL; rncptr=rncptr->next) {
				if (!strcmp(rncptr->url, feedurl)) {
					use_this_rncptr = rncptr;
				}
			}

			/* Otherwise create a new client request */
			if (use_this_rncptr == NULL) {
				rncptr = (struct rssnetcfg *) malloc(sizeof(struct rssnetcfg));
				if (rncptr != NULL) {
					rncptr->next = rnclist;
					safestrncpy(rncptr->url, feedurl, sizeof rncptr->url);
					rncptr->rooms = NULL;
					rnclist = rncptr;
					use_this_rncptr = rncptr;
				}
			}

			/* Add the room name to the request */
			if (use_this_rncptr != NULL) {
				if (use_this_rncptr->rooms == NULL) {
					rncptr->rooms = strdup(qrbuf->QRname);
				}
				else {
					len = strlen(use_this_rncptr->rooms) + strlen(qrbuf->QRname) + 5;
					ptr = realloc(use_this_rncptr->rooms, len);
					if (ptr != NULL) {
						strcat(ptr, "|");
						strcat(ptr, qrbuf->QRname);
						use_this_rncptr->rooms = ptr;
					}
				}
			}
		}

	}

	fclose(fp);

}

/*
 * Scan for rooms that have RSS client requests configured
 */
void rssclient_scan(void) {
	static time_t last_run = 0L;
	static int doing_rssclient = 0;
	struct rssnetcfg *rptr = NULL;

	/*
	 * Run RSS client no more frequently than once every n seconds
	 */
	if ( (time(NULL) - last_run) < config.c_net_freq ) {
		return;
	}

	/*
	 * This is a simple concurrency check to make sure only one rssclient run
	 * is done at a time.  We could do this with a mutex, but since we
	 * don't really require extremely fine granularity here, we'll do it
	 * with a static variable instead.
	 */
	if (doing_rssclient) return;
	doing_rssclient = 1;

	lprintf(CTDL_DEBUG, "rssclient started\n");
	ForEachRoom(rssclient_scan_room, NULL);

	while (rnclist != NULL) {
		rss_do_fetching(rnclist->url, rnclist->rooms);
		rptr = rnclist;
		rnclist = rnclist->next;
		if (rptr->rooms != NULL) free(rptr->rooms);
		free(rptr);
	}

	lprintf(CTDL_DEBUG, "rssclient ended\n");
	last_run = time(NULL);
	doing_rssclient = 0;
}


#endif	/* HAVE_EXPAT */

CTDL_MODULE_INIT(rssclient)
{
#ifdef HAVE_EXPAT
	CtdlRegisterSessionHook(rssclient_scan, EVT_TIMER);
#else
	 lprintf(CTDL_INFO, "This server is missing the Expat XML parser.  RSS client will be disabled.\n");
#endif
	/* return our Subversion id for the Log */
        return "$Id: serv_rssclient.c 5652 2007-10-29 20:14:48Z ajc $";
}
