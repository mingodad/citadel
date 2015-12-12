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

CtxType CTX_BLOGPOST = CTX_NONE;

typedef struct __BLOG {
	HashList *BLOGPOSTS;
	long p;
	int gotonext;
	StrBuf *Charset;
	StrBuf *Buf;
} BLOG;

/* 
 * Array type for a blog post.  The first message is the post; the rest are comments
 */
typedef struct _blogpost {
	int top_level_id;
	long *msgs;		/* Array of msgnums for messages we are displaying */
	int num_msgs;		/* Number of msgnums stored in 'msgs' */
	int alloc_msgs;		/* Currently allocated size of array */
	int unread_oments;
}blogpost;


/*
 * XML sitemap generator -- go through the message list for a Blog room
 */
void sitemap_do_blog(void) {
	wcsession *WCC = WC;
	blogpost oneBP;
	int num_msgs = 0;
	int i;
	SharedMessageStatus Stat;
	message_summary *Msg = NULL;
	StrBuf *Buf = NewStrBuf();
	StrBuf *FoundCharset = NewStrBuf();
	WCTemplputParams SubTP;

	memset(&Stat, 0, sizeof Stat);
	memset(&oneBP, 0, sizeof(blogpost));
        memset(&SubTP, 0, sizeof(WCTemplputParams));    
	StackContext(NULL, &SubTP, &oneBP, CTX_BLOGPOST, 0, NULL);

	Stat.maxload = INT_MAX;
	Stat.lowest_found = (-1);
	Stat.highest_found = (-1);
	num_msgs = load_msg_ptrs("MSGS ALL", NULL, &Stat, NULL);
	if (num_msgs < 1) return;

	for (i=0; i<num_msgs; ++i) {
		Msg = GetMessagePtrAt(i, WCC->summ);
		if (Msg != NULL) {
			ReadOneMessageSummary(Msg, FoundCharset, Buf);
			/* Show only top level posts, not comments */
			if ((Msg->reply_inreplyto_hash != 0) && (Msg->reply_references_hash == 0)) {
				oneBP.top_level_id = Msg->reply_inreplyto_hash;
				DoTemplate(HKEY("view_blog_sitemap"), WCC->WBuf, &SubTP);
			}
		}
	}
	UnStackContext(&SubTP);
	FreeStrBuf(&Buf);
	FreeStrBuf(&FoundCharset);
}



/*
 * Generate a permalink for a post
 * (Call with NULL arguments to make this function wcprintf() the permalink
 * instead of writing it to the template)
 */
void tmplput_blog_toplevel_id(StrBuf *Target, WCTemplputParams *TP) {
	blogpost *bp = (blogpost*) CTX(CTX_BLOGPOST);
	char buf[SIZ];
	snprintf(buf, SIZ, "%d", bp->top_level_id);
	StrBufAppendTemplateStr(Target, TP, buf, 0);
}

void tmplput_blog_comment_count(StrBuf *Target, WCTemplputParams *TP) {
	blogpost *bp = (blogpost*) CTX(CTX_BLOGPOST);
	char buf[SIZ];
	snprintf(buf, SIZ, "%d", bp->num_msgs -1);
	StrBufAppendTemplateStr(Target, TP, buf, 0);
}
void tmplput_blog_comment_unread_count(StrBuf *Target, WCTemplputParams *TP) {
	blogpost *bp = (blogpost*) CTX(CTX_BLOGPOST);
	char buf[SIZ];
	snprintf(buf, SIZ, "%d", bp->unread_oments);
	StrBufAppendTemplateStr(Target, TP, buf, 0);
}



/*
 * Render a single blog post and (optionally) its comments
 */
void blogpost_render(blogpost *bp, int with_comments)
{
	wcsession *WCC = WC;
	WCTemplputParams SubTP;
	const StrBuf *Mime;
	int i;

        memset(&SubTP, 0, sizeof(WCTemplputParams));    
	StackContext(NULL, &SubTP, bp, CTX_BLOGPOST, 0, NULL);

	/* Always show the top level post, unless we somehow ended up with an empty list */
	if (bp->num_msgs > 0) {
		read_message(WC->WBuf, HKEY("view_blog_post"), bp->msgs[0], NULL, &Mime);
	}

	if (with_comments) {
		/* Show any existing comments, then offer the comment box */
		DoTemplate(HKEY("view_blog_show_commentlink"), WCC->WBuf, &SubTP);

		for (i=1; i<bp->num_msgs; ++i) {
			read_message(WC->WBuf, HKEY("view_blog_comment"), bp->msgs[i], NULL, &Mime);
		}
		DoTemplate(HKEY("view_blog_comment_box"), WCC->WBuf, &SubTP);
	}

	else {
		/* Show only the number of comments */
		DoTemplate(HKEY("view_blog_show_no_comments"), WCC->WBuf, &SubTP);
	}
	UnStackContext(&SubTP);
}


