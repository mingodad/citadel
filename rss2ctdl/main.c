/*
 * $Id$
 *
 * rss2ctdl -- a utility to pull RSS feeds into Citadel rooms.
 * 
 * Main program is (c)2004 by Art Cancro
 * RSS parser is (c)2003-2004 by Oliver Feiler
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>

#include "config.h"
#include "main.h"
#include "io-internal.h"
#include "conversions.h"
#include "md5.h"
#include "digcalc.h"

struct feed *first_ptr = NULL;
struct entity *first_entity = NULL;

/*
 * If you want to use a proxy server, you can hack the following two lines.
 */
char *proxyname = "";
unsigned short proxyport = 0;

/*
 * Main function of program.
 */
int main (int argc, char *argv[]) {
	struct feed *new_ptr;
	char *url;
	char tmp[512];
	struct newsitem *itemptr;
	FILE *fp;
	char md5msgid[256];
	MD5_CTX md5context;
	HASHHEX md5context_hex;
	
#ifdef LOCALEPATH
	setlocale (LC_ALL, "");
	bindtextdomain ("rss2ctdl", LOCALEPATH);
	textdomain ("rss2ctdl");
#endif

	if (argc != 5) {
		fprintf(stderr,
			"%s: usage:\n %s <feedurl> <roomname> <nodefqdn> <ctdldir>\n",
			argv[0], argv[0]);
		exit(1);
	}

	/* Init the pRNG. See about.c for usages of rand() ;) */
	srand(time(0));

	url = strdup(argv[1]);
	CleanupString(url, 0);

	/* Support that stupid feed:// "protocol" */
	if (strncasecmp (url, "feed://", 7) == 0)
		memcpy (url, "http", 4);
	
	/* If URL does not start with the procotol specification,
	assume http://
	-> tmp[512] -> we can "only" use max 504 chars from url ("http://" == 7). */
	if ((strncasecmp (url, "http://", 7) != 0) &&
		(strncasecmp (url, "https://", 8) != 0)) {
		if (strlen (url) < 504) {
			strcpy (tmp, "http://");
			strncat (tmp, url, 504);
			free (url);
			url = strdup (tmp);
		} else {
			free (url);
			return 2;
		}
	}

	new_ptr = malloc (sizeof(struct feed));
	new_ptr->feedurl = strdup(url);
	new_ptr->feed = NULL;
	new_ptr->content_length = 0;
	new_ptr->title = NULL;
	new_ptr->link = NULL;
	new_ptr->description = NULL;
	new_ptr->lastmodified = NULL;
	new_ptr->lasthttpstatus = 0;
	new_ptr->content_type = NULL;
	new_ptr->netio_error = NET_ERR_OK;
	new_ptr->connectresult = 0;
	new_ptr->cookies = NULL;
	new_ptr->authinfo = NULL;
	new_ptr->servauth = NULL;
	new_ptr->items = NULL;
	new_ptr->problem = 0;
	new_ptr->original = NULL;
	
	/* Don't need url text anymore. */
	free (url);

	/* Download new feed and DeXMLize it. */	
	if ((UpdateFeed (new_ptr)) != 0) {
		exit(1);
	}

	sprintf(tmp, "%s/network/spoolin/rssfeed.%ld", argv[4], time(NULL));
	fp = fopen(tmp, "w");
	if (fp == NULL) {
		fprintf(stderr, "%s: cannot open %s: %s\n",
			argv[0], tmp, strerror(errno));
		exit(errno);
	}

	for (itemptr = new_ptr->items; itemptr != NULL; itemptr = itemptr->next_ptr) {
		fprintf(stderr, "--> %s\n", itemptr->data->title);
		fprintf(fp, "%c", 255);			/* Start of message */
		fprintf(fp, "A");			/* Non-anonymous */
		fprintf(fp, "%c", 4);			/* MIME */
		fprintf(fp, "Prss%c", 0);		/* path */

		/* The message ID will be an MD5 hash of the GUID.
		 * If there is no GUID present, we construct a message ID based
		 * on an MD5 hash of each item.  Citadel's loopzapper will automatically
		 * reject items with message ID's which have already been submitted.
		 */
		MD5Init(&md5context);
		if (itemptr->data->guid != NULL) {
			MD5Update(&md5context, itemptr->data->guid, strlen(itemptr->data->guid));
		}
		else {
			if (itemptr->data->title != NULL) {
				MD5Update(&md5context, itemptr->data->title, strlen(itemptr->data->title));
			}
			if (itemptr->data->description != NULL) {
				MD5Update(&md5context, itemptr->data->description, strlen(itemptr->data->description));
			}
			if (itemptr->data->link != NULL) {
				MD5Update(&md5context, itemptr->data->link, strlen(itemptr->data->link));
			}
		}
		MD5Final(md5msgid, &md5context);
		CvtHex(md5msgid, md5context_hex);

		fprintf(fp, "I%s@%s%c", md5context_hex, argv[3], 0);	/* ID */ 

		fprintf(fp, "T%ld%c",  time(NULL),  0);	/* time */
		fprintf(fp, "Arss%c", 0);		/* author */
		fprintf(fp, "O%s%c", argv[2], 0);	/* room */
		fprintf(fp, "C%s%c", argv[2], 0);	/* room */
		fprintf(fp, "N%s%c", argv[3], 0);	/* orig node */
		if (itemptr->data->title != NULL) {
			fprintf(fp, "U%s%c", itemptr->data->title, 0);	/* subject */
		}

		fprintf(fp, "M");			/* msg text */
		fprintf(fp, "Content-type: text/html\r\n\r\n");
		fprintf(fp, "<HTML><BODY>\r\n");
		fprintf(fp, "%s\n", itemptr->data->description);
		if (itemptr->data->link != NULL) {
			fprintf(fp, "<BR><BR>\r\n");
			fprintf(fp, "<A HREF=\"%s\">%s</A>\n",
				itemptr->data->link,
				itemptr->data->link);
		}
		fprintf(fp, "</BODY></HTML>\r\n");
		fprintf(fp, "%c", 0);
	}

	fclose(fp);

	/* Be lazy and let the operating system free all the memory. */
	return(0);
}
