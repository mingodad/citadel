/* 
 * $Id: $
 *
 * BBS View renderer module for WebCit
 *
 * Copyright (c) 1996-2009 by the citadel.org team
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

#include "webcit.h"
#include "webserver.h"
#include "groupdav.h"

/*
 * Data which gets passed around between the various functions in this module
 */
struct bbsview {
	long *msgs;		/* Array of msgnums for messages we are displaying */
	int num_msgs;		/* Number of msgnums stored in 'msgs' */
	int alloc_msgs;		/* Currently allocated size of array */
};


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

	Stat->defaultsortorder = 1;
	Stat->startmsg = -1;
	Stat->sortit = 1;
	
	rlid[oper].cmd(cmd, len);		/* this performs the server call to fetch the msg list */
	
	if (havebstr("maxmsgs")) {
		Stat->maxmsgs = ibstr("maxmsgs");
	}
	if (Stat->maxmsgs == 0) Stat->maxmsgs = DEFAULT_MAXMSGS;
	
	if (havebstr("startmsg")) {
		Stat->startmsg = lbstr("startmsg");
	}
	if (lbstr("SortOrder") == 2) {
		Stat->reverse = 1;
		Stat->num_displayed = -DEFAULT_MAXMSGS;
	}
	else {
		Stat->reverse = 0;
		Stat->num_displayed = DEFAULT_MAXMSGS;
	}

	return 200;
}


/*
 * begin_ajax_response() was moved from bbsview_LoadMsgFromServer() to here ...
 */
int bbsview_PrintViewHeader(SharedMessageStatus *Stat, void **ViewSpecific)
{
	if (WC->is_ajax) {
		begin_ajax_response();		/* for non-ajax, headers are output in messages.c */
	}

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
		BBS->alloc_msgs = Stat->maxmsgs;
		BBS->msgs = malloc(BBS->alloc_msgs * sizeof(long));
	}

	/* Theoretically this never happens because the initial allocation == maxmsgs */
	if (BBS->num_msgs >= BBS->alloc_msgs) {
		BBS->alloc_msgs *= 2;
		BBS->msgs = realloc(BBS->msgs, (BBS->alloc_msgs * sizeof(long)));
	}

	BBS->msgs[BBS->num_msgs++] = Msg->msgnum;

	return 200;
}

int bbsview_sortfunc_reverse(const void *s1, const void *s2) {
	long l1;
	long l2;

	l1 = *(long *)(s1);
	l2 = *(long *)(s2);

	if (l1 > l2) return(-1);
	if (l1 < l2) return(+1);
	return(0);
}


