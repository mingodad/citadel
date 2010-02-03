/* 
 * $Id$
 *
 * BBS View renderer module for WebCit
 *
 * Note: we briefly had a dynamic UI for this.  I thought it was cool, but
 * it was not received well by the user community.  If you want to play
 * with it, go get r8256 of bbsview_renderer.c and have fun.
 *
 * Copyright (c) 1996-2010 by the citadel.org team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define RANGE 5

#include "webcit.h"
#include "webserver.h"
#include "groupdav.h"

/*
 * Data which gets passed around between the various functions in this module
 *
 * We do this weird "pivot point" thing instead of starting the page numbers at 0 or 1 so that
 * the border between old and new messages always falls on a page boundary.  We'll renumber them
 * to page numbers starting at 1 when presenting them to the user.
 */
struct bbsview {
	long *msgs;		/* Array of msgnums for messages we are displaying */
	int num_msgs;		/* Number of msgnums stored in 'msgs' */
	int alloc_msgs;		/* Currently allocated size of array */
	long pivot_msgnum;	/* Page numbers are relative to this message number */
	int requested_page;	/* Which page number did the user request? */
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

		char *comma_pos = strchr(buf, ',');	/* kill first comma and everything to its right */
		if (comma_pos) {
			*comma_pos = 0;
		}

		char *colon_pos = strchr(buf, ':');	/* kill first colon and everything to its left */
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
				   long len)
{
	struct bbsview *BBS = malloc(sizeof(struct bbsview));
	memset(BBS, 0, sizeof(struct bbsview));
	*ViewSpecific = BBS;

	Stat->startmsg = -1;					/* not used here */
	Stat->sortit = 1;					/* not used here */
	Stat->num_displayed = DEFAULT_MAXMSGS;			/* not used here */
	BBS->requested_page = 0;

	if (havebstr("page")) {
		BBS->requested_page = ibstr("page");
	}
	
	if (havebstr("pivot")) {
		BBS->pivot_msgnum = ibstr("pivot");
	}
	else if (oper == 2) {	/* 2 == "read all" (otherwise we pivot at the beginning of new msgs) */
		BBS->pivot_msgnum = 0;				/* start from the top */
	}
	else {
		BBS->pivot_msgnum = bbsview_get_last_seen();
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
	}

	/* Check our buffer size */
	if (BBS->num_msgs >= BBS->alloc_msgs) {
		BBS->alloc_msgs *= 2;
		BBS->msgs = realloc(BBS->msgs, (BBS->alloc_msgs * sizeof(long)));
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
	int pivot_index = 0;
	int page_offset = 0;
	int start_index = 0;
	int end_index = 0;

	/* Cut the message list down to the requested size */
	if (Stat->nummsgs > 0) {
		lprintf(9, "sorting %d messages\n", BBS->num_msgs);
		qsort(BBS->msgs, (size_t)(BBS->num_msgs), sizeof(long), bbsview_sortfunc);

		/* Cut it down to 20 messages (or whatever value Stat->maxmsgs is set to) */

		if (BBS->num_msgs > Stat->maxmsgs) {

			/* Locate the pivot point in our message index */
			for (i=0; i<(BBS->num_msgs); ++i) {
				if (BBS->msgs[i] <= BBS->pivot_msgnum) {
					pivot_index = i;
				}
			}

			page_offset = (pivot_index / Stat->maxmsgs) + 2;

		}
	}

	start_index = pivot_index + (BBS->requested_page * Stat->maxmsgs) ;
	if (start_index < 0) start_index = 0;
	end_index = start_index + Stat->maxmsgs - 1;

	for (seq = 0; seq < 3; ++seq) {		/* cheap and sleazy way of rendering the funbar twice */

		if (seq == 1) {
			/* display the selected range of messages */

			if (Stat->nummsgs > 0) {
				wc_printf("<font size=\"-1\">\n");
				for (i=start_index; (i<=end_index && i<=BBS->num_msgs); ++i) {
					if (BBS->msgs[i] > 0L) {
						read_message(WC->WBuf, HKEY("view_message"), BBS->msgs[i], NULL, &Mime);
					}
				}
				wc_printf("</font><br>\n");
			}
		}
		else {
			/* Display the range selecto-bar */

			wc_printf("<div class=\"moreprompt\">");
			wc_printf(_("Go to page: "));

			int first = 1;
			int last = (BBS->num_msgs / Stat->maxmsgs) + 2 ;

			for (i=1; i<=last; ++i) {

				if (
					(i == first)
					|| (i == last)
					|| ((i - page_offset) == BBS->requested_page)
					|| (
						((BBS->requested_page - (i - page_offset)) < RANGE)
						&& ((BBS->requested_page - (i - page_offset)) > (0 - RANGE))
					)
				) {

					if (
						(i == last) 
						&& (last - (BBS->requested_page + page_offset) > RANGE)
					) {
						wc_printf("...&nbsp;");
					}
					if ((i - page_offset) == BBS->requested_page) {
						wc_printf("[");
					}
					else {
						wc_printf("<a href=\"readfwd?pivot=%ld?page=%d\">",
							BBS->pivot_msgnum,
							i - page_offset
						);
						wc_printf("<span class=\"moreprompt_link\">");
					}
					if (
						(i == first)
						&& ((BBS->requested_page + page_offset) > (RANGE + 1))
					) {
						wc_printf(_("First"));
					}
					else if (
						(i == last)
						&& (last - (BBS->requested_page + page_offset) > RANGE)
					) {
						wc_printf(_("Last"));
					}
					else {
						wc_printf("%d", i);
					}
					if ((i - page_offset) == BBS->requested_page) {
						wc_printf("]");
					}
					else {
						wc_printf("</span>");
						wc_printf("</a>");
					}
					if (
						(i == first)
						&& ((BBS->requested_page + page_offset) > (RANGE + 1))
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
		bbsview_LoadMsgFromServer,
		bbsview_RenderView_or_Tail,
		bbsview_Cleanup
	);
}
