/*
 * $Id$
 * 
 * Copyright 2003-2004 Oliver Feiler <kiza@kcore.de>
 *
 * io-internal.c
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

#include "config.h"

#include "main.h"
#include "conversions.h"
#include "netio.h"
#include "xmlparse.h"
#include "io-internal.h"

extern char *browser;

void GetHTTPErrorString (char * errorstring, int size, int httpstatus) {
	switch (httpstatus) {
		case 400:
			snprintf(errorstring, size, "Bad request");
			break;
		case 402:
			snprintf(errorstring, size, "Payment required");
			break;
		case 403:
			snprintf(errorstring, size, "Access denied");
			break;
		case 500:
			snprintf(errorstring, size, "Internal server error");
			break;
		case 501:
			snprintf(errorstring, size, "Not implemented");
			break;
		case 502:
		case 503:
			snprintf(errorstring, size, "Service unavailable");
			break;
		default:
			sprintf(errorstring, "HTTP %d!", httpstatus);
	}
}

void PrintUpdateError (int suppressoutput, struct feed * cur_ptr) {
	netio_error_type err;
	char errstr[256];
	char httperrstr[64];

	err = cur_ptr->netio_error;
	
	if (!suppressoutput) {
		switch (err) {
			case NET_ERR_OK:
				break;
			case NET_ERR_URL_INVALID:
				fprintf(stderr, "%s: Invalid URL!\n", cur_ptr->title);
				break;
			case NET_ERR_SOCK_ERR:
				fprintf(stderr, "%s: Couldn't create network socket!\n", cur_ptr->title);
				break;
			case NET_ERR_HOST_NOT_FOUND:
				fprintf(stderr, "%s: Can't resolve host!\n", cur_ptr->title);
				break;
			case NET_ERR_CONN_REFUSED:
				fprintf(stderr, "%s: Connection refused!\n", cur_ptr->title);
				break;
			case NET_ERR_CONN_FAILED:
				fprintf(stderr, "%s: Couldn't connect to server: %s\n",
					cur_ptr->title,
					(strerror(cur_ptr->connectresult) ? strerror(cur_ptr->connectresult) : "(null)"));
				break;
			case NET_ERR_TIMEOUT:
				fprintf(stderr, "%s: Connection timed out.\n", cur_ptr->title);
				break;
			case NET_ERR_UNKNOWN:
				break;
			case NET_ERR_REDIRECT_COUNT_ERR:
				fprintf(stderr, "%s: Too many HTTP redirects encountered! Giving up.\n", cur_ptr->title);
				break;
			case NET_ERR_REDIRECT_ERR:
				fprintf(stderr, "%s: Server sent an invalid redirect!\n", cur_ptr->title);
				break;
			case NET_ERR_HTTP_410:
			case NET_ERR_HTTP_404:
				fprintf(stderr, "%s: This feed no longer exists. Please unsubscribe!\n", cur_ptr->title);
				break;
			case NET_ERR_HTTP_NON_200:
				GetHTTPErrorString(httperrstr, sizeof(httperrstr), cur_ptr->lasthttpstatus);
				fprintf(stderr, "%s: Could not download feed: %s\n", cur_ptr->title, httperrstr);
				break;
			case NET_ERR_HTTP_PROTO_ERR:
				fprintf(stderr, "%s: Error in server reply.\n", cur_ptr->title);
				break;
			case NET_ERR_AUTH_FAILED:
				fprintf(stderr, "%s: Authentication failed!\n", cur_ptr->title);
				break;
			case NET_ERR_AUTH_NO_AUTHINFO:
				fprintf(stderr, "%s: URL does not contain authentication information!\n", cur_ptr->title);
				break;
			case NET_ERR_AUTH_GEN_AUTH_ERR:
				fprintf(stderr, "%s: Could not generate authentication information!\n", cur_ptr->title);
				break;
			case NET_ERR_AUTH_UNSUPPORTED:
				fprintf(stderr, "%s: Unsupported authentication method requested by server!\n", cur_ptr->title);
				break;
			case NET_ERR_GZIP_ERR:
				fprintf(stderr, "%s: Error decompressing server reply!\n", cur_ptr->title);
				break;
			default:
				break;
		}
		/* Must be inside if(!suppressoutput) statement! */
	}
}


/* Update given feed from server.
 * Reload XML document and replace in memory cur_ptr->feed with it.
 */
int UpdateFeed (struct feed * cur_ptr) {
	char *tmpname;
	char *freeme;

	if (cur_ptr == NULL) {
		return 1;
	}
	
	/* Need to work on a copy of ->feedurl, because DownloadFeed() changes the pointer. */
	tmpname = strdup (cur_ptr->feedurl);
	freeme = tmpname;	/* Need to make a copy, otherwise we cannot free all RAM. */
	free (cur_ptr->feed);

	cur_ptr->feed = DownloadFeed (tmpname, cur_ptr, 0);
	free (freeme);

	/* Set title and link structure to something.
	 * To the feedurl in this case so the program show something
	 * as placeholder instead of crash. */
	if (cur_ptr->title == NULL)
		cur_ptr->title = strdup (cur_ptr->feedurl);
	if (cur_ptr->link == NULL)
		cur_ptr->link = strdup (cur_ptr->feedurl);

	/* If the download function returns a NULL pointer return from here. */
	if (cur_ptr->feed == NULL) {
	if (cur_ptr->problem == 1)
			PrintUpdateError (0, cur_ptr);
		return 1;
	}
	
	/* If there is no feed, return. */
	if (cur_ptr->feed == NULL)
		return 1;
	
	if ((DeXML (cur_ptr)) != 0) {
		fprintf(stderr, "Invalid XML! Cannot parse this feed!\n");

		/* Activate feed problem flag. */
		cur_ptr->problem = 1;
		return 1;
	}
	
	/* We don't need these anymore. Free the raw XML to save some memory. */
	free (cur_ptr->feed);
	cur_ptr->feed = NULL;
		
	return 0;
}


