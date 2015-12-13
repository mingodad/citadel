/* 
 * BBS View renderer module for WebCit
 *
 * Note: we briefly had a dynamic UI for this.  I thought it was cool, but
 * it was not received well by the user community.  If you want to play
 * with it, go get commit dcf99fe61379b78436c387ea3f89ebfd4ffaf635 of
 * bbsview_renderer.c and have fun.
 *
 * Copyright (c) 1996-2012 by the citadel.org team
 *
 * This program is open source software.  You can redistribute it and/or
 * modify it under the terms of the GNU General Public License, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define RANGE 5

#include "webcit.h"
#include "webserver.h"
#include "dav.h"

/*
 * Data which gets passed around between the various functions in this module
 *
 */
struct bbsview {
	long *msgs;		/* Array of msgnums for messages we are displaying */
	int num_msgs;		/* Number of msgnums stored in 'msgs' */
	long lastseen;		/* The number of the last seen message in this room */
	int alloc_msgs;		/* Currently allocated size of array */
	int requested_page;	/* Which page number did the user request? */
	int num_pages;		/* Total number of pages in this room */
	long start_reading_at;	/* Start reading at the page containing this message */
};


/*
 * Attempt to determine the closest thing to the "last seen message number" using the
 * results of the GTSN command
 */
long bbsview_get_last_seen(void)
{
	char buf[SIZ] = "0";

	serv_puts("GTSN");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '2') {
		char *colon_pos;
		char *comma_pos;

		comma_pos = strchr(buf, ',');	/* kill first comma and everything to its right */
		if (comma_pos) {
			*comma_pos = 0;
		}

		colon_pos = strchr(buf, ':');	/* kill first colon and everything to its left */
		if (colon_pos) {
			strcpy(buf, ++colon_pos);
		}
	}

	return(atol(buf));
}



/*
 * Entry point for message read operations.
 */
int bbsview_GetParamsGetServerCall(SharedMessageStatus *Stat, 
				   void **ViewSpecific, 
				   long oper, 
				   char *cmd, 
				   long len,
				   char *filter,
				   long flen)
{
	struct bbsview *BBS = malloc(sizeof(struct bbsview));
	memset(BBS, 0, sizeof(struct bbsview));
	*ViewSpecific = BBS;

	Stat->startmsg = (-1);					/* not used here */
	Stat->sortit = 1;					/* not used here */
	Stat->num_displayed = DEFAULT_MAXMSGS;			/* not used here */
	BBS->requested_page = 0;
	BBS->lastseen = bbsview_get_last_seen();
	BBS->start_reading_at = 0;

	/* By default, the requested page is the first one. */
	if (havebstr("start_reading_at")) {
		BBS->start_reading_at = lbstr("start_reading_at");
		BBS->requested_page = (-4);
	}

	/* However, if we are asked to start with a specific message number, make sure
	 * we start on the page containing that message
	 */

	/* Or, if a specific page was requested, make sure we go there */
	else if (havebstr("page")) {
		BBS->requested_page = ibstr("page");
	}

	/* Otherwise, if this is a "read new" operation, make sure we start on the page
	 * containing the first new message
	 */
	else if (oper == 3) {
		BBS->requested_page = (-3);
	}

	if (havebstr("maxmsgs")) {
		Stat->maxmsgs = ibstr("maxmsgs");
	}
	if (Stat->maxmsgs == 0) Stat->maxmsgs = DEFAULT_MAXMSGS;
	
	/* perform a "read all" call to fetch the message list -- we'll cut it down later */
	rlid[2].cmd(cmd, len);
	
	return 200;
}


/*
 * This function is called for every message in the list.
 */
int bbsview_LoadMsgFromServer(SharedMessageStatus *Stat, 
			      void **ViewSpecific, 
			      message_summary* Msg, 
			      int is_new, 
			      int i)
{
	struct bbsview *BBS = (struct bbsview *) *ViewSpecific;

	if (BBS->alloc_msgs == 0) {
		BBS->alloc_msgs = 1000;
		BBS->msgs = malloc(BBS->alloc_msgs * sizeof(long));
		memset(BBS->msgs, 0, (BBS->alloc_msgs * sizeof(long)) );
	}

	/* Check our buffer size */
	if (BBS->num_msgs >= BBS->alloc_msgs) {
		BBS->alloc_msgs *= 2;
		BBS->msgs = realloc(BBS->msgs, (BBS->alloc_msgs * sizeof(long)));
		memset(&BBS->msgs[BBS->num_msgs], 0, ((BBS->alloc_msgs - BBS->num_msgs) * sizeof(long)) );
	}

	BBS->msgs[BBS->num_msgs++] = Msg->msgnum;

	return 200;
}


