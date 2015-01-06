/* 
 * Blog view renderer module for WebCit
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

#include "webcit.h"
#include "webserver.h"
#include "dav.h"


typedef struct __BLOG {
	HashList *BLOG;
	long p;
	int gotonext;
	StrBuf *Charset;
	StrBuf *Buf;
} BLOG;


/*
 * Generate a permalink for a post
 * (Call with NULL arguments to make this function wcprintf() the permalink
 * instead of writing it to the template)
 */
void tmplput_blog_permalink(StrBuf *Target, WCTemplputParams *TP) {
	char perma[SIZ];
	
	strcpy(perma, "/readfwd?go=");
	urlesc(&perma[strlen(perma)], sizeof(perma)-strlen(perma), (char *)ChrPtr(WC->CurRoom.name));
	snprintf(&perma[strlen(perma)], sizeof(perma)-strlen(perma), "?p=%d", WC->bptlid);
	if (!Target) {
		wc_printf("%s", perma);
	}
	else {
		StrBufAppendPrintf(Target, "%s", perma);
	}
}


/*
 * Render a single blog post and (optionally) its comments
 */
void blogpost_render(struct blogpost *bp, int with_comments)
{
	const StrBuf *Mime;
	int i;

	WC->bptlid = bp->top_level_id;	/* This is used in templates; do not remove it */

	/* Always show the top level post, unless we somehow ended up with an empty list */
	if (bp->num_msgs > 0) {
		read_message(WC->WBuf, HKEY("view_blog_post"), bp->msgs[0], NULL, &Mime);
	}

	if (with_comments) {
		/* Show any existing comments, then offer the comment box */
		wc_printf("<a class=\"blog_show_comments_link\" name=\"comments\"></a>\n");
		wc_printf(_("%d comments"), bp->num_msgs - 1);
		wc_printf(" | <a class=\"blog_permalink_link\" href=\"");
		tmplput_blog_permalink(NULL, NULL);
		wc_printf("\">%s</a>", _("permalink"));
		wc_printf("</div>\n");
		for (i=1; i<bp->num_msgs; ++i) {
			read_message(WC->WBuf, HKEY("view_blog_comment"), bp->msgs[i], NULL, &Mime);
		}
		do_template("view_blog_comment_box");
	}

	else {
		/* Show only the number of comments */
		wc_printf("<a class=\"blog_show_comments_link\" href=\"readfwd?p=%d?go=", bp->top_level_id);
		urlescputs(ChrPtr(WC->CurRoom.name));
		wc_printf("#comments\">");
		wc_printf(_("%d comments"), bp->num_msgs - 1);
		wc_printf("</a> | <a class=\"blog_permalink_link\" href=\"");
		tmplput_blog_permalink(NULL, NULL);
		wc_printf("\">%s</a>", _("permalink"));
		wc_printf("<hr>\n</div>\n");
	}
}


/*
 * Destructor for "struct blogpost"
 */
void blogpost_destroy(struct blogpost *bp) {
	if (bp->alloc_msgs > 0) {
		free(bp->msgs);
	}
	free(bp);
}


/*
 * Entry point for message read operations.
 */
int blogview_GetParamsGetServerCall(SharedMessageStatus *Stat, 
				   void **ViewSpecific, 
				   long oper, 
				   char *cmd, 
				    long len,
				    char *filter,
				    long flen)
{
	BLOG *BL = (BLOG*) malloc(sizeof(BLOG)); 
	BL->BLOG = NewHash(1, lFlathash);
	
	/* are we looking for a specific post? */
	BL->p = lbstr("p");
	BL->gotonext = havebstr("gotonext");
	BL->Charset = NewStrBuf();
	BL->Buf = NewStrBuf();
	*ViewSpecific = BL;

	Stat->startmsg = (-1);					/* not used here */
	Stat->sortit = 1;					/* not used here */
	Stat->num_displayed = DEFAULT_MAXMSGS;			/* not used here */
	if (Stat->maxmsgs == 0) Stat->maxmsgs = DEFAULT_MAXMSGS;
	
	/* perform a "read all" call to fetch the message list -- we'll cut it down later */
	rlid[2].cmd(cmd, len);
	if (BL->gotonext)
		Stat->load_seen = 1;
	return 200;
}


