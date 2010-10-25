/* 
 * Blog view renderer module for WebCit
 *
 * Copyright (c) 1996-2010 by the citadel.org team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "webcit.h"
#include "webserver.h"
#include "groupdav.h"


/* 
 * Array type for a blog post.  The first message is the post; the rest are comments
 */
struct blogpost {
	int top_level_id;
	long *msgs;		/* Array of msgnums for messages we are displaying */
	int num_msgs;		/* Number of msgnums stored in 'msgs' */
	int alloc_msgs;		/* Currently allocated size of array */
};


/*
 * Destructor for 'struct blogpost' which does the rendering first.
 * By rendering from here, we eliminate the need for a separate iterator, although
 * we might run into trouble when we get around to displaying newest-to-oldest...
 */
void blogpost_render_and_destroy(struct blogpost *bp) {
	const StrBuf *Mime;
	int p = 0;
	int i;

	p = atoi(BSTR("p"));	/* are we looking for a specific post? */

	if ( ((p == 0) || (p == bp->top_level_id)) && (bp->num_msgs > 0) ) {
		/* Show the top level post */
		read_message(WC->WBuf, HKEY("view_message"), bp->msgs[0], NULL, &Mime);

		if (p == 0) {
			/* Show the number of comments */
			wc_printf("<a href=\"readfwd?p=%d?gotofirst=", bp->top_level_id);
			urlescputs(ChrPtr(WC->CurRoom.name));
			wc_printf("\">%d comments</a>", bp->num_msgs - 1);
		}
		else if (bp->num_msgs < 2) {
			wc_printf("dere r no comments here!<br>\n");
		}
		else {
			wc_printf("%d comments<br>\n", bp->num_msgs - 1);
			wc_printf("<blockquote>");
			for (i=1; i<bp->num_msgs; ++i) {
				read_message(WC->WBuf, HKEY("view_message"), bp->msgs[i], NULL, &Mime);
			}
			wc_printf("</blockquote>");
		}
	}


	if (bp->alloc_msgs > 0) {
		free(bp->msgs);
	}
	free(bp);
}


/*
 * Data which gets returned from a call to blogview_learn_thread_references()
 */
struct bltr {
	int id;
	int refs;
};


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
 * Given a 'struct blogpost' containing a msgnum, populate the id
 * and refs fields by fetching them from the Citadel server
 */
struct bltr blogview_learn_thread_references(long msgnum)
{
	StrBuf *Buf;
	StrBuf *r;
	struct bltr bltr = { 0, 0 } ;
	Buf = NewStrBuf();
	r = NewStrBuf();
	serv_printf("MSG0 %ld|1", msgnum);		/* top level citadel headers only */
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL) == 1) {
		while (StrBuf_ServGetln(Buf), strcmp(ChrPtr(Buf), "000")) {
			if (!strncasecmp(ChrPtr(Buf), "msgn=", 5)) {
				StrBufCutLeft(Buf, 5);
				bltr.id = HashLittle(ChrPtr(Buf), StrLength(Buf));
			}
			else if (!strncasecmp(ChrPtr(Buf), "wefw=", 5)) {
				StrBufCutLeft(Buf, 5);		/* trim the field name */
				StrBufExtract_token(r, Buf, 0, '|');
				bltr.refs = HashLittle(ChrPtr(r), StrLength(r));
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
		Put(BLOG, (const char *)&b.id, sizeof(b.id), bp,
					(DeleteHashDataFunc)blogpost_render_and_destroy);
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
 */
int blogview_sortfunc(const void *s1, const void *s2) {
	long *l1 = (long *)(s1);
	long *l2 = (long *)(s2);

	if (*l1 > *l2) return(-1);
	if (*l1 < *l2) return(+1);
	return(0);
}


int blogview_render(SharedMessageStatus *Stat, void **ViewSpecific, long oper)
{
	/*HashList *BLOG = (HashList *) *ViewSpecific;*/

	/*
	 * No code needed here -- we render during disposition.
	 * Maybe this is the location where we want to handle pretty permalinks.
	 */

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
}
