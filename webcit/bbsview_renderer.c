#include "webcit.h"
#include "webserver.h"
#include "groupdav.h"



/* We're jamming both of these in here so I can develop the new BBS view in-tree.
 * Define NEW_BBS_BIEW to get the new, better, but unfinished and untested version.
 *
 */

#ifndef NEW_BBS_VIEW


/*** Code for the OLD bbs view ***/


typedef struct _bbsview_stuct {
	StrBuf *BBViewToolBar;
	StrBuf *MessageDropdown;
	long *displayed_msgs;
	int a;
} bbsview_struct;

int bbsview_GetParamsGetServerCall(SharedMessageStatus *Stat, 
				   void **ViewSpecific, 
				   long oper, 
				   char *cmd, 
				   long len)
{
	bbsview_struct *VS;

	VS = (bbsview_struct*) malloc(sizeof(bbsview_struct));
	memset(VS, 0, sizeof(bbsview_struct));
	*ViewSpecific = (void*)VS;
	Stat->defaultsortorder = 1;
	Stat->startmsg = -1;
	Stat->sortit = 1;
	
	rlid[oper].cmd(cmd, len);
	
	if (havebstr("maxmsgs"))
		Stat->maxmsgs = ibstr("maxmsgs");
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

/* startmsg is an index within the message list.
 * starting_from is the Citadel message number to be supplied to a "MSGS GT" operation
 */
long DrawMessageDropdown(StrBuf *Selector, long maxmsgs, long startmsg, int nMessages, long starting_from)
{
	StrBuf *TmpBuf;
	wcsession *WCC = WC;
	void *vMsg;
	int lo, hi;
	long ret;
	long hklen;
	const char *key;
	int nItems;
	HashPos *At;
	long vector[16];
	WCTemplputParams SubTP;
	int wantmore = 1;

	memset(&SubTP, 0, sizeof(WCTemplputParams));
	SubTP.Filter.ContextType = CTX_LONGVECTOR;
	SubTP.Context = &vector;
	TmpBuf = NewStrBufPlain(NULL, SIZ);
	At = GetNewHashPos(WCC->summ, nMessages);
	nItems = GetCount(WCC->summ);
	ret = nMessages;
	vector[0] = 7;
	vector[2] = 1;
	vector[1] = startmsg;
	vector[3] = 0;
	vector[7] = starting_from;

	while (wantmore)
	{
	
		vector[3] = abs(nMessages);
		lo = GetHashPosCounter(WCC->summ, At);
		wantmore = GetNextHashPos(WCC->summ, At, &hklen, &key, &vMsg);
		if (!wantmore)
			break;
		if (nMessages > 0) {
			if (lo + nMessages >= nItems) {
				hi = nItems - 1;
				vector[3] = nItems - lo;
				if (startmsg == lo) 
					ret = vector[3];
			}
			else {
				hi = lo + nMessages - 1;
			}
		} else {
			if (lo + nMessages < -1) {
				hi = 0;
			}
			else {
				if ((lo % abs(nMessages)) != 0) {
					int offset = (lo % abs(nMessages) *
						      (nMessages / abs(nMessages)));
					hi = lo + offset;
					vector[3] = abs(offset);
					if (startmsg == lo)
						 ret = offset;
				}
				else
					hi = lo + nMessages;
			}
		}
		
		/*
		 * Bump these because although we're thinking in zero base, the user
		 * is a drooling idiot and is thinking in one base.
		 */
		vector[4] = lo + 1;
		vector[5] = hi + 1;
		vector[6] = lo;
		FlushStrBuf(TmpBuf);
		dbg_print_longvector(vector);
		DoTemplate(HKEY("select_messageindex"), TmpBuf, &SubTP);
		StrBufAppendBuf(Selector, TmpBuf, 0);
	}
	vector[6] = 0;
	FlushStrBuf(TmpBuf);
	if (maxmsgs == 9999999) {
		vector[1] = 1;
		ret = maxmsgs;
	}
	else
		vector[1] = 0;		
	vector[2] = 0;
	dbg_print_longvector(vector);
	DoTemplate(HKEY("select_messageindex_all"), TmpBuf, &SubTP);
	StrBufAppendBuf(Selector, TmpBuf, 0);
	FreeStrBuf(&TmpBuf);
	DeleteHashPos(&At);
	return ret;
}


int bbsview_PrintViewHeader(SharedMessageStatus *Stat, void **ViewSpecific)
{
	bbsview_struct *VS;
	WCTemplputParams SubTP;

	VS = (bbsview_struct*)*ViewSpecific;

	VS->BBViewToolBar = NewStrBufPlain(NULL, SIZ);
	VS->MessageDropdown = NewStrBufPlain(NULL, SIZ);

	/*** startmsg->maxmsgs = **/DrawMessageDropdown(VS->MessageDropdown, 
							Stat->maxmsgs, 
							Stat->startmsg,
							Stat->num_displayed, 
							Stat->lowest_found-1);
	if (Stat->num_displayed < 0) {
		Stat->startmsg += Stat->maxmsgs;
		if (Stat->num_displayed != Stat->maxmsgs)				
			Stat->maxmsgs = abs(Stat->maxmsgs) + 1;
		else
			Stat->maxmsgs = abs(Stat->maxmsgs);

	}
	if (Stat->nummsgs > 0) {
		memset(&SubTP, 0, sizeof(WCTemplputParams));
		SubTP.Filter.ContextType = CTX_STRBUF;
		SubTP.Context = VS->MessageDropdown;
		DoTemplate(HKEY("msg_listselector_top"), VS->BBViewToolBar, &SubTP);
		StrBufAppendBuf(WC->WBuf, VS->BBViewToolBar, 0);
		FlushStrBuf(VS->BBViewToolBar);
	}
	return 200;
}

int bbsview_LoadMsgFromServer(SharedMessageStatus *Stat, 
			      void **ViewSpecific, 
			      message_summary* Msg, 
			      int is_new, 
			      int i)
{
	bbsview_struct *VS;

	VS = (bbsview_struct*)*ViewSpecific;
	if (VS->displayed_msgs == NULL) {
		VS->displayed_msgs = malloc(sizeof(long) *
					    ((Stat->maxmsgs < Stat->nummsgs) ? 
					     Stat->maxmsgs + 1 : 
					     Stat->nummsgs + 1));
	}
	if ((i >= Stat->startmsg) && (i < Stat->startmsg + Stat->maxmsgs)) {
		VS->displayed_msgs[Stat->num_displayed] = Msg->msgnum;
		Stat->num_displayed++;
	}
	return 200;
}


int bbsview_RenderView_or_Tail(SharedMessageStatus *Stat, 
			       void **ViewSpecific, 
			       long oper)
{
	wcsession *WCC = WC;
	bbsview_struct *VS;
	WCTemplputParams SubTP;
	const StrBuf *Mime;

	VS = (bbsview_struct*)*ViewSpecific;
	if (Stat->nummsgs == 0) {
		wc_printf("<div class=\"nomsgs\"><br><em>");
		switch (oper) {
		case readnew:
			wc_printf(_("No new messages."));
			break;
		case readold:
			wc_printf(_("No old messages."));
			break;
		default:
			wc_printf(_("No messages here."));
		}
		wc_printf("</em><br></div>\n");
	}
	else 
	{
		if (VS->displayed_msgs != NULL) {
			/* if we do a split bbview in the future, begin messages div here */
			int a;/// todo	
			for (a=0; a < Stat->num_displayed; ++a) {
				read_message(WCC->WBuf, HKEY("view_message"), VS->displayed_msgs[a], NULL, &Mime);
			}
			
			/* if we do a split bbview in the future, end messages div here */
			
			free(VS->displayed_msgs);
			VS->displayed_msgs = NULL;
		}
		memset(&SubTP, 0, sizeof(WCTemplputParams));
		SubTP.Filter.ContextType = CTX_STRBUF;
		SubTP.Context = VS->MessageDropdown;
		DoTemplate(HKEY("msg_listselector_bottom"), VS->BBViewToolBar, &SubTP);
		StrBufAppendBuf(WCC->WBuf, VS->BBViewToolBar, 0);
	}
	return 0;

}


int bbsview_Cleanup(void **ViewSpecific)
{
	bbsview_struct *VS;

	VS = (bbsview_struct*)*ViewSpecific;
	wDumpContent(1);
	FreeStrBuf(&VS->BBViewToolBar);
	FreeStrBuf(&VS->MessageDropdown);
	free(VS);
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





#else /* NEW_BBS_VIEW */


/*** Code for the NEW bbs view ***/



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
 * FIXME do we even need this?
 */
int bbsview_PrintViewHeader(SharedMessageStatus *Stat, void **ViewSpecific)
{
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

	if (WC->is_ajax) {
		begin_ajax_response();		/* for non-ajax, headers are output in messages.c */
	}

	if (BBS->num_msgs < Stat->maxmsgs) {

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
	}
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
	char morediv[64];

	lprintf(9, "bbsview_RenderView_or_Tail() has been called\n");

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
		lprintf(9, "sorting %d messages\n", BBS->num_msgs);
		qsort(BBS->msgs, (size_t)(BBS->num_msgs), sizeof(long), (Stat->reverse ? bbsview_sortfunc_reverse : bbsview_sortfunc_forward));
	
		for (i=0; i<BBS->num_msgs; ++i) {
			read_message(WC->WBuf, HKEY("view_message"), BBS->msgs[i], NULL, &Mime);
		}

	}

	snprintf(morediv, sizeof morediv, "morediv%08lx%08x", time(NULL), rand());

	if (!WC->is_ajax) {	/* only supply the script during the initial page load */
	   StrBufAppendPrintf(WC->trailing_javascript,
		"	function moremsgs(target_div, gt, maxmsgs, sortorder) {				\n"
		"		$(target_div).innerHTML = '%s ... <img src=\"static/throbber.gif\">';	\n"
		"		p = 'gt=' + gt + '&maxmsgs=' + maxmsgs					\n"
		"			+ '&is_summary=0&SortOrder=' + sortorder + '&is_ajax=1'		\n"
		"			+ '&r=' + CtdlRandomString();			                \n"
		"		new Ajax.Updater(target_div, 'readgt',	 				\n"
		"			{ method: 'get', parameters: p, evalScripts: true } );		\n"
		"	}										\n"
		"",
		_("Loading")
	   );
	}

	wc_printf("<div id=\"%s\">", morediv);
	if (Stat->nummsgs > 0) {
		wc_printf("<a href=\"javascript:moremsgs('%s', %ld, %ld, %d );\">",
			morediv,
			BBS->msgs[BBS->num_msgs-1],
			Stat->maxmsgs,
			(Stat->reverse ? 2 : 1)
		);
	
		wc_printf("div \"%s\" - click for more messages<br><br><br><br>", morediv);
		wc_printf("</a>");
	}
	else {
		wc_printf("thththththat's all, folks!<br><br><br><br>");
	}
	wc_printf("</div>");

	return(0);
}


int bbsview_Cleanup(void **ViewSpecific)
{
	struct bbsview *BBS = (struct bbsview *) *ViewSpecific;
	lprintf(9, "bbsview_Cleanup() has been called\n");
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


#endif /* NEW_BBS_VIEW */