/*
 * This function is called for every message in the list.
 */
int blogview_LoadMsgFromServer(SharedMessageStatus *Stat, 
			      void **ViewSpecific, 
			      message_summary* Msg, 
			      int is_new, 
			      int i)
{
	BLOG *BL = (BLOG*) *ViewSpecific;
	struct blogpost *bp = NULL;

	ReadOneMessageSummary(Msg, BL->Charset, BL->Buf);

	/* Stop processing if the viewer is only interested in a single post and
	 * that message ID is neither the id nor the refs.
	 */
	if ((BL->p != 0) &&
	    (BL->p != Msg->reply_inreplyto_hash) &&
	    (BL->p != Msg->reply_references_hash)) {
		return 200;
	}

	/*
	 * Add our little bundle of blogworthy wonderfulness to the hash table
	 */
	if (Msg->reply_references_hash == 0) {
		bp = malloc(sizeof(struct blogpost));
		if (!bp) return(200);
		memset(bp, 0, sizeof (struct blogpost));
	 	bp->top_level_id = Msg->reply_inreplyto_hash;
		Put(BL->BLOG,
		    (const char *)&Msg->reply_inreplyto_hash,
		    sizeof(Msg->reply_inreplyto_hash),
		    bp,
		    (DeleteHashDataFunc)blogpost_destroy);
	}
	else {
		GetHash(BL->BLOG,
			(const char *)&Msg->reply_references_hash,
			sizeof(Msg->reply_references_hash),
			(void *)&bp);
	}

	/*
	 * Now we have a 'struct blogpost' to which we can add a message.  It's either the
	 * blog post itself or a comment attached to it; either way, the code is the same from
	 * this point onward.
	 */
	if (bp != NULL) {
		if (bp->alloc_msgs == 0) {
			bp->alloc_msgs = 1000;
			bp->msgs = malloc(bp->alloc_msgs * sizeof(long));
			memset(bp->msgs, 0, (bp->alloc_msgs * sizeof(long)) );
		}
		if (bp->num_msgs >= bp->alloc_msgs) {
			bp->alloc_msgs *= 2;
			bp->msgs = realloc(bp->msgs, (bp->alloc_msgs * sizeof(long)));
			memset(&bp->msgs[bp->num_msgs], 0,
				((bp->alloc_msgs - bp->num_msgs) * sizeof(long)) );
		}
		bp->msgs[bp->num_msgs++] = Msg->msgnum;
		if ((Msg->Flags & MSGFLAG_READ) != 0) {
			syslog(LOG_DEBUG, "****************** unread %ld", Msg->msgnum);
			
			bp->unread_oments++;
		}
	}
	else {
		syslog(LOG_DEBUG, "** comment %ld is unparented", Msg->msgnum);
	}

	return 200;
}


/*
 * Sort a list of 'struct blogpost' pointers by newest-to-oldest msgnum.
 * With big thanks to whoever wrote http://www.c.happycodings.com/Sorting_Searching/code14.html
 */
static int blogview_sortfunc(const void *a, const void *b) { 
	struct blogpost * const *one = a;
	struct blogpost * const *two = b;

	if ( (*one)->msgs[0] > (*two)->msgs[0] ) return(-1);
	if ( (*one)->msgs[0] < (*two)->msgs[0] ) return(+1);
	return(0);
}


/*
 * All blogpost entries are now in the hash list.
 * Sort them, select the desired range, and render what we want to see.
 */