/*
 * Destructor for "blogpost"
 */
void blogpost_destroy(blogpost *bp) {
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
	BL->BLOGPOSTS = NewHash(1, lFlathash);
	
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
	blogpost *bp = NULL;

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
		bp = malloc(sizeof(blogpost));
		if (!bp) return(200);
		memset(bp, 0, sizeof (blogpost));
	 	bp->top_level_id = Msg->reply_inreplyto_hash;
		Put(BL->BLOGPOSTS,
		    (const char *)&Msg->reply_inreplyto_hash,
		    sizeof(Msg->reply_inreplyto_hash),
		    bp,
		    (DeleteHashDataFunc)blogpost_destroy);
	}
	else {
		GetHash(BL->BLOGPOSTS,
			(const char *)&Msg->reply_references_hash,
			sizeof(Msg->reply_references_hash),
			(void *)&bp);
	}

	/*
	 * Now we have a 'blogpost' to which we can add a message.  It's either the
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
	blogpost * const *one = a;
	blogpost * const *two = b;

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
	wcsession *WCC = WC;
	BLOG *BL = (BLOG*) *ViewSpecific;
	HashPos *it;
	const char *Key;
	void *Data;
	long len;
	int i;
	blogpost **blogposts = NULL;
	int num_blogposts = 0;
	int num_blogposts_alloc = 0;
	int with_comments = 0;
	int firstp = 0;
	int maxp = 0;
	WCTemplputParams SubTP;
	blogpost oneBP;

        memset(&SubTP, 0, sizeof(WCTemplputParams));    
	memset(&oneBP, 0, sizeof(blogpost));

	/* Comments are shown if we are only viewing a single blog post */
	with_comments = (BL->p != 0);

	firstp = ibstr("firstp");   /* start reading at... */
	maxp   = ibstr("maxp");	    /* max posts to show... */
	if (maxp < 1) maxp = 5;	    /* default; move somewhere else? */
	putlbstr("maxp", maxp);

	it = GetNewHashPos(BL->BLOGPOSTS, 0);

	if ((BL->gotonext) && (BL->p == 0)) {
		/* did we come here via gotonext? lets find out whether
		 * this blog has just one blogpost with new comments just display 
		 * this one.
		 */
		blogpost *unread_bp = NULL;
		int unread_count = 0;
		while (GetNextHashPos(BL->BLOGPOSTS, it, &len, &Key, &Data)) {
			blogpost *one_bp = (blogpost *) Data;
			if (one_bp->unread_oments > 0) {
				unread_bp = one_bp;
				unread_count++;
			}
		}
		if (unread_count == 1) {
			blogpost_render(unread_bp, 1);

			DeleteHashPos(&it);
			return 0;
		}

		RewindHashPos(BL->BLOGPOSTS, it, 0);
	}

	/* Iterate through the hash list and copy the data pointers into an array */
	while (GetNextHashPos(BL->BLOGPOSTS, it, &len, &Key, &Data)) {
		if (num_blogposts >= num_blogposts_alloc) {
			if (num_blogposts_alloc == 0) {
				num_blogposts_alloc = 100;
			}
			else {
				num_blogposts_alloc *= 2;
			}
			blogposts = realloc(blogposts, (num_blogposts_alloc * sizeof (blogpost *)));
		}
		blogposts[num_blogposts++] = (blogpost *) Data;
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

				StackContext(NULL, &SubTP, &blogposts[j], CTX_BLOGPOST, 0, NULL);
				DoTemplate(HKEY("view_blog_post_start"), WCC->WBuf, &SubTP);
				UnStackContext(&SubTP);
			}
			if (i < (start_here + maxp)) {
				blogpost_render(blogposts[i], with_comments);
			}
			else if (i == (start_here + maxp)) {
				StackContext(NULL, &SubTP, &blogposts[i], CTX_BLOGPOST, 0, NULL);
				DoTemplate(HKEY("view_blog_post_stop"), WCC->WBuf, &SubTP);
				UnStackContext(&SubTP);
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
	DeleteHash(&BL->BLOGPOSTS);
	free(BL);
	wDumpContent(1);
	return 0;
}


void 
InitModule_BLOGVIEWRENDERERS
(void)
{
	RegisterCTX(CTX_BLOGPOST);

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

	RegisterNamespace("BLOG:TOPLEVEL:MSGID", 0, 0, tmplput_blog_toplevel_id, NULL, CTX_BLOGPOST);
	RegisterNamespace("BLOG:COMMENTS:COUNT", 0, 0, tmplput_blog_comment_count, NULL, CTX_BLOGPOST);
	RegisterNamespace("BLOG:COMMENTS:UNREAD:COUNT", 0, 0, tmplput_blog_comment_unread_count, NULL, CTX_BLOGPOST);
}