int bbsview_sortfunc(const void *s1, const void *s2) {
	long l1;
	long l2;

	l1 = *(long *)(s1);
	l2 = *(long *)(s2);

	if (l1 > l2) return(+1);
	if (l1 < l2) return(-1);
	return(0);
}


int bbsview_RenderView_or_Tail(SharedMessageStatus *Stat, 
			       void **ViewSpecific, 
			       long oper)
{
	struct bbsview *BBS = (struct bbsview *) *ViewSpecific;
	int i;
	int seq;
	const StrBuf *Mime;
	int start_index = 0;
	int end_index = 0;
	int go_to_the_very_end = 0;

	if (Stat->nummsgs > 0) {
		syslog(LOG_DEBUG, "sorting %d messages\n", BBS->num_msgs);
		qsort(BBS->msgs, (size_t)(BBS->num_msgs), sizeof(long), bbsview_sortfunc);
	}

	if ((BBS->num_msgs % Stat->maxmsgs) == 0) {
		BBS->num_pages = BBS->num_msgs / Stat->maxmsgs;
	}
	else {
		BBS->num_pages = (BBS->num_msgs / Stat->maxmsgs) + 1;
	}

	/* If the requested page number is -4,
	 * it means "whichever page on which msg#xxxxx starts"
	 * Change to the page number which contains that message.
	 */
	if (BBS->requested_page == (-4)) {
		if (BBS->num_msgs == 0) {
			BBS->requested_page = 0;
		}
		else {
			for (i=0; i<BBS->num_msgs; ++i) {
				if (
					(BBS->msgs[i] >= BBS->start_reading_at)
					&& (BBS->requested_page == (-4))
				) {
					BBS->requested_page = (i / Stat->maxmsgs) ;
				}
			}
		}
	}

	/* If the requested page number is -3,
	 * it means "whichever page on which new messages start"
	 * Change that to an actual page number now.
	 */
	if (BBS->requested_page == (-3)) {
		if (BBS->num_msgs == 0) {
			/*
			 * The room is empty; just start at the top and leave it there.
			 */
			BBS->requested_page = 0;
		}
		else if (
			(BBS->num_msgs > 0) 
			&& (BBS->lastseen <= BBS->msgs[0])
		) {
			/*
			 * All messages are new; this is probably the user's first visit to the room,
			 * so start at the last page instead of showing ancient history.
			 */
			BBS->requested_page = BBS->num_pages - 1;
			go_to_the_very_end = 1;
		}
		else {
			/*
			 * Some messages are old and some are new.  Go to the start of new messages.
			 */
			for (i=0; i<BBS->num_msgs; ++i) {
				if (
					(BBS->msgs[i] > BBS->lastseen)
					&& ( (i == 0) || (BBS->msgs[i-1] <= BBS->lastseen) )
				) {
					BBS->requested_page = (i / Stat->maxmsgs) ;
				}
			}
		}
	}

	/* Still set to -3 ?  If so, that probably means that there are no new messages,
	 * so we'll go to the *end* of the final page.
	 */
	if (BBS->requested_page == (-3)) {
		if (BBS->num_msgs == 0) {
			BBS->requested_page = 0;
		}
		else {
			BBS->requested_page = BBS->num_pages - 1;
		}
	}

	/* keep the requested page within bounds */
	if (BBS->requested_page < 0) BBS->requested_page = 0;
	if (BBS->requested_page >= BBS->num_pages) BBS->requested_page = BBS->num_pages - 1;

	start_index = BBS->requested_page * Stat->maxmsgs;
	if (start_index < 0) start_index = 0;
	end_index = start_index + Stat->maxmsgs - 1;

	for (seq = 0; seq < 3; ++seq) {		/* cheap & sleazy way of rendering the page numbers twice */

		if ( (seq == 1) && (Stat->nummsgs > 0)) {
			/* display the selected range of messages */

			for (i=start_index; (i<=end_index && i<BBS->num_msgs); ++i) {
				if (
					(BBS->msgs[i] > BBS->lastseen)
					&& ( (i == 0) || (BBS->msgs[i-1] <= BBS->lastseen) )
				) {
					/* new messages start here */
					do_template("start_of_new_msgs");
					if (!go_to_the_very_end) {
						StrBufAppendPrintf(WC->trailing_javascript, "location.href=\"#newmsgs\";\n");
					}
				}
				if (BBS->msgs[i] > 0L) {
					read_message(WC->WBuf, HKEY("view_message"), BBS->msgs[i], NULL, &Mime, NULL);
				}
				if (
					(i == (BBS->num_msgs - 1))
					&& (BBS->msgs[i] <= BBS->lastseen)
				) {
					/* no new messages */
					do_template("no_new_msgs");
					if (!go_to_the_very_end) {
						StrBufAppendPrintf(WC->trailing_javascript, "location.href=\"#nonewmsgs\";\n");
					}
				}
			}
		}

		else if ( (seq == 0) || (seq == 2) ) {
			int first;
			int last;
			/* Display the selecto-bar with the page numbers */

			wc_printf("<div class=\"moreprompt\">");
			if (seq == 2) {
				wc_printf("<a name=\"end_of_msgs\">");
			}
			wc_printf(_("Go to page: "));
			if (seq == 2) {
				wc_printf("</a>");
			}

			first = 0;
			last = BBS->num_pages - 1;

			for (i=0; i<=last; ++i) {

				if (
					(i == first)
					|| (i == last)
					|| (i == BBS->requested_page)
					|| (
						((BBS->requested_page - i) < RANGE)
						&& ((BBS->requested_page - i) > (0 - RANGE))
					)
				) {

					if (
						(i == last) 
						&& (last - BBS->requested_page > RANGE)
					) {
						wc_printf("...&nbsp;");
					}
					if (i == BBS->requested_page) {
						wc_printf("[");
					}
					else {
						wc_printf("<a href=\"readfwd?go=");
						urlescputs(ChrPtr(WC->CurRoom.name));
						wc_printf("?start_reading_at=%ld\">",
							BBS->msgs[i*Stat->maxmsgs]
						);
						/* wc_printf("?page=%d\">", i); */
						wc_printf("<span class=\"moreprompt_link\">");
					}
					if (
						(i == first)
						&& (BBS->requested_page > (RANGE + 1))
					) {
						wc_printf(_("First"));
					}
					else if (
						(i == last)
						&& (last - BBS->requested_page > RANGE)
					) {
						wc_printf(_("Last"));
					}
					else {
						wc_printf("%d", i + 1);	/* change to one-based for display */
					}
					if (i == BBS->requested_page) {
						wc_printf("]");
					}
					else {
						wc_printf("</span>");
						wc_printf("</a>");
					}
					if (
						(i == first)
						&& (BBS->requested_page > (RANGE + 1))
					) {
						wc_printf("&nbsp;...");
					}
					if (i != last) {
						wc_printf("&nbsp;");
					}
				}
			}
			wc_printf("</div>\n");
		}
	}

	if (go_to_the_very_end) {
		StrBufAppendPrintf(WC->trailing_javascript, "location.href=\"#end_of_msgs\";\n");
	}
	return(0);
}


int bbsview_Cleanup(void **ViewSpecific)
{
	struct bbsview *BBS = (struct bbsview *) *ViewSpecific;

	if (BBS->alloc_msgs != 0) {
		free(BBS->msgs);
	}
	free(BBS);

	wDumpContent(1);
	return 0;
}


void 
InitModule_BBSVIEWRENDERERS
(void)
{
	RegisterReadLoopHandlerset(
		VIEW_BBS,
		bbsview_GetParamsGetServerCall,
		NULL,
		NULL,
		NULL, 
		bbsview_LoadMsgFromServer,
		bbsview_RenderView_or_Tail,
		bbsview_Cleanup
	);
}