int blogview_render(SharedMessageStatus *Stat, void **ViewSpecific, long oper)
{
	BLOG *BL = (BLOG*) *ViewSpecific;
	HashPos *it;
	const char *Key;
	void *Data;
	long len;
	int i;
	struct blogpost **blogposts = NULL;
	int num_blogposts = 0;
	int num_blogposts_alloc = 0;
	int with_comments = 0;
	int firstp = 0;
	int maxp = 0;

	/* Comments are shown if we are only viewing a single blog post */
	with_comments = (BL->p != 0);

	firstp = atoi(BSTR("firstp"));	/* start reading at... */
	maxp = atoi(BSTR("maxp"));	/* max posts to show... */
	if (maxp < 1) maxp = 5;		/* default; move somewhere else? */


	//// bp->unread_oments++;

	/* Iterate through the hash list and copy the data pointers into an array */
	it = GetNewHashPos(BL->BLOG, 0);
	while (GetNextHashPos(BL->BLOG, it, &len, &Key, &Data)) {
		if (num_blogposts >= num_blogposts_alloc) {
			if (num_blogposts_alloc == 0) {
				num_blogposts_alloc = 100;
			}
			else {
				num_blogposts_alloc *= 2;
			}
			blogposts = realloc(blogposts, (num_blogposts_alloc * sizeof (struct blogpost *)));
		}
		blogposts[num_blogposts++] = (struct blogpost *) Data;
	}
	DeleteHashPos(&it);

	/* Now we have our array.  It is ONLY an array of pointers.  The objects to
	 * which they point are still owned by the hash list.
	 */
	if (num_blogposts > 0) {
		int start_here = 0;
		/* Sort newest-to-oldest */
		qsort(blogposts, num_blogposts, sizeof(void *), blogview_sortfunc);

		/* allow the user to select a starting point in the list */
		for (i=0; i<num_blogposts; ++i) {
			if (blogposts[i]->top_level_id == firstp) {
				start_here = i;
			}
		}

		/* FIXME -- allow the user (or a default setting) to select a maximum number of posts to display */

		/* Now go through the list and render what we've got */
		for (i=start_here; i<num_blogposts; ++i) {
			if ((i > 0) && (i == start_here)) {
				int j = i - maxp;
				if (j < 0) j = 0;
				wc_printf("<div class=\"newer_blog_posts\"><a href=\"readfwd?go=");
				urlescputs(ChrPtr(WC->CurRoom.name));
				wc_printf("?firstp=%d?maxp=%d\">", blogposts[j]->top_level_id, maxp);
				wc_printf("%s →</a></div>\n", _("Newer posts"));
			}
			if (i < (start_here + maxp)) {
				blogpost_render(blogposts[i], with_comments);
			}
			else if (i == (start_here + maxp)) {
				wc_printf("<div class=\"older_blog_posts\"><a href=\"readfwd?go=");
				urlescputs(ChrPtr(WC->CurRoom.name));
				wc_printf("?firstp=%d?maxp=%d\">", blogposts[i]->top_level_id, maxp);
				wc_printf("← %s</a></div>\n", _("Older posts"));
			}
		}

		/* Done.  We are only freeing the array of pointers; the data itself
		 * will be freed along with the hash list.
		 */
		free(blogposts);
	}

	return(0);
}


int blogview_Cleanup(void **ViewSpecific)
{
	BLOG *BL = (BLOG*) *ViewSpecific;

	FreeStrBuf(&BL->Buf);
	FreeStrBuf(&BL->Charset);
	DeleteHash(&BL->BLOG);
	free(BL);
	wDumpContent(1);
	return 0;
}


void 
InitModule_BLOGVIEWRENDERERS
(void)
{
	RegisterReadLoopHandlerset(
		VIEW_BLOG,
		blogview_GetParamsGetServerCall,
		NULL,
		NULL,
		NULL, 
		blogview_LoadMsgFromServer,
		blogview_render,
		blogview_Cleanup
	);
	RegisterNamespace("BLOG:PERMALINK", 0, 0, tmplput_blog_permalink, NULL, CTX_NONE);
}