int bbsview_sortfunc_forward(const void *s1, const void *s2) {
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
	const StrBuf *Mime;
	char olderdiv[64];
	char newerdiv[64];
	int doing_older_messages = 0;
	int doing_newer_messages = 0;

	snprintf(olderdiv, sizeof olderdiv, "olderdiv%08lx%08x", time(NULL), rand());
	snprintf(newerdiv, sizeof newerdiv, "newerdiv%08lx%08x", time(NULL), rand());

	/* If this is the initial page load (and not an update), supply the required JavaScript code */
	if (!WC->is_ajax) {
	   StrBufAppendPrintf(WC->trailing_javascript,
		"	function moremsgs(target_div, gt_or_lt, gt_or_lt_value, maxmsgs, sortorder) {	\n"
		"		$(target_div).innerHTML = '<div class=\"moreprompt\">%s ... <img src=\"static/throbber.gif\"></div>';	\n"
		"		p = gt_or_lt + '=' + gt_or_lt_value + '&maxmsgs=' + maxmsgs		\n"
		"			+ '&is_summary=0&SortOrder=' + sortorder + '&is_ajax=1'		\n"
		"			+ '&gt_or_lt=' + gt_or_lt					\n"
		"			+ '&r=' + CtdlRandomString();			                \n"
		"		new Ajax.Updater(target_div, 'read' + gt_or_lt,	 				\n"
		"			{ method: 'get', parameters: p, evalScripts: true } );		\n"
		"	}										\n"
		"",
		_("Loading")
	   );
	}


	/* Determine whether we are in the middle of a 'click for older messages' or 'click for
	 * newer messages' operation.  If neither, then we are in the initial page load.
	 */
	if (!strcasecmp(bstr("gt_or_lt"), "lt")) {
		doing_older_messages = 1;
		doing_newer_messages = 0;
	}
	else if (!strcasecmp(bstr("gt_or_lt"), "gt")) {
		doing_older_messages = 0;
		doing_newer_messages = 1;
	}
	else {
		doing_older_messages = 0;
		doing_newer_messages = 0;
	}


	/* Cut the message list down to the requested size */
	if (Stat->nummsgs > 0) {
		lprintf(9, "sorting %d messages\n", BBS->num_msgs);
		qsort(BBS->msgs, (size_t)(BBS->num_msgs), sizeof(long), (Stat->reverse ? bbsview_sortfunc_reverse : bbsview_sortfunc_forward));

		/* Cut it down to 20 messages (or whatever value Stat->maxmsgs is set to) */

		if (BBS->num_msgs > Stat->maxmsgs) {

			if (doing_older_messages) {
				/* LT ... cut it down to the LAST 20 messages received */
				memcpy(&BBS->msgs[0], &BBS->msgs[BBS->num_msgs - Stat->maxmsgs],
					(Stat->maxmsgs * sizeof(long))
				);
				BBS->num_msgs = Stat->maxmsgs;
			}
			else {
				/* GT ... cut it down to the FIRST 20 messages received */
				BBS->num_msgs = Stat->maxmsgs;
			}
		}
	}


	/* Supply the link to prepend the previous 20 messages */

	if (doing_newer_messages == 0) {
		wc_printf("<div id=\"%s\">", olderdiv);
		/* if (Stat->nummsgs > 0) { */
		if (Stat->nummsgs > 0) {
			wc_printf("<a href=\"javascript:moremsgs('%s', 'lt', %ld, %ld, %d );\">",
				olderdiv,
				BBS->msgs[0],
				Stat->maxmsgs,
				(Stat->reverse ? 2 : 1)
			);
		
			wc_printf("<div class=\"moreprompt\">"
				"&uarr; &uarr; &uarr; %s &uarr; &uarr; &uarr;"
				"</div>", _("older messages")
			);
			wc_printf("</a>");
		}
		wc_printf("</div>");
	}



	/* Handle the empty message set gracefully... */
	if (Stat->nummsgs == 0) {
		if (!WC->is_ajax) {
			wc_printf("<div class=\"nomsgs\"><br><em>");
			wc_printf(_("No messages here."));
			wc_printf("</em><br></div>\n");
		}
	}

	/* Non-empty message set... */
	else {
		/* Render the messages */
	
		for (i=0; i<BBS->num_msgs; ++i) {
			read_message(WC->WBuf, HKEY("view_message"), BBS->msgs[i], NULL, &Mime);
		}

	}


	/* Supply the link to append the next 20 messages */

	if (doing_older_messages == 0) {
		wc_printf("<div id=\"%s\">", newerdiv);
		/* if (Stat->nummsgs > 0) { */
		if (Stat->nummsgs >= Stat->maxmsgs) {
			wc_printf("<a href=\"javascript:moremsgs('%s', 'gt', %ld, %ld, %d );\">",
				newerdiv,
				BBS->msgs[BBS->num_msgs-1],
				Stat->maxmsgs,
				(Stat->reverse ? 2 : 1)
			);
		
			wc_printf("<div class=\"moreprompt\">"
				"&darr; &darr; &darr; %s &darr; &darr; &darr;"
				"</div>", _("newer messages")
			);
			wc_printf("</a>");
		}
		else {
			long gt = 0;	/* if new messages appear later, where will they begin? */
			if (Stat->nummsgs > 0) {
				gt = BBS->msgs[BBS->num_msgs-1];
			}
			else {
				gt = atol(bstr("gt"));
			}
			wc_printf("<a href=\"javascript:moremsgs('%s', 'gt', %ld, %ld, %d );\">",
				newerdiv,
				gt,
				Stat->maxmsgs,
				(Stat->reverse ? 2 : 1)
			);
			wc_printf("<div class=\"moreprompt\">");
			wc_printf("%s", _("no more messages"));
			wc_printf("</div>");
			wc_printf("</a>");
		}
		wc_printf("</div>");
	}

	/* Leave a little padding at the bottom, but only for the initial page load -- don't keep
	 * adding it every time we extend the visible message set.
	 */
	if (!WC->is_ajax) {
		wc_printf("<br><br><br><br>");
	}

	return(0);
}


int bbsview_Cleanup(void **ViewSpecific)
{
	struct bbsview *BBS = (struct bbsview *) *ViewSpecific;
	free(BBS);

	if (WC->is_ajax) {
		end_ajax_response();
		WC->is_ajax = 0;
	}
	else {
		wDumpContent(1);
	}
	return 0;
}

void 
InitModule_BBSVIEWRENDERERS
(void)
{
	RegisterReadLoopHandlerset(
		VIEW_BBS,
		bbsview_GetParamsGetServerCall,
		bbsview_PrintViewHeader,
		bbsview_LoadMsgFromServer,
		bbsview_RenderView_or_Tail,
		bbsview_Cleanup);
}
