/* 
 * Blog view renderer module for WebCit
 *
 * Copyright (c) 1996-2011 by the citadel.org team
 *
 * This program is open source software.  You can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "webcit.h"
#include "webserver.h"
#include "groupdav.h"


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
 * Render (maybe) a single blog post and (maybe) its comments
 */
void blogpost_render(struct blogpost *bp) {
	const StrBuf *Mime;
	int p = 0;
	int i;

	p = atoi(BSTR("p"));	/* are we looking for a specific post? */
	WC->bptlid = bp->top_level_id;

	if ( ((p == 0) || (p == bp->top_level_id)) && (bp->num_msgs > 0) ) {
		/* Show the top level post */
		read_message(WC->WBuf, HKEY("view_blog_post"), bp->msgs[0], NULL, &Mime);

		if (p == 0) {
			/* Show the number of comments */
			wc_printf("<a href=\"readfwd?p=%d?go=", bp->top_level_id);
			urlescputs(ChrPtr(WC->CurRoom.name));
			wc_printf("#comments\">");
			wc_printf(_("%d comments"), bp->num_msgs - 1);
			wc_printf("</a> | <a href=\"");
			tmplput_blog_permalink(NULL, NULL);
			wc_printf("\">%s</a>", _("permalink"));
			wc_printf("<br><br><br>\n");
		}
		else if (bp->num_msgs < 2) {
			wc_printf(_("%d comments"), 0);
		}
		else {
			wc_printf("<a name=\"comments\"></a>\n");
			wc_printf(_("%d comments"), bp->num_msgs - 1);
			wc_printf(" | <a href=\"");
			tmplput_blog_permalink(NULL, NULL);
			wc_printf("\">%s</a>", _("permalink"));
			wc_printf("<br>\n");
			for (i=1; i<bp->num_msgs; ++i) {
				read_message(WC->WBuf, HKEY("view_blog_comment"), bp->msgs[i], NULL, &Mime);
			}
		}
	}

	/* offer the comment box */
	if (p == bp->top_level_id) {
		do_template("blog_comment_box");
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
				   long len)
{
	HashList *BLOG = NewHash(1, NULL);
	*ViewSpecific = BLOG;

	Stat->startmsg = (-1);					/* not used here */
	Stat->sortit = 1;					/* not used here */
	Stat->num_displayed = DEFAULT_MAXMSGS;			/* not used here */
	if (Stat->maxmsgs == 0) Stat->maxmsgs = DEFAULT_MAXMSGS;
	
	/* perform a "read all" call to fetch the message list -- we'll cut it down later */
	rlid[2].cmd(cmd, len);
	
	return 200;
}


/*
 * Given a msgnum, populate the id and refs fields of
 * a "struct bltr" by fetching them from the Citadel server
 */
struct bltr blogview_learn_thread_references(long msgnum)
{
	StrBuf *Buf;
	StrBuf *r;
	int len;
	struct bltr bltr = { 0, 0 } ;
	Buf = NewStrBuf();
	r = NewStrBuf();
	serv_printf("MSG0 %ld|1", msgnum);		/* top level citadel headers only */
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL) == 1) {
		while (len = StrBuf_ServGetln(Buf), 
		       ((len >= 0) && 
			((len != 3) || strcmp(ChrPtr(Buf), "000") )))
		{
			if (!strncasecmp(ChrPtr(Buf), "msgn=", 5)) {
				StrBufCutLeft(Buf, 5);
				bltr.id = ThreadIdHash(Buf);
			}
			else if (!strncasecmp(ChrPtr(Buf), "wefw=", 5)) {
				StrBufCutLeft(Buf, 5);		/* trim the field name */
				StrBufExtract_token(r, Buf, 0, '|');
				bltr.refs = ThreadIdHash(r);
			}
		}
	}
	FreeStrBuf(&Buf);
	FreeStrBuf(&r);
	return(bltr);
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
	HashList *BLOG = (HashList *) *ViewSpecific;
	struct bltr b;
	struct blogpost *bp = NULL;

	b = blogview_learn_thread_references(Msg->msgnum);

	/* FIXME an optimization here -- one we ought to perform -- is to exit this
	 * function immediately if the viewer is only interested in a single post and
	 * that message ID is neither the id nor the refs.  Actually, that might *be*
	 * the way to display only a single message (with or without comments).
	 */

	if (b.refs == 0) {
		bp = malloc(sizeof(struct blogpost));
		if (!bp) return(200);
		memset(bp, 0, sizeof (struct blogpost));
	 	bp->top_level_id = b.id;
		Put(BLOG, (const char *)&b.id, sizeof(b.id), bp, (DeleteHashDataFunc)blogpost_destroy);
	}
	else {
		GetHash(BLOG, (const char *)&b.refs , sizeof(b.refs), (void *)&bp);
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
	}

	return 200;
}


/*
 * Sort a list of 'struct blogpost' objects by newest-to-oldest msgnum.
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
 * Sort them, (FIXME cull by date range if needed,) and render what we want to see.
 */
int blogview_render(SharedMessageStatus *Stat, void **ViewSpecific, long oper)
{
	HashList *BLOG = (HashList *) *ViewSpecific;
	HashPos *it;
	const char *Key;
	void *Data;
	long len;
	int i;
	struct blogpost **blogposts = NULL;
	int num_blogposts = 0;
	int num_blogposts_alloc = 0;

	it = GetNewHashPos(BLOG, 0);
	while (GetNextHashPos(BLOG, it, &len, &Key, &Data)) {
		if (num_blogposts >= num_blogposts_alloc) {
			if (num_blogposts_alloc == 0) {
				num_blogposts_alloc = 100;
				blogposts = malloc((num_blogposts_alloc * sizeof (struct blogpost *)));
			}
			else {
				num_blogposts_alloc *= 2;
				blogposts = realloc(blogposts, (num_blogposts_alloc * sizeof (struct blogpost *)));
			}
		}
		blogposts[num_blogposts++] = (struct blogpost *) Data;
	}
	DeleteHashPos(&it);

	if (num_blogposts > 0) {
		qsort(blogposts, num_blogposts, sizeof(void *), blogview_sortfunc);

		/* FIXME this is where we handle date ranges etc */

		for (i=0; i<num_blogposts; ++i) {
			blogpost_render(blogposts[i]);
		}
		free(blogposts);
	}

	return(0);
}


int blogview_Cleanup(void **ViewSpecific)
{
	HashList *BLOG = (HashList *) *ViewSpecific;

	DeleteHash(&BLOG);

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
		blogview_LoadMsgFromServer,
		blogview_render,
		blogview_Cleanup
	);
	RegisterNamespace("BLOG:PERMALINK", 0, 0, tmplput_blog_permalink, NULL, CTX_NONE);
}
