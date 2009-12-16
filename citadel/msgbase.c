/*
 * $Id$
 *
 * Implements the message store.
 * 
 * Copyright (c) 1987-2009 by the citadel.org team
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

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif


#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <regex.h>
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "serv_extensions.h"
#include "database.h"
#include "msgbase.h"
#include "support.h"
#include "sysdep_decls.h"
#include "citserver.h"
#include "room_ops.h"
#include "user_ops.h"
#include "file_ops.h"
#include "config.h"
#include "control.h"
#include "genstamp.h"
#include "internet_addressing.h"
#include "euidindex.h"
#include "journaling.h"
#include "citadel_dirs.h"
#include "clientsocket.h"
#include "serv_network.h"
#include "threads.h"

#include "ctdl_module.h"

long config_msgnum;
struct addresses_to_be_filed *atbf = NULL;

/* This temp file holds the queue of operations for AdjRefCount() */
static FILE *arcfp = NULL;

/* 
 * This really belongs in serv_network.c, but I don't know how to export
 * symbols between modules.
 */
struct FilterList *filterlist = NULL;


/*
 * These are the four-character field headers we use when outputting
 * messages in Citadel format (as opposed to RFC822 format).
 */
char *msgkeys[] = {
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
	NULL, 
	"from",
	NULL, NULL, NULL,
	"exti",
	"rfca",
	NULL, 
	"hnod",
	"msgn",
	"jrnl",
	NULL,
	"list",
	"text",
	"node",
	"room",
	"path",
	NULL,
	"rcpt",
	"spec",
	"time",
	"subj",
	NULL,
	"wefw",
	NULL,
	"cccc",
	NULL
};

/*
 * This function is self explanatory.
 * (What can I say, I'm in a weird mood today...)
 */
void remove_any_whitespace_to_the_left_or_right_of_at_symbol(char *name)
{
	int i;

	for (i = 0; i < strlen(name); ++i) {
		if (name[i] == '@') {
			while (isspace(name[i - 1]) && i > 0) {
				strcpy(&name[i - 1], &name[i]);
				--i;
			}
			while (isspace(name[i + 1])) {
				strcpy(&name[i + 1], &name[i + 2]);
			}
		}
	}
}


/*
 * Aliasing for network mail.
 * (Error messages have been commented out, because this is a server.)
 */
int alias(char *name)
{				/* process alias and routing info for mail */
	FILE *fp;
	int a, i;
	char aaa[SIZ], bbb[SIZ];
	char *ignetcfg = NULL;
	char *ignetmap = NULL;
	int at = 0;
	char node[64];
	char testnode[64];
	char buf[SIZ];

	char original_name[256];
	safestrncpy(original_name, name, sizeof original_name);

	striplt(name);
	remove_any_whitespace_to_the_left_or_right_of_at_symbol(name);
	stripallbut(name, '<', '>');

	fp = fopen(file_mail_aliases, "r");
	if (fp == NULL) {
		fp = fopen("/dev/null", "r");
	}
	if (fp == NULL) {
		return (MES_ERROR);
	}
	strcpy(aaa, "");
	strcpy(bbb, "");
	while (fgets(aaa, sizeof aaa, fp) != NULL) {
		while (isspace(name[0]))
			strcpy(name, &name[1]);
		aaa[strlen(aaa) - 1] = 0;
		strcpy(bbb, "");
		for (a = 0; a < strlen(aaa); ++a) {
			if (aaa[a] == ',') {
				strcpy(bbb, &aaa[a + 1]);
				aaa[a] = 0;
			}
		}
		if (!strcasecmp(name, aaa))
			strcpy(name, bbb);
	}
	fclose(fp);

	/* Hit the Global Address Book */
	if (CtdlDirectoryLookup(aaa, name, sizeof aaa) == 0) {
		strcpy(name, aaa);
	}

	if (strcasecmp(original_name, name)) {
		CtdlLogPrintf(CTDL_INFO, "%s is being forwarded to %s\n", original_name, name);
	}

	/* Change "user @ xxx" to "user" if xxx is an alias for this host */
	for (a=0; a<strlen(name); ++a) {
		if (name[a] == '@') {
			if (CtdlHostAlias(&name[a+1]) == hostalias_localhost) {
				name[a] = 0;
				CtdlLogPrintf(CTDL_INFO, "Changed to <%s>\n", name);
			}
		}
	}

	/* determine local or remote type, see citadel.h */
	at = haschar(name, '@');
	if (at == 0) return(MES_LOCAL);		/* no @'s - local address */
	if (at > 1) return(MES_ERROR);		/* >1 @'s - invalid address */
	remove_any_whitespace_to_the_left_or_right_of_at_symbol(name);

	/* figure out the delivery mode */
	extract_token(node, name, 1, '@', sizeof node);

	/* If there are one or more dots in the nodename, we assume that it
	 * is an FQDN and will attempt SMTP delivery to the Internet.
	 */
	if (haschar(node, '.') > 0) {
		return(MES_INTERNET);
	}

	/* Otherwise we look in the IGnet maps for a valid Citadel node.
	 * Try directly-connected nodes first...
	 */
	ignetcfg = CtdlGetSysConfig(IGNETCFG);
	for (i=0; i<num_tokens(ignetcfg, '\n'); ++i) {
		extract_token(buf, ignetcfg, i, '\n', sizeof buf);
		extract_token(testnode, buf, 0, '|', sizeof testnode);
		if (!strcasecmp(node, testnode)) {
			free(ignetcfg);
			return(MES_IGNET);
		}
	}
	free(ignetcfg);

	/*
	 * Then try nodes that are two or more hops away.
	 */
	ignetmap = CtdlGetSysConfig(IGNETMAP);
	for (i=0; i<num_tokens(ignetmap, '\n'); ++i) {
		extract_token(buf, ignetmap, i, '\n', sizeof buf);
		extract_token(testnode, buf, 0, '|', sizeof testnode);
		if (!strcasecmp(node, testnode)) {
			free(ignetmap);
			return(MES_IGNET);
		}
	}
	free(ignetmap);

	/* If we get to this point it's an invalid node name */
	return (MES_ERROR);
}


/*
 * Back end for the MSGS command: output message number only.
 */
void simple_listing(long msgnum, void *userdata)
{
	cprintf("%ld\n", msgnum);
}



/*
 * Back end for the MSGS command: output header summary.
 */
void headers_listing(long msgnum, void *userdata)
{
	struct CtdlMessage *msg;

	msg = CtdlFetchMessage(msgnum, 0);
	if (msg == NULL) {
		cprintf("%ld|0|||||\n", msgnum);
		return;
	}

	cprintf("%ld|%s|%s|%s|%s|%s|\n",
		msgnum,
		(msg->cm_fields['T'] ? msg->cm_fields['T'] : "0"),
		(msg->cm_fields['A'] ? msg->cm_fields['A'] : ""),
		(msg->cm_fields['N'] ? msg->cm_fields['N'] : ""),
		(msg->cm_fields['F'] ? msg->cm_fields['F'] : ""),
		(msg->cm_fields['U'] ? msg->cm_fields['U'] : "")
	);
	CtdlFreeMessage(msg);
}

/*
 * Back end for the MSGS command: output EUID header.
 */
void headers_euid(long msgnum, void *userdata)
{
	struct CtdlMessage *msg;

	msg = CtdlFetchMessage(msgnum, 0);
	if (msg == NULL) {
		cprintf("%ld||\n", msgnum);
		return;
	}

	cprintf("%ld|%s|\n", msgnum, (msg->cm_fields['E'] ? msg->cm_fields['E'] : ""));
	CtdlFreeMessage(msg);
}





/* Determine if a given message matches the fields in a message template.
 * Return 0 for a successful match.
 */
int CtdlMsgCmp(struct CtdlMessage *msg, struct CtdlMessage *template) {
	int i;

	/* If there aren't any fields in the template, all messages will
	 * match.
	 */
	if (template == NULL) return(0);

	/* Null messages are bogus. */
	if (msg == NULL) return(1);

	for (i='A'; i<='Z'; ++i) {
		if (template->cm_fields[i] != NULL) {
			if (msg->cm_fields[i] == NULL) {
				/* Considered equal if temmplate is empty string */
				if (IsEmptyStr(template->cm_fields[i])) continue;
				return 1;
			}
			if (strcasecmp(msg->cm_fields[i],
				template->cm_fields[i])) return 1;
		}
	}

	/* All compares succeeded: we have a match! */
	return 0;
}



/*
 * Retrieve the "seen" message list for the current room.
 */
void CtdlGetSeen(char *buf, int which_set) {
	struct visit vbuf;

	/* Learn about the user and room in question */
	CtdlGetRelationship(&vbuf, &CC->user, &CC->room);

	if (which_set == ctdlsetseen_seen)
		safestrncpy(buf, vbuf.v_seen, SIZ);
	if (which_set == ctdlsetseen_answered)
		safestrncpy(buf, vbuf.v_answered, SIZ);
}



/*
 * Manipulate the "seen msgs" string (or other message set strings)
 */
void CtdlSetSeen(long *target_msgnums, int num_target_msgnums,
		int target_setting, int which_set,
		struct ctdluser *which_user, struct ctdlroom *which_room) {
	struct cdbdata *cdbfr;
	int i, k;
	int is_seen = 0;
	int was_seen = 0;
	long lo = (-1L);
	long hi = (-1L);
	struct visit vbuf;
	long *msglist;
	int num_msgs = 0;
	StrBuf *vset;
	StrBuf *setstr;
	StrBuf *lostr;
	StrBuf *histr;
	const char *pvset;
	char *is_set;	/* actually an array of booleans */

	/* Don't bother doing *anything* if we were passed a list of zero messages */
	if (num_target_msgnums < 1) {
		return;
	}

	/* If no room was specified, we go with the current room. */
	if (!which_room) {
		which_room = &CC->room;
	}

	/* If no user was specified, we go with the current user. */
	if (!which_user) {
		which_user = &CC->user;
	}

	CtdlLogPrintf(CTDL_DEBUG, "CtdlSetSeen(%d msgs starting with %ld, %s, %d) in <%s>\n",
		num_target_msgnums, target_msgnums[0],
		(target_setting ? "SET" : "CLEAR"),
		which_set,
		which_room->QRname);

	/* Learn about the user and room in question */
	CtdlGetRelationship(&vbuf, which_user, which_room);

	/* Load the message list */
	cdbfr = cdb_fetch(CDB_MSGLISTS, &which_room->QRnumber, sizeof(long));
	if (cdbfr != NULL) {
		msglist = (long *) cdbfr->ptr;
		cdbfr->ptr = NULL;	/* CtdlSetSeen() now owns this memory */
		num_msgs = cdbfr->len / sizeof(long);
		cdb_free(cdbfr);
	} else {
		return;	/* No messages at all?  No further action. */
	}

	is_set = malloc(num_msgs * sizeof(char));
	memset(is_set, 0, (num_msgs * sizeof(char)) );

	/* Decide which message set we're manipulating */
	switch(which_set) {
	case ctdlsetseen_seen:
		vset = NewStrBufPlain(vbuf.v_seen, -1);
		break;
	case ctdlsetseen_answered:
		vset = NewStrBufPlain(vbuf.v_answered, -1);
		break;
	default:
		vset = NewStrBuf();
	}


#if 0	/* This is a special diagnostic section.  Do not allow it to run during normal operation. */
	CtdlLogPrintf(CTDL_DEBUG, "There are %d messages in the room.\n", num_msgs);
	for (i=0; i<num_msgs; ++i) {
		if ((i > 0) && (msglist[i] <= msglist[i-1])) abort();
	}
	CtdlLogPrintf(CTDL_DEBUG, "We are twiddling %d of them.\n", num_target_msgnums);
	for (k=0; k<num_target_msgnums; ++k) {
		if ((k > 0) && (target_msgnums[k] <= target_msgnums[k-1])) abort();
	}
#endif

	CtdlLogPrintf(CTDL_DEBUG, "before update: %s\n", ChrPtr(vset));

	/* Translate the existing sequence set into an array of booleans */
	setstr = NewStrBuf();
	lostr = NewStrBuf();
	histr = NewStrBuf();
	pvset = NULL;
	while (StrBufExtract_NextToken(setstr, vset, &pvset, ',') >= 0) {

		StrBufExtract_token(lostr, setstr, 0, ':');
		if (StrBufNum_tokens(setstr, ':') >= 2) {
			StrBufExtract_token(histr, setstr, 1, ':');
		}
		else {
			FlushStrBuf(histr);
			StrBufAppendBuf(histr, lostr, 0);
		}
		lo = StrTol(lostr);
		if (!strcmp(ChrPtr(histr), "*")) {
			hi = LONG_MAX;
		}
		else {
			hi = StrTol(histr);
		}

		for (i = 0; i < num_msgs; ++i) {
			if ((msglist[i] >= lo) && (msglist[i] <= hi)) {
				is_set[i] = 1;
			}
		}
	}
	FreeStrBuf(&setstr);
	FreeStrBuf(&lostr);
	FreeStrBuf(&histr);


	/* Now translate the array of booleans back into a sequence set */
	FlushStrBuf(vset);
	was_seen = 0;
	lo = (-1);
	hi = (-1);

	for (i=0; i<num_msgs; ++i) {
		is_seen = is_set[i];

		/* Apply changes */
		for (k=0; k<num_target_msgnums; ++k) {
			if (msglist[i] == target_msgnums[k]) {
				is_seen = target_setting;
			}
		}

		if ((was_seen == 0) && (is_seen == 1)) {
			lo = msglist[i];
		}
		else if ((was_seen == 1) && (is_seen == 0)) {
			hi = msglist[i-1];

			if (StrLength(vset) > 0) {
				StrBufAppendBufPlain(vset, HKEY(","), 0);
			}
			if (lo == hi) {
				StrBufAppendPrintf(vset, "%ld", hi);
			}
			else {
				StrBufAppendPrintf(vset, "%ld:%ld", lo, hi);
			}
		}

		if ((is_seen) && (i == num_msgs - 1)) {
			if (StrLength(vset) > 0) {
				StrBufAppendBufPlain(vset, HKEY(","), 0);
			}
			if ((i==0) || (was_seen == 0)) {
				StrBufAppendPrintf(vset, "%ld", msglist[i]);
			}
			else {
				StrBufAppendPrintf(vset, "%ld:%ld", lo, msglist[i]);
			}
		}

		was_seen = is_seen;
	}

	/*
	 * We will have to stuff this string back into a 4096 byte buffer, so if it's
	 * larger than that now, truncate it by removing tokens from the beginning.
	 * The limit of 100 iterations is there to prevent an infinite loop in case
	 * something unexpected happens.
	 */
	int number_of_truncations = 0;
	while ( (StrLength(vset) > SIZ) && (number_of_truncations < 100) ) {
		StrBufRemove_token(vset, 0, ',');
		++number_of_truncations;
	}

	/*
	 * If we're truncating the sequence set of messages marked with the 'seen' flag,
	 * we want the earliest messages (the truncated ones) to be marked, not unmarked.
	 * Otherwise messages at the beginning will suddenly appear to be 'unseen'.
	 */
	if ( (which_set == ctdlsetseen_seen) && (number_of_truncations > 0) ) {
		StrBuf *first_tok;
		first_tok = NewStrBuf();
		StrBufExtract_token(first_tok, vset, 0, ',');
		StrBufRemove_token(vset, 0, ',');

		if (StrBufNum_tokens(first_tok, ':') > 1) {
			StrBufRemove_token(first_tok, 0, ':');
		}
		
		StrBuf *new_set;
		new_set = NewStrBuf();
		StrBufAppendBufPlain(new_set, HKEY("1:"), 0);
		StrBufAppendBuf(new_set, first_tok, 0);
		StrBufAppendBufPlain(new_set, HKEY(":"), 0);
		StrBufAppendBuf(new_set, vset, 0);

		FreeStrBuf(&vset);
		FreeStrBuf(&first_tok);
		vset = new_set;
	}

	CtdlLogPrintf(CTDL_DEBUG, " after update: %s\n", ChrPtr(vset));

	/* Decide which message set we're manipulating */
	switch (which_set) {
		case ctdlsetseen_seen:
			safestrncpy(vbuf.v_seen, ChrPtr(vset), sizeof vbuf.v_seen);
			break;
		case ctdlsetseen_answered:
			safestrncpy(vbuf.v_answered, ChrPtr(vset), sizeof vbuf.v_answered);
			break;
	}

	free(is_set);
	free(msglist);
	CtdlSetRelationship(&vbuf, which_user, which_room);
	FreeStrBuf(&vset);
}


/*
 * API function to perform an operation for each qualifying message in the
 * current room.  (Returns the number of messages processed.)
 */
int CtdlForEachMessage(int mode, long ref, char *search_string,
			char *content_type,
			struct CtdlMessage *compare,
                        ForEachMsgCallback CallBack,
			void *userdata)
{

	int a, i, j;
	struct visit vbuf;
	struct cdbdata *cdbfr;
	long *msglist = NULL;
	int num_msgs = 0;
	int num_processed = 0;
	long thismsg;
	struct MetaData smi;
	struct CtdlMessage *msg = NULL;
	int is_seen = 0;
	long lastold = 0L;
	int printed_lastold = 0;
	int num_search_msgs = 0;
	long *search_msgs = NULL;
	regex_t re;
	int need_to_free_re = 0;
	regmatch_t pm;

	if ((content_type) && (!IsEmptyStr(content_type))) {
		regcomp(&re, content_type, 0);
		need_to_free_re = 1;
	}

	/* Learn about the user and room in question */
	CtdlGetUser(&CC->user, CC->curr_user);
	CtdlGetRelationship(&vbuf, &CC->user, &CC->room);

	/* Load the message list */
	cdbfr = cdb_fetch(CDB_MSGLISTS, &CC->room.QRnumber, sizeof(long));
	if (cdbfr != NULL) {
		msglist = (long *) cdbfr->ptr;
		num_msgs = cdbfr->len / sizeof(long);
	} else {
		if (need_to_free_re) regfree(&re);
		return 0;	/* No messages at all?  No further action. */
	}


	/*
	 * Now begin the traversal.
	 */
	if (num_msgs > 0) for (a = 0; a < num_msgs; ++a) {

		/* If the caller is looking for a specific MIME type, filter
		 * out all messages which are not of the type requested.
	 	 */
		if ((content_type != NULL) && (!IsEmptyStr(content_type))) {

			/* This call to GetMetaData() sits inside this loop
			 * so that we only do the extra database read per msg
			 * if we need to.  Doing the extra read all the time
			 * really kills the server.  If we ever need to use
			 * metadata for another search criterion, we need to
			 * move the read somewhere else -- but still be smart
			 * enough to only do the read if the caller has
			 * specified something that will need it.
			 */
			GetMetaData(&smi, msglist[a]);

			/* if (strcasecmp(smi.meta_content_type, content_type)) { old non-regex way */
			if (regexec(&re, smi.meta_content_type, 1, &pm, 0) != 0) {
				msglist[a] = 0L;
			}
		}
	}

	num_msgs = sort_msglist(msglist, num_msgs);

	/* If a template was supplied, filter out the messages which
	 * don't match.  (This could induce some delays!)
	 */
	if (num_msgs > 0) {
		if (compare != NULL) {
			for (a = 0; a < num_msgs; ++a) {
				msg = CtdlFetchMessage(msglist[a], 1);
				if (msg != NULL) {
					if (CtdlMsgCmp(msg, compare)) {
						msglist[a] = 0L;
					}
					CtdlFreeMessage(msg);
				}
			}
		}
	}

	/* If a search string was specified, get a message list from
	 * the full text index and remove messages which aren't on both
	 * lists.
	 *
	 * How this works:
	 * Since the lists are sorted and strictly ascending, and the
	 * output list is guaranteed to be shorter than or equal to the
	 * input list, we overwrite the bottom of the input list.  This
	 * eliminates the need to memmove big chunks of the list over and
	 * over again.
	 */
	if ( (num_msgs > 0) && (mode == MSGS_SEARCH) && (search_string) ) {

		/* Call search module via hook mechanism.
		 * NULL means use any search function available.
		 * otherwise replace with a char * to name of search routine
		 */
		CtdlModuleDoSearch(&num_search_msgs, &search_msgs, search_string, "fulltext");

		if (num_search_msgs > 0) {
	
			int orig_num_msgs;

			orig_num_msgs = num_msgs;
			num_msgs = 0;
			for (i=0; i<orig_num_msgs; ++i) {
				for (j=0; j<num_search_msgs; ++j) {
					if (msglist[i] == search_msgs[j]) {
						msglist[num_msgs++] = msglist[i];
					}
				}
			}
		}
		else {
			num_msgs = 0;	/* No messages qualify */
		}
		if (search_msgs != NULL) free(search_msgs);

		/* Now that we've purged messages which don't contain the search
		 * string, treat a MSGS_SEARCH just like a MSGS_ALL from this
		 * point on.
		 */
		mode = MSGS_ALL;
	}

	/*
	 * Now iterate through the message list, according to the
	 * criteria supplied by the caller.
	 */
	if (num_msgs > 0)
		for (a = 0; a < num_msgs; ++a) {
			thismsg = msglist[a];
			if (mode == MSGS_ALL) {
				is_seen = 0;
			}
			else {
				is_seen = is_msg_in_sequence_set(
							vbuf.v_seen, thismsg);
				if (is_seen) lastold = thismsg;
			}
			if ((thismsg > 0L)
			    && (

				       (mode == MSGS_ALL)
				       || ((mode == MSGS_OLD) && (is_seen))
				       || ((mode == MSGS_NEW) && (!is_seen))
				       || ((mode == MSGS_LAST) && (a >= (num_msgs - ref)))
				   || ((mode == MSGS_FIRST) && (a < ref))
				|| ((mode == MSGS_GT) && (thismsg > ref))
				|| ((mode == MSGS_EQ) && (thismsg == ref))
			    )
			    ) {
				if ((mode == MSGS_NEW) && (CC->user.flags & US_LASTOLD) && (lastold > 0L) && (printed_lastold == 0) && (!is_seen)) {
					if (CallBack)
						CallBack(lastold, userdata);
					printed_lastold = 1;
					++num_processed;
				}
				if (CallBack) CallBack(thismsg, userdata);
				++num_processed;
			}
		}
	cdb_free(cdbfr);	/* Clean up */
	if (need_to_free_re) regfree(&re);
	return num_processed;
}



/*
 * cmd_msgs()  -  get list of message #'s in this room
 *		implements the MSGS server command using CtdlForEachMessage()
 */
void cmd_msgs(char *cmdbuf)
{
	int mode = 0;
	char which[16];
	char buf[256];
	char tfield[256];
	char tvalue[256];
	int cm_ref = 0;
	int i;
	int with_template = 0;
	struct CtdlMessage *template = NULL;
	char search_string[1024];
	ForEachMsgCallback CallBack;

	extract_token(which, cmdbuf, 0, '|', sizeof which);
	cm_ref = extract_int(cmdbuf, 1);
	extract_token(search_string, cmdbuf, 1, '|', sizeof search_string);
	with_template = extract_int(cmdbuf, 2);
	switch (extract_int(cmdbuf, 3))
	{
	default:
	case MSG_HDRS_BRIEF:
		CallBack = simple_listing;
		break;
	case MSG_HDRS_ALL:
		CallBack = headers_listing;
		break;
	case MSG_HDRS_EUID:
		CallBack = headers_euid;
		break;
	}

	strcat(which, "   ");
	if (!strncasecmp(which, "OLD", 3))
		mode = MSGS_OLD;
	else if (!strncasecmp(which, "NEW", 3))
		mode = MSGS_NEW;
	else if (!strncasecmp(which, "FIRST", 5))
		mode = MSGS_FIRST;
	else if (!strncasecmp(which, "LAST", 4))
		mode = MSGS_LAST;
	else if (!strncasecmp(which, "GT", 2))
		mode = MSGS_GT;
	else if (!strncasecmp(which, "SEARCH", 6))
		mode = MSGS_SEARCH;
	else
		mode = MSGS_ALL;

	if ((!(CC->logged_in)) && (!(CC->internal_pgm))) {
		cprintf("%d not logged in\n", ERROR + NOT_LOGGED_IN);
		return;
	}

	if ( (mode == MSGS_SEARCH) && (!config.c_enable_fulltext) ) {
		cprintf("%d Full text index is not enabled on this server.\n",
			ERROR + CMD_NOT_SUPPORTED);
		return;
	}

	if (with_template) {
		unbuffer_output();
		cprintf("%d Send template then receive message list\n",
			START_CHAT_MODE);
		template = (struct CtdlMessage *)
			malloc(sizeof(struct CtdlMessage));
		memset(template, 0, sizeof(struct CtdlMessage));
		template->cm_magic = CTDLMESSAGE_MAGIC;
		template->cm_anon_type = MES_NORMAL;

		while(client_getln(buf, sizeof buf) >= 0 && strcmp(buf,"000")) {
			extract_token(tfield, buf, 0, '|', sizeof tfield);
			extract_token(tvalue, buf, 1, '|', sizeof tvalue);
			for (i='A'; i<='Z'; ++i) if (msgkeys[i]!=NULL) {
				if (!strcasecmp(tfield, msgkeys[i])) {
					template->cm_fields[i] =
						strdup(tvalue);
				}
			}
		}
		buffer_output();
	}
	else {
		cprintf("%d  \n", LISTING_FOLLOWS);
	}

	CtdlForEachMessage(mode,
			   ( (mode == MSGS_SEARCH) ? 0 : cm_ref ),
			   ( (mode == MSGS_SEARCH) ? search_string : NULL ),
			   NULL,
			   template,
			   CallBack,
			   NULL);
	if (template != NULL) CtdlFreeMessage(template);
	cprintf("000\n");
}




/* 
 * help_subst()  -  support routine for help file viewer
 */
void help_subst(char *strbuf, char *source, char *dest)
{
	char workbuf[SIZ];
	int p;

	while (p = pattern2(strbuf, source), (p >= 0)) {
		strcpy(workbuf, &strbuf[p + strlen(source)]);
		strcpy(&strbuf[p], dest);
		strcat(strbuf, workbuf);
	}
}


void do_help_subst(char *buffer)
{
	char buf2[16];

	help_subst(buffer, "^nodename", config.c_nodename);
	help_subst(buffer, "^humannode", config.c_humannode);
	help_subst(buffer, "^fqdn", config.c_fqdn);
	help_subst(buffer, "^username", CC->user.fullname);
	snprintf(buf2, sizeof buf2, "%ld", CC->user.usernum);
	help_subst(buffer, "^usernum", buf2);
	help_subst(buffer, "^sysadm", config.c_sysadm);
	help_subst(buffer, "^variantname", CITADEL);
	snprintf(buf2, sizeof buf2, "%d", config.c_maxsessions);
	help_subst(buffer, "^maxsessions", buf2);
	help_subst(buffer, "^bbsdir", ctdl_message_dir);
}



/*
 * memfmout()  -  Citadel text formatter and paginator.
 *	     Although the original purpose of this routine was to format
 *	     text to the reader's screen width, all we're really using it
 *	     for here is to format text out to 80 columns before sending it
 *	     to the client.  The client software may reformat it again.
 */
void memfmout(
	char *mptr,		/* where are we going to get our text from? */
	char subst,		/* nonzero if we should do substitutions */
	char *nl)		/* string to terminate lines with */
{
	int a, b, c;
	int real = 0;
	int old = 0;
	cit_uint8_t ch;
	char aaa[140];
	char buffer[SIZ];
	static int width = 80;

	strcpy(aaa, "");
	old = 255;
	strcpy(buffer, "");
	c = 1;			/* c is the current pos */

	do {
		if (subst) {
			while (ch = *mptr, ((ch != 0) && (strlen(buffer) < 126))) {
				ch = *mptr++;
				buffer[strlen(buffer) + 1] = 0;
				buffer[strlen(buffer)] = ch;
			}

			if (buffer[0] == '^')
				do_help_subst(buffer);

			buffer[strlen(buffer) + 1] = 0;
			a = buffer[0];
			strcpy(buffer, &buffer[1]);
		} else {
			ch = *mptr++;
		}

		old = real;
		real = ch;

		if (((ch == 13) || (ch == 10)) && (old != 13) && (old != 10)) {
			ch = 32;
		}
		if (((old == 13) || (old == 10)) && (isspace(real))) {
			cprintf("%s", nl);
			c = 1;
		}
		if (ch > 32) {
			if (((strlen(aaa) + c) > (width - 5)) && (strlen(aaa) > (width - 5))) {
				cprintf("%s%s", nl, aaa);
				c = strlen(aaa);
				aaa[0] = 0;
			}
			b = strlen(aaa);
			aaa[b] = ch;
			aaa[b + 1] = 0;
		}
		if (ch == 32) {
			if ((strlen(aaa) + c) > (width - 5)) {
				cprintf("%s", nl);
				c = 1;
			}
			cprintf("%s ", aaa);
			++c;
			c = c + strlen(aaa);
			strcpy(aaa, "");
		}
		if ((ch == 13) || (ch == 10)) {
			cprintf("%s%s", aaa, nl);
			c = 1;
			strcpy(aaa, "");
		}

	} while (ch > 0);

	cprintf("%s%s", aaa, nl);
}



/*
 * Callback function for mime parser that simply lists the part
 */
void list_this_part(char *name, char *filename, char *partnum, char *disp,
		    void *content, char *cbtype, char *cbcharset, size_t length, char *encoding,
		    char *cbid, void *cbuserdata)
{
	struct ma_info *ma;
	
	ma = (struct ma_info *)cbuserdata;
	if (ma->is_ma == 0) {
		cprintf("part=%s|%s|%s|%s|%s|%ld|%s\n",
			name, filename, partnum, disp, cbtype, (long)length, cbid);
	}
}

/* 
 * Callback function for multipart prefix
 */
void list_this_pref(char *name, char *filename, char *partnum, char *disp,
		    void *content, char *cbtype, char *cbcharset, size_t length, char *encoding,
		    char *cbid, void *cbuserdata)
{
	struct ma_info *ma;
	
	ma = (struct ma_info *)cbuserdata;
	if (!strcasecmp(cbtype, "multipart/alternative")) {
		++ma->is_ma;
	}

	if (ma->is_ma == 0) {
		cprintf("pref=%s|%s\n", partnum, cbtype);
	}
}

/* 
 * Callback function for multipart sufffix
 */
void list_this_suff(char *name, char *filename, char *partnum, char *disp,
		    void *content, char *cbtype, char *cbcharset, size_t length, char *encoding,
		    char *cbid, void *cbuserdata)
{
	struct ma_info *ma;
	
	ma = (struct ma_info *)cbuserdata;
	if (ma->is_ma == 0) {
		cprintf("suff=%s|%s\n", partnum, cbtype);
	}
	if (!strcasecmp(cbtype, "multipart/alternative")) {
		--ma->is_ma;
	}
}


/*
 * Callback function for mime parser that opens a section for downloading
 */
void mime_download(char *name, char *filename, char *partnum, char *disp,
		   void *content, char *cbtype, char *cbcharset, size_t length,
		   char *encoding, char *cbid, void *cbuserdata)
{
	int rv = 0;

	/* Silently go away if there's already a download open. */
	if (CC->download_fp != NULL)
		return;

	if (
		(!IsEmptyStr(partnum) && (!strcasecmp(CC->download_desired_section, partnum)))
	||	(!IsEmptyStr(cbid) && (!strcasecmp(CC->download_desired_section, cbid)))
	) {
		CC->download_fp = tmpfile();
		if (CC->download_fp == NULL)
			return;
	
		rv = fwrite(content, length, 1, CC->download_fp);
		fflush(CC->download_fp);
		rewind(CC->download_fp);
	
		OpenCmdResult(filename, cbtype);
	}
}



/*
 * Callback function for mime parser that outputs a section all at once.
 * We can specify the desired section by part number *or* content-id.
 */
void mime_spew_section(char *name, char *filename, char *partnum, char *disp,
		   void *content, char *cbtype, char *cbcharset, size_t length,
		   char *encoding, char *cbid, void *cbuserdata)
{
	int *found_it = (int *)cbuserdata;

	if (
		(!IsEmptyStr(partnum) && (!strcasecmp(CC->download_desired_section, partnum)))
	||	(!IsEmptyStr(cbid) && (!strcasecmp(CC->download_desired_section, cbid)))
	) {
		*found_it = 1;
		cprintf("%d %d|-1|%s|%s\n",
			BINARY_FOLLOWS,
			(int)length,
			filename,
			cbtype
		);
		client_write(content, length);
	}
}



/*
 * Load a message from disk into memory.
 * This is used by CtdlOutputMsg() and other fetch functions.
 *
 * NOTE: Caller is responsible for freeing the returned CtdlMessage struct
 *       using the CtdlMessageFree() function.
 */
struct CtdlMessage *CtdlFetchMessage(long msgnum, int with_body)
{
	struct cdbdata *dmsgtext;
	struct CtdlMessage *ret = NULL;
	char *mptr;
	char *upper_bound;
	cit_uint8_t ch;
	cit_uint8_t field_header;

	CtdlLogPrintf(CTDL_DEBUG, "CtdlFetchMessage(%ld, %d)\n", msgnum, with_body);

	dmsgtext = cdb_fetch(CDB_MSGMAIN, &msgnum, sizeof(long));
	if (dmsgtext == NULL) {
		return NULL;
	}
	mptr = dmsgtext->ptr;
	upper_bound = mptr + dmsgtext->len;

	/* Parse the three bytes that begin EVERY message on disk.
	 * The first is always 0xFF, the on-disk magic number.
	 * The second is the anonymous/public type byte.
	 * The third is the format type byte (vari, fixed, or MIME).
	 */
	ch = *mptr++;
	if (ch != 255) {
		CtdlLogPrintf(CTDL_ERR, "Message %ld appears to be corrupted.\n", msgnum);
		cdb_free(dmsgtext);
		return NULL;
	}
	ret = (struct CtdlMessage *) malloc(sizeof(struct CtdlMessage));
	memset(ret, 0, sizeof(struct CtdlMessage));

	ret->cm_magic = CTDLMESSAGE_MAGIC;
	ret->cm_anon_type = *mptr++;	/* Anon type byte */
	ret->cm_format_type = *mptr++;	/* Format type byte */

	/*
	 * The rest is zero or more arbitrary fields.  Load them in.
	 * We're done when we encounter either a zero-length field or
	 * have just processed the 'M' (message text) field.
	 */
	do {
		if (mptr >= upper_bound) {
			break;
		}
		field_header = *mptr++;
		ret->cm_fields[field_header] = strdup(mptr);

		while (*mptr++ != 0);	/* advance to next field */

	} while ((mptr < upper_bound) && (field_header != 'M'));

	cdb_free(dmsgtext);

	/* Always make sure there's something in the msg text field.  If
	 * it's NULL, the message text is most likely stored separately,
	 * so go ahead and fetch that.  Failing that, just set a dummy
	 * body so other code doesn't barf.
	 */
	if ( (ret->cm_fields['M'] == NULL) && (with_body) ) {
		dmsgtext = cdb_fetch(CDB_BIGMSGS, &msgnum, sizeof(long));
		if (dmsgtext != NULL) {
			ret->cm_fields['M'] = strdup(dmsgtext->ptr);
			cdb_free(dmsgtext);
		}
	}
	if (ret->cm_fields['M'] == NULL) {
		ret->cm_fields['M'] = strdup("\r\n\r\n (no text)\r\n");
	}

	/* Perform "before read" hooks (aborting if any return nonzero) */
	if (PerformMessageHooks(ret, EVT_BEFOREREAD) > 0) {
		CtdlFreeMessage(ret);
		return NULL;
	}

	return (ret);
}


/*
 * Returns 1 if the supplied pointer points to a valid Citadel message.
 * If the pointer is NULL or the magic number check fails, returns 0.
 */
int is_valid_message(struct CtdlMessage *msg) {
	if (msg == NULL)
		return 0;
	if ((msg->cm_magic) != CTDLMESSAGE_MAGIC) {
		CtdlLogPrintf(CTDL_WARNING, "is_valid_message() -- self-check failed\n");
		return 0;
	}
	return 1;
}


/*
 * 'Destructor' for struct CtdlMessage
 */
void CtdlFreeMessage(struct CtdlMessage *msg)
{
	int i;

	if (is_valid_message(msg) == 0) 
	{
		if (msg != NULL) free (msg);
		return;
	}

	for (i = 0; i < 256; ++i)
		if (msg->cm_fields[i] != NULL) {
			free(msg->cm_fields[i]);
		}

	msg->cm_magic = 0;	/* just in case */
	free(msg);
}


/*
 * Pre callback function for multipart/alternative
 *
 * NOTE: this differs from the standard behavior for a reason.  Normally when
 *       displaying multipart/alternative you want to show the _last_ usable
 *       format in the message.  Here we show the _first_ one, because it's
 *       usually text/plain.  Since this set of functions is designed for text
 *       output to non-MIME-aware clients, this is the desired behavior.
 *
 */
void fixed_output_pre(char *name, char *filename, char *partnum, char *disp,
	  	void *content, char *cbtype, char *cbcharset, size_t length, char *encoding,
		char *cbid, void *cbuserdata)
{
	struct ma_info *ma;
	
	ma = (struct ma_info *)cbuserdata;
	CtdlLogPrintf(CTDL_DEBUG, "fixed_output_pre() type=<%s>\n", cbtype);	
	if (!strcasecmp(cbtype, "multipart/alternative")) {
		++ma->is_ma;
		ma->did_print = 0;
	}
	if (!strcasecmp(cbtype, "message/rfc822")) {
		++ma->freeze;
	}
}

/*
 * Post callback function for multipart/alternative
 */
void fixed_output_post(char *name, char *filename, char *partnum, char *disp,
	  	void *content, char *cbtype, char *cbcharset, size_t length,
		char *encoding, char *cbid, void *cbuserdata)
{
	struct ma_info *ma;
	
	ma = (struct ma_info *)cbuserdata;
	CtdlLogPrintf(CTDL_DEBUG, "fixed_output_post() type=<%s>\n", cbtype);	
	if (!strcasecmp(cbtype, "multipart/alternative")) {
		--ma->is_ma;
		ma->did_print = 0;
	}
	if (!strcasecmp(cbtype, "message/rfc822")) {
		--ma->freeze;
	}
}

/*
 * Inline callback function for mime parser that wants to display text
 */
void fixed_output(char *name, char *filename, char *partnum, char *disp,
	  	void *content, char *cbtype, char *cbcharset, size_t length,
		char *encoding, char *cbid, void *cbuserdata)
{
	char *ptr;
	char *wptr;
	size_t wlen;
	struct ma_info *ma;

	ma = (struct ma_info *)cbuserdata;

	CtdlLogPrintf(CTDL_DEBUG,
		"fixed_output() part %s: %s (%s) (%ld bytes)\n",
		partnum, filename, cbtype, (long)length);

	/*
	 * If we're in the middle of a multipart/alternative scope and
	 * we've already printed another section, skip this one.
	 */	
   	if ( (ma->is_ma) && (ma->did_print) ) {
		CtdlLogPrintf(CTDL_DEBUG, "Skipping part %s (%s)\n", partnum, cbtype);
		return;
	}
	ma->did_print = 1;

	if ( (!strcasecmp(cbtype, "text/plain")) 
	   || (IsEmptyStr(cbtype)) ) {
		wptr = content;
		if (length > 0) {
			client_write(wptr, length);
			if (wptr[length-1] != '\n') {
				cprintf("\n");
			}
		}
		return;
	}

	if (!strcasecmp(cbtype, "text/html")) {
		ptr = html_to_ascii(content, length, 80, 0);
		wlen = strlen(ptr);
		client_write(ptr, wlen);
		if (ptr[wlen-1] != '\n') {
			cprintf("\n");
		}
		free(ptr);
		return;
	}

	if (ma->use_fo_hooks) {
		if (PerformFixedOutputHooks(cbtype, content, length)) {
		/* above function returns nonzero if it handled the part */
			return;
		}
	}

	if (strncasecmp(cbtype, "multipart/", 10)) {
		cprintf("Part %s: %s (%s) (%ld bytes)\r\n",
			partnum, filename, cbtype, (long)length);
		return;
	}
}

/*
 * The client is elegant and sophisticated and wants to be choosy about
 * MIME content types, so figure out which multipart/alternative part
 * we're going to send.
 *
 * We use a system of weights.  When we find a part that matches one of the
 * MIME types we've declared as preferential, we can store it in ma->chosen_part
 * and then set ma->chosen_pref to that MIME type's position in our preference
 * list.  If we then hit another match, we only replace the first match if
 * the preference value is lower.
 */
void choose_preferred(char *name, char *filename, char *partnum, char *disp,
	  	void *content, char *cbtype, char *cbcharset, size_t length,
		char *encoding, char *cbid, void *cbuserdata)
{
	char buf[1024];
	int i;
	struct ma_info *ma;
	
	ma = (struct ma_info *)cbuserdata;

	// NOTE: REMOVING THIS CONDITIONAL FIXES BUG 220
	//       http://bugzilla.citadel.org/show_bug.cgi?id=220
	// I don't know if there are any side effects!  Please TEST TEST TEST
	//if (ma->is_ma > 0) {

	for (i=0; i<num_tokens(CC->preferred_formats, '|'); ++i) {
		extract_token(buf, CC->preferred_formats, i, '|', sizeof buf);
		if ( (!strcasecmp(buf, cbtype)) && (!ma->freeze) ) {
			if (i < ma->chosen_pref) {
				CtdlLogPrintf(CTDL_DEBUG, "Setting chosen part: <%s>\n", partnum);
				safestrncpy(ma->chosen_part, partnum, sizeof ma->chosen_part);
				ma->chosen_pref = i;
			}
		}
	}
}

/*
 * Now that we've chosen our preferred part, output it.
 */
void output_preferred(char *name, char *filename, char *partnum, char *disp,
	  	void *content, char *cbtype, char *cbcharset, size_t length,
		char *encoding, char *cbid, void *cbuserdata)
{
	int i;
	char buf[128];
	int add_newline = 0;
	char *text_content;
	struct ma_info *ma;
	
	ma = (struct ma_info *)cbuserdata;

	/* This is not the MIME part you're looking for... */
	if (strcasecmp(partnum, ma->chosen_part)) return;

	/* If the content-type of this part is in our preferred formats
	 * list, we can simply output it verbatim.
	 */
	for (i=0; i<num_tokens(CC->preferred_formats, '|'); ++i) {
		extract_token(buf, CC->preferred_formats, i, '|', sizeof buf);
		if (!strcasecmp(buf, cbtype)) {
			/* Yeah!  Go!  W00t!! */

			text_content = (char *)content;
			if (text_content[length-1] != '\n') {
				++add_newline;
			}
			cprintf("Content-type: %s", cbtype);
			if (!IsEmptyStr(cbcharset)) {
				cprintf("; charset=%s", cbcharset);
			}
			cprintf("\nContent-length: %d\n",
				(int)(length + add_newline) );
			if (!IsEmptyStr(encoding)) {
				cprintf("Content-transfer-encoding: %s\n", encoding);
			}
			else {
				cprintf("Content-transfer-encoding: 7bit\n");
			}
			cprintf("X-Citadel-MSG4-Partnum: %s\n", partnum);
			cprintf("\n");
			client_write(content, length);
			if (add_newline) cprintf("\n");
			return;
		}
	}

	/* No translations required or possible: output as text/plain */
	cprintf("Content-type: text/plain\n\n");
	fixed_output(name, filename, partnum, disp, content, cbtype, cbcharset,
			length, encoding, cbid, cbuserdata);
}


struct encapmsg {
	char desired_section[64];
	char *msg;
	size_t msglen;
};


/*
 * Callback function for
 */
void extract_encapsulated_message(char *name, char *filename, char *partnum, char *disp,
		   void *content, char *cbtype, char *cbcharset, size_t length,
		   char *encoding, char *cbid, void *cbuserdata)
{
	struct encapmsg *encap;

	encap = (struct encapmsg *)cbuserdata;

	/* Only proceed if this is the desired section... */
	if (!strcasecmp(encap->desired_section, partnum)) {
		encap->msglen = length;
		encap->msg = malloc(length + 2);
		memcpy(encap->msg, content, length);
		return;
	}

}






int CtdlDoIHavePermissionToReadMessagesInThisRoom(void) {
	if ((!(CC->logged_in)) && (!(CC->internal_pgm))) {
		return(om_not_logged_in);
	}
	return(om_ok);
}


/*
 * Get a message off disk.  (returns om_* values found in msgbase.h)
 * 
 */
int CtdlOutputMsg(long msg_num,		/* message number (local) to fetch */
		  int mode,		/* how would you like that message? */
		  int headers_only,	/* eschew the message body? */
		  int do_proto,		/* do Citadel protocol responses? */
		  int crlf,		/* Use CRLF newlines instead of LF? */
		  char *section, 	/* NULL or a message/rfc822 section */
		  int flags		/* various flags; see msgbase.h */
) {
	struct CtdlMessage *TheMessage = NULL;
	int retcode = om_no_such_msg;
	struct encapmsg encap;
	int r;

	CtdlLogPrintf(CTDL_DEBUG, "CtdlOutputMsg(msgnum=%ld, mode=%d, section=%s)\n", 
		msg_num, mode,
		(section ? section : "<>")
	);

	r = CtdlDoIHavePermissionToReadMessagesInThisRoom();
	if (r != om_ok) {
		if (do_proto) {
			if (r == om_not_logged_in) {
				cprintf("%d Not logged in.\n", ERROR + NOT_LOGGED_IN);
			}
			else {
				cprintf("%d An unknown error has occurred.\n", ERROR);
			}
		}
		return(r);
	}

	/* FIXME: check message id against msglist for this room */

	/*
	 * Fetch the message from disk.  If we're in HEADERS_FAST mode,
	 * request that we don't even bother loading the body into memory.
	 */
	if (headers_only == HEADERS_FAST) {
		TheMessage = CtdlFetchMessage(msg_num, 0);
	}
	else {
		TheMessage = CtdlFetchMessage(msg_num, 1);
	}

	if (TheMessage == NULL) {
		if (do_proto) cprintf("%d Can't locate msg %ld on disk\n",
			ERROR + MESSAGE_NOT_FOUND, msg_num);
		return(om_no_such_msg);
	}

	/* Here is the weird form of this command, to process only an
	 * encapsulated message/rfc822 section.
	 */
	if (section) if (!IsEmptyStr(section)) if (strcmp(section, "0")) {
		memset(&encap, 0, sizeof encap);
		safestrncpy(encap.desired_section, section, sizeof encap.desired_section);
		mime_parser(TheMessage->cm_fields['M'],
			NULL,
			*extract_encapsulated_message,
			NULL, NULL, (void *)&encap, 0
		);
		CtdlFreeMessage(TheMessage);
		TheMessage = NULL;

		if (encap.msg) {
			encap.msg[encap.msglen] = 0;
			TheMessage = convert_internet_message(encap.msg);
			encap.msg = NULL;	/* no free() here, TheMessage owns it now */

			/* Now we let it fall through to the bottom of this
			 * function, because TheMessage now contains the
			 * encapsulated message instead of the top-level
			 * message.  Isn't that neat?
			 */

		}
		else {
			if (do_proto) cprintf("%d msg %ld has no part %s\n",
				ERROR + MESSAGE_NOT_FOUND, msg_num, section);
			retcode = om_no_such_msg;
		}

	}

	/* Ok, output the message now */
	retcode = CtdlOutputPreLoadedMsg(TheMessage, mode, headers_only, do_proto, crlf, flags);
	CtdlFreeMessage(TheMessage);

	return(retcode);
}


char *qp_encode_email_addrs(char *source)
{
	char user[256], node[256], name[256];
	const char headerStr[] = "=?UTF-8?Q?";
	char *Encoded;
	char *EncodedName;
	char *nPtr;
	int need_to_encode = 0;
	long SourceLen;
	long EncodedMaxLen;
	long nColons = 0;
	long *AddrPtr;
	long *AddrUtf8;
	long nAddrPtrMax = 50;
	long nmax;
	int InQuotes = 0;
	int i, n;

	if (source == NULL) return source;
	if (IsEmptyStr(source)) return source;

	AddrPtr = malloc (sizeof (long) * nAddrPtrMax);
	AddrUtf8 = malloc (sizeof (long) * nAddrPtrMax);
	memset(AddrUtf8, 0, sizeof (long) * nAddrPtrMax);
	*AddrPtr = 0;
	i = 0;
	while (!IsEmptyStr (&source[i])) {
		if (nColons >= nAddrPtrMax){
			long *ptr;

			ptr = (long *) malloc(sizeof (long) * nAddrPtrMax * 2);
			memcpy (ptr, AddrPtr, sizeof (long) * nAddrPtrMax);
			free (AddrPtr), AddrPtr = ptr;

			ptr = (long *) malloc(sizeof (long) * nAddrPtrMax * 2);
			memset(&ptr[nAddrPtrMax], 0, 
			       sizeof (long) * nAddrPtrMax);

			memcpy (ptr, AddrUtf8, sizeof (long) * nAddrPtrMax);
			free (AddrUtf8), AddrUtf8 = ptr;
			nAddrPtrMax *= 2;				
		}
		if (((unsigned char) source[i] < 32) || 
		    ((unsigned char) source[i] > 126)) {
			need_to_encode = 1;
			AddrUtf8[nColons] = 1;
		}
		if (source[i] == '"')
			InQuotes = !InQuotes;
		if (!InQuotes && source[i] == ',') {
			AddrPtr[nColons] = i;
			nColons++;
		}
		i++;
	}
	if (need_to_encode == 0) {
		free(AddrPtr);
		free(AddrUtf8);
		return source;
	}

	SourceLen = i;
	EncodedMaxLen = nColons * (sizeof(headerStr) + 3) + SourceLen * 3;
	Encoded = (char*) malloc (EncodedMaxLen);

	for (i = 0; i < nColons; i++)
		source[AddrPtr[i]++] = '\0';

	nPtr = Encoded;
	*nPtr = '\0';
	for (i = 0; i < nColons && nPtr != NULL; i++) {
		nmax = EncodedMaxLen - (nPtr - Encoded);
		if (AddrUtf8[i]) {
			process_rfc822_addr(&source[AddrPtr[i]], 
					    user,
					    node,
					    name);
			/* TODO: libIDN here ! */
			if (IsEmptyStr(name)) {
				n = snprintf(nPtr, nmax, 
					     (i==0)?"%s@%s" : ",%s@%s",
					     user, node);
			}
			else {
				EncodedName = rfc2047encode(name, strlen(name));			
				n = snprintf(nPtr, nmax, 
					     (i==0)?"%s <%s@%s>" : ",%s <%s@%s>",
					     EncodedName, user, node);
				free(EncodedName);
			}
		}
		else { 
			n = snprintf(nPtr, nmax, 
				     (i==0)?"%s" : ",%s",
				     &source[AddrPtr[i]]);
		}
		if (n > 0 )
			nPtr += n;
		else { 
			char *ptr, *nnPtr;
			ptr = (char*) malloc(EncodedMaxLen * 2);
			memcpy(ptr, Encoded, EncodedMaxLen);
			nnPtr = ptr + (nPtr - Encoded), nPtr = nnPtr;
			free(Encoded), Encoded = ptr;
			EncodedMaxLen *= 2;
			i--; /* do it once more with properly lengthened buffer */
		}
	}
	for (i = 0; i < nColons; i++)
		source[--AddrPtr[i]] = ',';
	free(AddrUtf8);
	free(AddrPtr);
	return Encoded;
}


/* If the last item in a list of recipients was truncated to a partial address,
 * remove it completely in order to avoid choking libSieve
 */
void sanitize_truncated_recipient(char *str)
{
	if (!str) return;
	if (num_tokens(str, ',') < 2) return;

	int len = strlen(str);
	if (len < 900) return;
	if (len > 998) str[998] = 0;

	char *cptr = strrchr(str, ',');
	if (!cptr) return;

	char *lptr = strchr(cptr, '<');
	char *rptr = strchr(cptr, '>');

	if ( (lptr) && (rptr) && (rptr > lptr) ) return;

	*cptr = 0;
}



/*
 * Get a message off disk.  (returns om_* values found in msgbase.h)
 */
int CtdlOutputPreLoadedMsg(
		struct CtdlMessage *TheMessage,
		int mode,		/* how would you like that message? */
		int headers_only,	/* eschew the message body? */
		int do_proto,		/* do Citadel protocol responses? */
		int crlf,		/* Use CRLF newlines instead of LF? */
		int flags		/* should the bessage be exported clean? */
) {
	int i, j, k;
	char buf[SIZ];
	cit_uint8_t ch, prev_ch;
	char allkeys[30];
	char display_name[256];
	char *mptr, *mpptr;
	char *nl;	/* newline string */
	int suppress_f = 0;
	int subject_found = 0;
	struct ma_info ma;

	/* Buffers needed for RFC822 translation.  These are all filled
	 * using functions that are bounds-checked, and therefore we can
	 * make them substantially smaller than SIZ.
	 */
	char suser[100];
	char luser[100];
	char fuser[100];
	char snode[100];
	char mid[100];
	char datestamp[100];

	CtdlLogPrintf(CTDL_DEBUG, "CtdlOutputPreLoadedMsg(TheMessage=%s, %d, %d, %d, %d\n",
		((TheMessage == NULL) ? "NULL" : "not null"),
		mode, headers_only, do_proto, crlf);

	strcpy(mid, "unknown");
	nl = (crlf ? "\r\n" : "\n");

	if (!is_valid_message(TheMessage)) {
		CtdlLogPrintf(CTDL_ERR,
			"ERROR: invalid preloaded message for output\n");
		cit_backtrace ();
	 	return(om_no_such_msg);
	}

	/* Suppress envelope recipients if required to avoid disclosing BCC addresses.
	 * Pad it with spaces in order to avoid changing the RFC822 length of the message.
	 */
	if ( (flags & SUPPRESS_ENV_TO) && (TheMessage->cm_fields['V'] != NULL) ) {
		memset(TheMessage->cm_fields['V'], ' ', strlen(TheMessage->cm_fields['V']));
	}
		
	/* Are we downloading a MIME component? */
	if (mode == MT_DOWNLOAD) {
		if (TheMessage->cm_format_type != FMT_RFC822) {
			if (do_proto)
				cprintf("%d This is not a MIME message.\n",
				ERROR + ILLEGAL_VALUE);
		} else if (CC->download_fp != NULL) {
			if (do_proto) cprintf(
				"%d You already have a download open.\n",
				ERROR + RESOURCE_BUSY);
		} else {
			/* Parse the message text component */
			mptr = TheMessage->cm_fields['M'];
			mime_parser(mptr, NULL, *mime_download, NULL, NULL, NULL, 0);
			/* If there's no file open by this time, the requested
			 * section wasn't found, so print an error
			 */
			if (CC->download_fp == NULL) {
				if (do_proto) cprintf(
					"%d Section %s not found.\n",
					ERROR + FILE_NOT_FOUND,
					CC->download_desired_section);
			}
		}
		return((CC->download_fp != NULL) ? om_ok : om_mime_error);
	}

	/* MT_SPEW_SECTION is like MT_DOWNLOAD except it outputs the whole MIME part
	 * in a single server operation instead of opening a download file.
	 */
	if (mode == MT_SPEW_SECTION) {
		if (TheMessage->cm_format_type != FMT_RFC822) {
			if (do_proto)
				cprintf("%d This is not a MIME message.\n",
				ERROR + ILLEGAL_VALUE);
		} else {
			/* Parse the message text component */
			int found_it = 0;

			mptr = TheMessage->cm_fields['M'];
			mime_parser(mptr, NULL, *mime_spew_section, NULL, NULL, (void *)&found_it, 0);
			/* If section wasn't found, print an error
			 */
			if (!found_it) {
				if (do_proto) cprintf(
					"%d Section %s not found.\n",
					ERROR + FILE_NOT_FOUND,
					CC->download_desired_section);
			}
		}
		return((CC->download_fp != NULL) ? om_ok : om_mime_error);
	}

	/* now for the user-mode message reading loops */
	if (do_proto) cprintf("%d msg:\n", LISTING_FOLLOWS);

	/* Does the caller want to skip the headers? */
	if (headers_only == HEADERS_NONE) goto START_TEXT;

	/* Tell the client which format type we're using. */
	if ( (mode == MT_CITADEL) && (do_proto) ) {
		cprintf("type=%d\n", TheMessage->cm_format_type);
	}

	/* nhdr=yes means that we're only displaying headers, no body */
	if ( (TheMessage->cm_anon_type == MES_ANONONLY)
	   && ((mode == MT_CITADEL) || (mode == MT_MIME))
	   && (do_proto)
	   ) {
		cprintf("nhdr=yes\n");
	}

	/* begin header processing loop for Citadel message format */

	if ((mode == MT_CITADEL) || (mode == MT_MIME)) {

		safestrncpy(display_name, "<unknown>", sizeof display_name);
		if (TheMessage->cm_fields['A']) {
			strcpy(buf, TheMessage->cm_fields['A']);
			if (TheMessage->cm_anon_type == MES_ANONONLY) {
				safestrncpy(display_name, "****", sizeof display_name);
			}
			else if (TheMessage->cm_anon_type == MES_ANONOPT) {
				safestrncpy(display_name, "anonymous", sizeof display_name);
			}
			else {
				safestrncpy(display_name, buf, sizeof display_name);
			}
			if ((is_room_aide())
			    && ((TheMessage->cm_anon_type == MES_ANONONLY)
			     || (TheMessage->cm_anon_type == MES_ANONOPT))) {
				size_t tmp = strlen(display_name);
				snprintf(&display_name[tmp],
					 sizeof display_name - tmp,
					 " [%s]", buf);
			}
		}

		/* Don't show Internet address for users on the
		 * local Citadel network.
		 */
		suppress_f = 0;
		if (TheMessage->cm_fields['N'] != NULL)
		   if (!IsEmptyStr(TheMessage->cm_fields['N']))
		      if (haschar(TheMessage->cm_fields['N'], '.') == 0) {
			suppress_f = 1;
		}

		/* Now spew the header fields in the order we like them. */
		safestrncpy(allkeys, FORDER, sizeof allkeys);
		for (i=0; i<strlen(allkeys); ++i) {
			k = (int) allkeys[i];
			if (k != 'M') {
				if ( (TheMessage->cm_fields[k] != NULL)
				   && (msgkeys[k] != NULL) ) {
					if ((k == 'V') || (k == 'R') || (k == 'Y')) {
						sanitize_truncated_recipient(TheMessage->cm_fields[k]);
					}
					if (k == 'A') {
						if (do_proto) cprintf("%s=%s\n",
							msgkeys[k],
							display_name);
					}
					else if ((k == 'F') && (suppress_f)) {
						/* do nothing */
					}
					/* Masquerade display name if needed */
					else {
						if (do_proto) cprintf("%s=%s\n",
							msgkeys[k],
							TheMessage->cm_fields[k]
					);
					}
				}
			}
		}

	}

	/* begin header processing loop for RFC822 transfer format */

	strcpy(suser, "");
	strcpy(luser, "");
	strcpy(fuser, "");
	strcpy(snode, NODENAME);
	if (mode == MT_RFC822) {
		for (i = 0; i < 256; ++i) {
			if (TheMessage->cm_fields[i]) {
				mptr = mpptr = TheMessage->cm_fields[i];
				
				if (i == 'A') {
					safestrncpy(luser, mptr, sizeof luser);
					safestrncpy(suser, mptr, sizeof suser);
				}
				else if (i == 'Y') {
					if ((flags & QP_EADDR) != 0) {
						mptr = qp_encode_email_addrs(mptr);
					}
					sanitize_truncated_recipient(mptr);
					cprintf("CC: %s%s", mptr, nl);
				}
				else if (i == 'P') {
					cprintf("Return-Path: %s%s", mptr, nl);
				}
				else if (i == 'L') {
					cprintf("List-ID: %s%s", mptr, nl);
				}
				else if (i == 'V') {
					if ((flags & QP_EADDR) != 0) 
						mptr = qp_encode_email_addrs(mptr);
					cprintf("Envelope-To: %s%s", mptr, nl);
				}
				else if (i == 'U') {
					cprintf("Subject: %s%s", mptr, nl);
					subject_found = 1;
				}
				else if (i == 'I')
					safestrncpy(mid, mptr, sizeof mid);
				else if (i == 'F')
					safestrncpy(fuser, mptr, sizeof fuser);
				/* else if (i == 'O')
					cprintf("X-Citadel-Room: %s%s",
						mptr, nl); */
				else if (i == 'N')
					safestrncpy(snode, mptr, sizeof snode);
				else if (i == 'R')
				{
					if (haschar(mptr, '@') == 0)
					{
						sanitize_truncated_recipient(mptr);
						cprintf("To: %s@%s", mptr, config.c_fqdn);
						cprintf("%s", nl);
					}
					else
					{
						if ((flags & QP_EADDR) != 0) {
							mptr = qp_encode_email_addrs(mptr);
						}
						sanitize_truncated_recipient(mptr);
						cprintf("To: %s", mptr);
						cprintf("%s", nl);
					}
				}
				else if (i == 'T') {
					datestring(datestamp, sizeof datestamp,
						atol(mptr), DATESTRING_RFC822);
					cprintf("Date: %s%s", datestamp, nl);
				}
				else if (i == 'W') {
					cprintf("References: ");
					k = num_tokens(mptr, '|');
					for (j=0; j<k; ++j) {
						extract_token(buf, mptr, j, '|', sizeof buf);
						cprintf("<%s>", buf);
						if (j == (k-1)) {
							cprintf("%s", nl);
						}
						else {
							cprintf(" ");
						}
					}
				}
				if (mptr != mpptr)
					free (mptr);
			}
		}
		if (subject_found == 0) {
			cprintf("Subject: (no subject)%s", nl);
		}
	}

	for (i=0; !IsEmptyStr(&suser[i]); ++i) {
		suser[i] = tolower(suser[i]);
		if (!isalnum(suser[i])) suser[i]='_';
	}

	if (mode == MT_RFC822) {
		if (!strcasecmp(snode, NODENAME)) {
			safestrncpy(snode, FQDN, sizeof snode);
		}

		/* Construct a fun message id */
		cprintf("Message-ID: <%s", mid);/// todo: this possibly breaks threadding mails.
		if (strchr(mid, '@')==NULL) {
			cprintf("@%s", snode);
		}
		cprintf(">%s", nl);

		if (!is_room_aide() && (TheMessage->cm_anon_type == MES_ANONONLY)) {
			cprintf("From: \"----\" <x@x.org>%s", nl);
		}
		else if (!is_room_aide() && (TheMessage->cm_anon_type == MES_ANONOPT)) {
			cprintf("From: \"anonymous\" <x@x.org>%s", nl);
		}
		else if (!IsEmptyStr(fuser)) {
			cprintf("From: \"%s\" <%s>%s", luser, fuser, nl);
		}
		else {
			cprintf("From: \"%s\" <%s@%s>%s", luser, suser, snode, nl);
		}

		/* Blank line signifying RFC822 end-of-headers */
		if (TheMessage->cm_format_type != FMT_RFC822) {
			cprintf("%s", nl);
		}
	}

	/* end header processing loop ... at this point, we're in the text */
START_TEXT:
	if (headers_only == HEADERS_FAST) goto DONE;
	mptr = TheMessage->cm_fields['M'];

	/* Tell the client about the MIME parts in this message */
	if (TheMessage->cm_format_type == FMT_RFC822) {
		if ( (mode == MT_CITADEL) || (mode == MT_MIME) ) {
			memset(&ma, 0, sizeof(struct ma_info));
			mime_parser(mptr, NULL,
				(do_proto ? *list_this_part : NULL),
				(do_proto ? *list_this_pref : NULL),
				(do_proto ? *list_this_suff : NULL),
				(void *)&ma, 0);
		}
		else if (mode == MT_RFC822) {	/* unparsed RFC822 dump */
			char *start_of_text = NULL;
			start_of_text = strstr(mptr, "\n\r\n");
			if (start_of_text == NULL) start_of_text = strstr(mptr, "\n\n");
			if (start_of_text == NULL) start_of_text = mptr;
			++start_of_text;
			start_of_text = strstr(start_of_text, "\n");
			++start_of_text;

			char outbuf[1024];
			int outlen = 0;
			int nllen = strlen(nl);
			prev_ch = 0;
			while (ch=*mptr, ch!=0) {
				if (ch==13) {
					/* do nothing */
				}
				else {
					if (
						((headers_only == HEADERS_NONE) && (mptr >= start_of_text))
					   ||	((headers_only == HEADERS_ONLY) && (mptr < start_of_text))
					   ||	((headers_only != HEADERS_NONE) && (headers_only != HEADERS_ONLY))
					) {
						if (ch == 10) {
							sprintf(&outbuf[outlen], "%s", nl);
							outlen += nllen;
						}
						else {
							outbuf[outlen++] = ch;
						}
					}
				}
				if (flags & ESC_DOT)
				{
					if ((prev_ch == 10) && (ch == '.') && ((*(mptr+1) == 13) || (*(mptr+1) == 10)))
					{
						outbuf[outlen++] = '.';
					}
				}
				prev_ch = ch;
				++mptr;
				if (outlen > 1000) {
					client_write(outbuf, outlen);
					outlen = 0;
				}
			}
			if (outlen > 0) {
				client_write(outbuf, outlen);
				outlen = 0;
			}

			goto DONE;
		}
	}

	if (headers_only == HEADERS_ONLY) {
		goto DONE;
	}

	/* signify start of msg text */
	if ( (mode == MT_CITADEL) || (mode == MT_MIME) ) {
		if (do_proto) cprintf("text\n");
	}

	/* If the format type on disk is 1 (fixed-format), then we want
	 * everything to be output completely literally ... regardless of
	 * what message transfer format is in use.
	 */
	if (TheMessage->cm_format_type == FMT_FIXED) {
		int buflen;
		if (mode == MT_MIME) {
			cprintf("Content-type: text/plain\n\n");
		}
		*buf = '\0';
		buflen = 0;
		while (ch = *mptr++, ch > 0) {
			if (ch == 13)
				ch = 10;
			if ((ch == 10) || (buflen > 250)) {
				buf[buflen] = '\0';
				cprintf("%s%s", buf, nl);
				*buf = '\0';
				buflen = 0;
			} else {
				buf[buflen] = ch;
				buflen++;
			}
		}
		buf[buflen] = '\0';
		if (!IsEmptyStr(buf))
			cprintf("%s%s", buf, nl);
	}

	/* If the message on disk is format 0 (Citadel vari-format), we
	 * output using the formatter at 80 columns.  This is the final output
	 * form if the transfer format is RFC822, but if the transfer format
	 * is Citadel proprietary, it'll still work, because the indentation
	 * for new paragraphs is correct and the client will reformat the
	 * message to the reader's screen width.
	 */
	if (TheMessage->cm_format_type == FMT_CITADEL) {
		if (mode == MT_MIME) {
			cprintf("Content-type: text/x-citadel-variformat\n\n");
		}
		memfmout(mptr, 0, nl);
	}

	/* If the message on disk is format 4 (MIME), we've gotta hand it
	 * off to the MIME parser.  The client has already been told that
	 * this message is format 1 (fixed format), so the callback function
	 * we use will display those parts as-is.
	 */
	if (TheMessage->cm_format_type == FMT_RFC822) {
		memset(&ma, 0, sizeof(struct ma_info));

		if (mode == MT_MIME) {
			ma.use_fo_hooks = 0;
			strcpy(ma.chosen_part, "1");
			ma.chosen_pref = 9999;
			mime_parser(mptr, NULL,
				*choose_preferred, *fixed_output_pre,
				*fixed_output_post, (void *)&ma, 0);
			mime_parser(mptr, NULL,
				*output_preferred, NULL, NULL, (void *)&ma, CC->msg4_dont_decode);
		}
		else {
			ma.use_fo_hooks = 1;
			mime_parser(mptr, NULL,
				*fixed_output, *fixed_output_pre,
				*fixed_output_post, (void *)&ma, 0);
		}

	}

DONE:	/* now we're done */
	if (do_proto) cprintf("000\n");
	return(om_ok);
}



/*
 * display a message (mode 0 - Citadel proprietary)
 */
void cmd_msg0(char *cmdbuf)
{
	long msgid;
	int headers_only = HEADERS_ALL;

	msgid = extract_long(cmdbuf, 0);
	headers_only = extract_int(cmdbuf, 1);

	CtdlOutputMsg(msgid, MT_CITADEL, headers_only, 1, 0, NULL, 0);
	return;
}


/*
 * display a message (mode 2 - RFC822)
 */
void cmd_msg2(char *cmdbuf)
{
	long msgid;
	int headers_only = HEADERS_ALL;

	msgid = extract_long(cmdbuf, 0);
	headers_only = extract_int(cmdbuf, 1);

	CtdlOutputMsg(msgid, MT_RFC822, headers_only, 1, 1, NULL, 0);
}



/* 
 * display a message (mode 3 - IGnet raw format - internal programs only)
 */
void cmd_msg3(char *cmdbuf)
{
	long msgnum;
	struct CtdlMessage *msg = NULL;
	struct ser_ret smr;

	if (CC->internal_pgm == 0) {
		cprintf("%d This command is for internal programs only.\n",
			ERROR + HIGHER_ACCESS_REQUIRED);
		return;
	}

	msgnum = extract_long(cmdbuf, 0);
	msg = CtdlFetchMessage(msgnum, 1);
	if (msg == NULL) {
		cprintf("%d Message %ld not found.\n", 
			ERROR + MESSAGE_NOT_FOUND, msgnum);
		return;
	}

	serialize_message(&smr, msg);
	CtdlFreeMessage(msg);

	if (smr.len == 0) {
		cprintf("%d Unable to serialize message\n",
			ERROR + INTERNAL_ERROR);
		return;
	}

	cprintf("%d %ld\n", BINARY_FOLLOWS, (long)smr.len);
	client_write((char *)smr.ser, (int)smr.len);
	free(smr.ser);
}



/* 
 * Display a message using MIME content types
 */
void cmd_msg4(char *cmdbuf)
{
	long msgid;
	char section[64];

	msgid = extract_long(cmdbuf, 0);
	extract_token(section, cmdbuf, 1, '|', sizeof section);
	CtdlOutputMsg(msgid, MT_MIME, 0, 1, 0, (section[0] ? section : NULL) , 0);
}



/* 
 * Client tells us its preferred message format(s)
 */
void cmd_msgp(char *cmdbuf)
{
	if (!strcasecmp(cmdbuf, "dont_decode")) {
		CC->msg4_dont_decode = 1;
		cprintf("%d MSG4 will not pre-decode messages.\n", CIT_OK);
	}
	else {
		safestrncpy(CC->preferred_formats, cmdbuf, sizeof(CC->preferred_formats));
		cprintf("%d Preferred MIME formats have been set.\n", CIT_OK);
	}
}


/*
 * Open a component of a MIME message as a download file 
 */
void cmd_opna(char *cmdbuf)
{
	long msgid;
	char desired_section[128];

	msgid = extract_long(cmdbuf, 0);
	extract_token(desired_section, cmdbuf, 1, '|', sizeof desired_section);
	safestrncpy(CC->download_desired_section, desired_section,
		sizeof CC->download_desired_section);
	CtdlOutputMsg(msgid, MT_DOWNLOAD, 0, 1, 1, NULL, 0);
}			


/*
 * Open a component of a MIME message and transmit it all at once
 */
void cmd_dlat(char *cmdbuf)
{
	long msgid;
	char desired_section[128];

	msgid = extract_long(cmdbuf, 0);
	extract_token(desired_section, cmdbuf, 1, '|', sizeof desired_section);
	safestrncpy(CC->download_desired_section, desired_section,
		sizeof CC->download_desired_section);
	CtdlOutputMsg(msgid, MT_SPEW_SECTION, 0, 1, 1, NULL, 0);
}			


/*
 * Save one or more message pointers into a specified room
 * (Returns 0 for success, nonzero for failure)
 * roomname may be NULL to use the current room
 *
 * Note that the 'supplied_msg' field may be set to NULL, in which case
 * the message will be fetched from disk, by number, if we need to perform
 * replication checks.  This adds an additional database read, so if the
 * caller already has the message in memory then it should be supplied.  (Obviously
 * this mode of operation only works if we're saving a single message.)
 */
int CtdlSaveMsgPointersInRoom(char *roomname, long newmsgidlist[], int num_newmsgs,
				int do_repl_check, struct CtdlMessage *supplied_msg)
{
	int i, j, unique;
	char hold_rm[ROOMNAMELEN];
	struct cdbdata *cdbfr;
	int num_msgs;
	long *msglist;
	long highest_msg = 0L;

	long msgid = 0;
	struct CtdlMessage *msg = NULL;

	long *msgs_to_be_merged = NULL;
	int num_msgs_to_be_merged = 0;

	CtdlLogPrintf(CTDL_DEBUG,
		"CtdlSaveMsgPointersInRoom(room=%s, num_msgs=%d, repl=%d)\n",
		roomname, num_newmsgs, do_repl_check);

	strcpy(hold_rm, CC->room.QRname);

	/* Sanity checks */
	if (newmsgidlist == NULL) return(ERROR + INTERNAL_ERROR);
	if (num_newmsgs < 1) return(ERROR + INTERNAL_ERROR);
	if (num_newmsgs > 1) supplied_msg = NULL;

	/* Now the regular stuff */
	if (CtdlGetRoomLock(&CC->room,
	   ((roomname != NULL) ? roomname : CC->room.QRname) )
	   != 0) {
		CtdlLogPrintf(CTDL_ERR, "No such room <%s>\n", roomname);
		return(ERROR + ROOM_NOT_FOUND);
	}


	msgs_to_be_merged = malloc(sizeof(long) * num_newmsgs);
	num_msgs_to_be_merged = 0;


	cdbfr = cdb_fetch(CDB_MSGLISTS, &CC->room.QRnumber, sizeof(long));
	if (cdbfr == NULL) {
		msglist = NULL;
		num_msgs = 0;
	} else {
		msglist = (long *) cdbfr->ptr;
		cdbfr->ptr = NULL;	/* CtdlSaveMsgPointerInRoom() now owns this memory */
		num_msgs = cdbfr->len / sizeof(long);
		cdb_free(cdbfr);
	}


	/* Create a list of msgid's which were supplied by the caller, but do
	 * not already exist in the target room.  It is absolutely taboo to
	 * have more than one reference to the same message in a room.
	 */
	for (i=0; i<num_newmsgs; ++i) {
		unique = 1;
		if (num_msgs > 0) for (j=0; j<num_msgs; ++j) {
			if (msglist[j] == newmsgidlist[i]) {
				unique = 0;
			}
		}
		if (unique) {
			msgs_to_be_merged[num_msgs_to_be_merged++] = newmsgidlist[i];
		}
	}

	CtdlLogPrintf(9, "%d unique messages to be merged\n", num_msgs_to_be_merged);

	/*
	 * Now merge the new messages
	 */
	msglist = realloc(msglist, (sizeof(long) * (num_msgs + num_msgs_to_be_merged)) );
	if (msglist == NULL) {
		CtdlLogPrintf(CTDL_ALERT, "ERROR: can't realloc message list!\n");
	}
	memcpy(&msglist[num_msgs], msgs_to_be_merged, (sizeof(long) * num_msgs_to_be_merged) );
	num_msgs += num_msgs_to_be_merged;

	/* Sort the message list, so all the msgid's are in order */
	num_msgs = sort_msglist(msglist, num_msgs);

	/* Determine the highest message number */
	highest_msg = msglist[num_msgs - 1];

	/* Write it back to disk. */
	cdb_store(CDB_MSGLISTS, &CC->room.QRnumber, (int)sizeof(long),
		  msglist, (int)(num_msgs * sizeof(long)));

	/* Free up the memory we used. */
	free(msglist);

	/* Update the highest-message pointer and unlock the room. */
	CC->room.QRhighest = highest_msg;
	CtdlPutRoomLock(&CC->room);

	/* Perform replication checks if necessary */
	if ( (DoesThisRoomNeedEuidIndexing(&CC->room)) && (do_repl_check) ) {
		CtdlLogPrintf(CTDL_DEBUG, "CtdlSaveMsgPointerInRoom() doing repl checks\n");

		for (i=0; i<num_msgs_to_be_merged; ++i) {
			msgid = msgs_to_be_merged[i];
	
			if (supplied_msg != NULL) {
				msg = supplied_msg;
			}
			else {
				msg = CtdlFetchMessage(msgid, 0);
			}
	
			if (msg != NULL) {
				ReplicationChecks(msg);
		
				/* If the message has an Exclusive ID, index that... */
				if (msg->cm_fields['E'] != NULL) {
					index_message_by_euid(msg->cm_fields['E'], &CC->room, msgid);
				}

				/* Free up the memory we may have allocated */
				if (msg != supplied_msg) {
					CtdlFreeMessage(msg);
				}
			}
	
		}
	}

	else {
		CtdlLogPrintf(CTDL_DEBUG, "CtdlSaveMsgPointerInRoom() skips repl checks\n");
	}

	/* Submit this room for processing by hooks */
	PerformRoomHooks(&CC->room);

	/* Go back to the room we were in before we wandered here... */
	CtdlGetRoom(&CC->room, hold_rm);

	/* Bump the reference count for all messages which were merged */
	for (i=0; i<num_msgs_to_be_merged; ++i) {
		AdjRefCount(msgs_to_be_merged[i], +1);
	}

	/* Free up memory... */
	if (msgs_to_be_merged != NULL) {
		free(msgs_to_be_merged);
	}

	/* Return success. */
	return (0);
}


/*
 * This is the same as CtdlSaveMsgPointersInRoom() but it only accepts
 * a single message.
 */
int CtdlSaveMsgPointerInRoom(char *roomname, long msgid,
			int do_repl_check, struct CtdlMessage *supplied_msg)
{
	return CtdlSaveMsgPointersInRoom(roomname, &msgid, 1, do_repl_check, supplied_msg);
}




/*
 * Message base operation to save a new message to the message store
 * (returns new message number)
 *
 * This is the back end for CtdlSubmitMsg() and should not be directly
 * called by server-side modules.
 *
 */
long send_message(struct CtdlMessage *msg) {
	long newmsgid;
	long retval;
	char msgidbuf[256];
	struct ser_ret smr;
	int is_bigmsg = 0;
	char *holdM = NULL;

	/* Get a new message number */
	newmsgid = get_new_message_number();
	snprintf(msgidbuf, sizeof msgidbuf, "%010ld@%s", newmsgid, config.c_fqdn);

	/* Generate an ID if we don't have one already */
	if (msg->cm_fields['I']==NULL) {
		msg->cm_fields['I'] = strdup(msgidbuf);
	}

	/* If the message is big, set its body aside for storage elsewhere */
	if (msg->cm_fields['M'] != NULL) {
		if (strlen(msg->cm_fields['M']) > BIGMSG) {
			is_bigmsg = 1;
			holdM = msg->cm_fields['M'];
			msg->cm_fields['M'] = NULL;
		}
	}

	/* Serialize our data structure for storage in the database */	
	serialize_message(&smr, msg);

	if (is_bigmsg) {
		msg->cm_fields['M'] = holdM;
	}

	if (smr.len == 0) {
		cprintf("%d Unable to serialize message\n",
			ERROR + INTERNAL_ERROR);
		return (-1L);
	}

	/* Write our little bundle of joy into the message base */
	if (cdb_store(CDB_MSGMAIN, &newmsgid, (int)sizeof(long),
		      smr.ser, smr.len) < 0) {
		CtdlLogPrintf(CTDL_ERR, "Can't store message\n");
		retval = 0L;
	} else {
		if (is_bigmsg) {
			cdb_store(CDB_BIGMSGS,
				&newmsgid,
				(int)sizeof(long),
				holdM,
				(strlen(holdM) + 1)
			);
		}
		retval = newmsgid;
	}

	/* Free the memory we used for the serialized message */
	free(smr.ser);

	/* Return the *local* message ID to the caller
	 * (even if we're storing an incoming network message)
	 */
	return(retval);
}



/*
 * Serialize a struct CtdlMessage into the format used on disk and network.
 * 
 * This function loads up a "struct ser_ret" (defined in server.h) which
 * contains the length of the serialized message and a pointer to the
 * serialized message in memory.  THE LATTER MUST BE FREED BY THE CALLER.
 */
void serialize_message(struct ser_ret *ret,		/* return values */
			struct CtdlMessage *msg)	/* unserialized msg */
{
	size_t wlen, fieldlen;
	int i;
	static char *forder = FORDER;

	/*
	 * Check for valid message format
	 */
	if (is_valid_message(msg) == 0) {
		CtdlLogPrintf(CTDL_ERR, "serialize_message() aborting due to invalid message\n");
		ret->len = 0;
		ret->ser = NULL;
		return;
	}

	ret->len = 3;
	for (i=0; i<26; ++i) if (msg->cm_fields[(int)forder[i]] != NULL)
		ret->len = ret->len +
			strlen(msg->cm_fields[(int)forder[i]]) + 2;

	ret->ser = malloc(ret->len);
	if (ret->ser == NULL) {
		CtdlLogPrintf(CTDL_ERR, "serialize_message() malloc(%ld) failed: %s\n",
			(long)ret->len, strerror(errno));
		ret->len = 0;
		ret->ser = NULL;
		return;
	}

	ret->ser[0] = 0xFF;
	ret->ser[1] = msg->cm_anon_type;
	ret->ser[2] = msg->cm_format_type;
	wlen = 3;

	for (i=0; i<26; ++i) if (msg->cm_fields[(int)forder[i]] != NULL) {
		fieldlen = strlen(msg->cm_fields[(int)forder[i]]);
		ret->ser[wlen++] = (char)forder[i];
		safestrncpy((char *)&ret->ser[wlen], msg->cm_fields[(int)forder[i]], fieldlen+1);
		wlen = wlen + fieldlen + 1;
	}
	if (ret->len != wlen) CtdlLogPrintf(CTDL_ERR, "ERROR: len=%ld wlen=%ld\n",
		(long)ret->len, (long)wlen);

	return;
}


/*
 * Serialize a struct CtdlMessage into the format used on disk and network.
 * 
 * This function loads up a "struct ser_ret" (defined in server.h) which
 * contains the length of the serialized message and a pointer to the
 * serialized message in memory.  THE LATTER MUST BE FREED BY THE CALLER.
 */
void dump_message(struct CtdlMessage *msg,	/* unserialized msg */
		  long Siz)                     /* how many chars ? */
{
	size_t wlen;
	int i;
	static char *forder = FORDER;
	char *buf;

	/*
	 * Check for valid message format
	 */
	if (is_valid_message(msg) == 0) {
		CtdlLogPrintf(CTDL_ERR, "dump_message() aborting due to invalid message\n");
		return;
	}

	buf = (char*) malloc (Siz + 1);

	wlen = 3;
	
	for (i=0; i<26; ++i) if (msg->cm_fields[(int)forder[i]] != NULL) {
			snprintf (buf, Siz, " msg[%c] = %s ...\n", (char) forder[i], 
				   msg->cm_fields[(int)forder[i]]);
			client_write (buf, strlen(buf));
		}

	return;
}



/*
 * Check to see if any messages already exist in the current room which
 * carry the same Exclusive ID as this one.  If any are found, delete them.
 */
void ReplicationChecks(struct CtdlMessage *msg) {
	long old_msgnum = (-1L);

	if (DoesThisRoomNeedEuidIndexing(&CC->room) == 0) return;

	CtdlLogPrintf(CTDL_DEBUG, "Performing replication checks in <%s>\n",
		CC->room.QRname);

	/* No exclusive id?  Don't do anything. */
	if (msg == NULL) return;
	if (msg->cm_fields['E'] == NULL) return;
	if (IsEmptyStr(msg->cm_fields['E'])) return;
	/*CtdlLogPrintf(CTDL_DEBUG, "Exclusive ID: <%s> for room <%s>\n",
		msg->cm_fields['E'], CC->room.QRname);*/

	old_msgnum = CtdlLocateMessageByEuid(msg->cm_fields['E'], &CC->room);
	if (old_msgnum > 0L) {
		CtdlLogPrintf(CTDL_DEBUG, "ReplicationChecks() replacing message %ld\n", old_msgnum);
		CtdlDeleteMessages(CC->room.QRname, &old_msgnum, 1, "");
	}
}



/*
 * Save a message to disk and submit it into the delivery system.
 */
long CtdlSubmitMsg(struct CtdlMessage *msg,	/* message to save */
		   struct recptypes *recps,	/* recipients (if mail) */
		   char *force,			/* force a particular room? */
		   int flags			/* should the message be exported clean? */
) {
	char submit_filename[128];
	char generated_timestamp[32];
	char hold_rm[ROOMNAMELEN];
	char actual_rm[ROOMNAMELEN];
	char force_room[ROOMNAMELEN];
	char content_type[SIZ];			/* We have to learn this */
	char recipient[SIZ];
	long newmsgid;
	char *mptr = NULL;
	struct ctdluser userbuf;
	int a, i;
	struct MetaData smi;
	FILE *network_fp = NULL;
	static int seqnum = 1;
	struct CtdlMessage *imsg = NULL;
	char *instr = NULL;
	size_t instr_alloc = 0;
	struct ser_ret smr;
	char *hold_R, *hold_D;
	char *collected_addresses = NULL;
	struct addresses_to_be_filed *aptr = NULL;
	char *saved_rfc822_version = NULL;
	int qualified_for_journaling = 0;
	CitContext *CCC = CC;		/* CachedCitContext - performance boost */
	char bounce_to[1024] = "";
	size_t tmp = 0;
	int rv = 0;

	CtdlLogPrintf(CTDL_DEBUG, "CtdlSubmitMsg() called\n");
	if (is_valid_message(msg) == 0) return(-1);	/* self check */

	/* If this message has no timestamp, we take the liberty of
	 * giving it one, right now.
	 */
	if (msg->cm_fields['T'] == NULL) {
		snprintf(generated_timestamp, sizeof generated_timestamp, "%ld", (long)time(NULL));
		msg->cm_fields['T'] = strdup(generated_timestamp);
	}

	/* If this message has no path, we generate one.
	 */
	if (msg->cm_fields['P'] == NULL) {
		if (msg->cm_fields['A'] != NULL) {
			msg->cm_fields['P'] = strdup(msg->cm_fields['A']);
			for (a=0; !IsEmptyStr(&msg->cm_fields['P'][a]); ++a) {
				if (isspace(msg->cm_fields['P'][a])) {
					msg->cm_fields['P'][a] = ' ';
				}
			}
		}
		else {
			msg->cm_fields['P'] = strdup("unknown");
		}
	}

	if (force == NULL) {
		strcpy(force_room, "");
	}
	else {
		strcpy(force_room, force);
	}

	/* Learn about what's inside, because it's what's inside that counts */
	if (msg->cm_fields['M'] == NULL) {
		CtdlLogPrintf(CTDL_ERR, "ERROR: attempt to save message with NULL body\n");
		return(-2);
	}

	switch (msg->cm_format_type) {
	case 0:
		strcpy(content_type, "text/x-citadel-variformat");
		break;
	case 1:
		strcpy(content_type, "text/plain");
		break;
	case 4:
		strcpy(content_type, "text/plain");
		mptr = bmstrcasestr(msg->cm_fields['M'], "Content-type:");
		if (mptr != NULL) {
			char *aptr;
			safestrncpy(content_type, &mptr[13], sizeof content_type);
			striplt(content_type);
			aptr = content_type;
			while (!IsEmptyStr(aptr)) {
				if ((*aptr == ';')
				    || (*aptr == ' ')
				    || (*aptr == 13)
				    || (*aptr == 10)) {
					*aptr = 0;
				}
				else aptr++;
			}
		}
	}

	/* Goto the correct room */
	CtdlLogPrintf(CTDL_DEBUG, "Selected room %s\n", (recps) ? CCC->room.QRname : SENTITEMS);
	strcpy(hold_rm, CCC->room.QRname);
	strcpy(actual_rm, CCC->room.QRname);
	if (recps != NULL) {
		strcpy(actual_rm, SENTITEMS);
	}

	/* If the user is a twit, move to the twit room for posting */
	if (TWITDETECT) {
		if (CCC->user.axlevel == 2) {
			strcpy(hold_rm, actual_rm);
			strcpy(actual_rm, config.c_twitroom);
			CtdlLogPrintf(CTDL_DEBUG, "Diverting to twit room\n");
		}
	}

	/* ...or if this message is destined for Aide> then go there. */
	if (!IsEmptyStr(force_room)) {
		strcpy(actual_rm, force_room);
	}

	CtdlLogPrintf(CTDL_DEBUG, "Final selection: %s\n", actual_rm);
	if (strcasecmp(actual_rm, CCC->room.QRname)) {
		/* CtdlGetRoom(&CCC->room, actual_rm); */
		CtdlUserGoto(actual_rm, 0, 1, NULL, NULL);
	}

	/*
	 * If this message has no O (room) field, generate one.
	 */
	if (msg->cm_fields['O'] == NULL) {
		msg->cm_fields['O'] = strdup(CCC->room.QRname);
	}

	/* Perform "before save" hooks (aborting if any return nonzero) */
	CtdlLogPrintf(CTDL_DEBUG, "Performing before-save hooks\n");
	if (PerformMessageHooks(msg, EVT_BEFORESAVE) > 0) return(-3);

	/*
	 * If this message has an Exclusive ID, and the room is replication
	 * checking enabled, then do replication checks.
	 */
	if (DoesThisRoomNeedEuidIndexing(&CCC->room)) {
		ReplicationChecks(msg);
	}

	/* Save it to disk */
	CtdlLogPrintf(CTDL_DEBUG, "Saving to disk\n");
	newmsgid = send_message(msg);
	if (newmsgid <= 0L) return(-5);

	/* Write a supplemental message info record.  This doesn't have to
	 * be a critical section because nobody else knows about this message
	 * yet.
	 */
	CtdlLogPrintf(CTDL_DEBUG, "Creating MetaData record\n");
	memset(&smi, 0, sizeof(struct MetaData));
	smi.meta_msgnum = newmsgid;
	smi.meta_refcount = 0;
	safestrncpy(smi.meta_content_type, content_type,
			sizeof smi.meta_content_type);

	/*
	 * Measure how big this message will be when rendered as RFC822.
	 * We do this for two reasons:
	 * 1. We need the RFC822 length for the new metadata record, so the
	 *    POP and IMAP services don't have to calculate message lengths
	 *    while the user is waiting (multiplied by potentially hundreds
	 *    or thousands of messages).
	 * 2. If journaling is enabled, we will need an RFC822 version of the
	 *    message to attach to the journalized copy.
	 */
	if (CCC->redirect_buffer != NULL) {
		CtdlLogPrintf(CTDL_ALERT, "CCC->redirect_buffer is not NULL during message submission!\n");
		abort();
	}
	CCC->redirect_buffer = malloc(SIZ);
	CCC->redirect_len = 0;
	CCC->redirect_alloc = SIZ;
	CtdlOutputPreLoadedMsg(msg, MT_RFC822, HEADERS_ALL, 0, 1, QP_EADDR);
	smi.meta_rfc822_length = CCC->redirect_len;
	saved_rfc822_version = CCC->redirect_buffer;
	CCC->redirect_buffer = NULL;
	CCC->redirect_len = 0;
	CCC->redirect_alloc = 0;

	PutMetaData(&smi);

	/* Now figure out where to store the pointers */
	CtdlLogPrintf(CTDL_DEBUG, "Storing pointers\n");

	/* If this is being done by the networker delivering a private
	 * message, we want to BYPASS saving the sender's copy (because there
	 * is no local sender; it would otherwise go to the Trashcan).
	 */
	if ((!CCC->internal_pgm) || (recps == NULL)) {
		if (CtdlSaveMsgPointerInRoom(actual_rm, newmsgid, 1, msg) != 0) {
			CtdlLogPrintf(CTDL_ERR, "ERROR saving message pointer!\n");
			CtdlSaveMsgPointerInRoom(config.c_aideroom, newmsgid, 0, msg);
		}
	}

	/* For internet mail, drop a copy in the outbound queue room */
	if ((recps != NULL) && (recps->num_internet > 0)) {
		CtdlSaveMsgPointerInRoom(SMTP_SPOOLOUT_ROOM, newmsgid, 0, msg);
	}

	/* If other rooms are specified, drop them there too. */
	if ((recps != NULL) && (recps->num_room > 0))
	  for (i=0; i<num_tokens(recps->recp_room, '|'); ++i) {
		extract_token(recipient, recps->recp_room, i,
					'|', sizeof recipient);
		CtdlLogPrintf(CTDL_DEBUG, "Delivering to room <%s>\n", recipient);
		CtdlSaveMsgPointerInRoom(recipient, newmsgid, 0, msg);
	}

	/* Bump this user's messages posted counter. */
	CtdlLogPrintf(CTDL_DEBUG, "Updating user\n");
	CtdlGetUserLock(&CCC->user, CCC->curr_user);
	CCC->user.posted = CCC->user.posted + 1;
	CtdlPutUserLock(&CCC->user);

	/* Decide where bounces need to be delivered */
	if ((recps != NULL) && (recps->bounce_to != NULL)) {
		safestrncpy(bounce_to, recps->bounce_to, sizeof bounce_to);
	}
	else if (CCC->logged_in) {
		snprintf(bounce_to, sizeof bounce_to, "%s@%s", CCC->user.fullname, config.c_nodename);
	}
	else {
		snprintf(bounce_to, sizeof bounce_to, "%s@%s", msg->cm_fields['A'], msg->cm_fields['N']);
	}

	/* If this is private, local mail, make a copy in the
	 * recipient's mailbox and bump the reference count.
	 */
	if ((recps != NULL) && (recps->num_local > 0))
	  for (i=0; i<num_tokens(recps->recp_local, '|'); ++i) {
		extract_token(recipient, recps->recp_local, i,
					'|', sizeof recipient);
		CtdlLogPrintf(CTDL_DEBUG, "Delivering private local mail to <%s>\n",
			recipient);
		if (CtdlGetUser(&userbuf, recipient) == 0) {
			// Add a flag so the Funambol module knows its mail
			msg->cm_fields['W'] = strdup(recipient);
			CtdlMailboxName(actual_rm, sizeof actual_rm, &userbuf, MAILROOM);
			CtdlSaveMsgPointerInRoom(actual_rm, newmsgid, 0, msg);
			CtdlBumpNewMailCounter(userbuf.usernum);
			if (!IsEmptyStr(config.c_funambol_host) || !IsEmptyStr(config.c_pager_program)) {
			/* Generate a instruction message for the Funambol notification
			 * server, in the same style as the SMTP queue
			 */
			   instr_alloc = 1024;
			   instr = malloc(instr_alloc);
			   snprintf(instr, instr_alloc,
			"Content-type: %s\n\nmsgid|%ld\nsubmitted|%ld\n"
			"bounceto|%s\n",
			SPOOLMIME, newmsgid, (long)time(NULL),
			bounce_to
			);

			   imsg = malloc(sizeof(struct CtdlMessage));
			   memset(imsg, 0, sizeof(struct CtdlMessage));
			   imsg->cm_magic = CTDLMESSAGE_MAGIC;
			   imsg->cm_anon_type = MES_NORMAL;
			   imsg->cm_format_type = FMT_RFC822;
			   imsg->cm_fields['A'] = strdup("Citadel");
			   imsg->cm_fields['J'] = strdup("do not journal");
			   imsg->cm_fields['M'] = instr;	/* imsg owns this memory now */
			   imsg->cm_fields['W'] = strdup(recipient);
			   CtdlSubmitMsg(imsg, NULL, FNBL_QUEUE_ROOM, 0);
			   CtdlFreeMessage(imsg);
			}
		}
		else {
			CtdlLogPrintf(CTDL_DEBUG, "No user <%s>\n", recipient);
			CtdlSaveMsgPointerInRoom(config.c_aideroom,
				newmsgid, 0, msg);
		}
	}

	/* Perform "after save" hooks */
	CtdlLogPrintf(CTDL_DEBUG, "Performing after-save hooks\n");
	PerformMessageHooks(msg, EVT_AFTERSAVE);

	/* For IGnet mail, we have to save a new copy into the spooler for
	 * each recipient, with the R and D fields set to the recipient and
	 * destination-node.  This has two ugly side effects: all other
	 * recipients end up being unlisted in this recipient's copy of the
	 * message, and it has to deliver multiple messages to the same
	 * node.  We'll revisit this again in a year or so when everyone has
	 * a network spool receiver that can handle the new style messages.
	 */
	if ((recps != NULL) && (recps->num_ignet > 0))
	  for (i=0; i<num_tokens(recps->recp_ignet, '|'); ++i) {
		extract_token(recipient, recps->recp_ignet, i,
				'|', sizeof recipient);

		hold_R = msg->cm_fields['R'];
		hold_D = msg->cm_fields['D'];
		msg->cm_fields['R'] = malloc(SIZ);
		msg->cm_fields['D'] = malloc(128);
		extract_token(msg->cm_fields['R'], recipient, 0, '@', SIZ);
		extract_token(msg->cm_fields['D'], recipient, 1, '@', 128);
		
		serialize_message(&smr, msg);
		if (smr.len > 0) {
			snprintf(submit_filename, sizeof submit_filename,
					 "%s/netmail.%04lx.%04x.%04x",
					 ctdl_netin_dir,
					 (long) getpid(), CCC->cs_pid, ++seqnum);
			network_fp = fopen(submit_filename, "wb+");
			if (network_fp != NULL) {
				rv = fwrite(smr.ser, smr.len, 1, network_fp);
				fclose(network_fp);
			}
			free(smr.ser);
		}

		free(msg->cm_fields['R']);
		free(msg->cm_fields['D']);
		msg->cm_fields['R'] = hold_R;
		msg->cm_fields['D'] = hold_D;
	}

	/* Go back to the room we started from */
	CtdlLogPrintf(CTDL_DEBUG, "Returning to original room %s\n", hold_rm);
	if (strcasecmp(hold_rm, CCC->room.QRname))
		CtdlUserGoto(hold_rm, 0, 1, NULL, NULL);

	/* For internet mail, generate delivery instructions.
	 * Yes, this is recursive.  Deal with it.  Infinite recursion does
	 * not happen because the delivery instructions message does not
	 * contain a recipient.
	 */
	if ((recps != NULL) && (recps->num_internet > 0)) {
		CtdlLogPrintf(CTDL_DEBUG, "Generating delivery instructions\n");
		instr_alloc = 1024;
		instr = malloc(instr_alloc);
		snprintf(instr, instr_alloc,
			"Content-type: %s\n\nmsgid|%ld\nsubmitted|%ld\n"
			"bounceto|%s\n",
			SPOOLMIME, newmsgid, (long)time(NULL),
			bounce_to
		);

		if (recps->envelope_from != NULL) {
			tmp = strlen(instr);
			snprintf(&instr[tmp], instr_alloc-tmp, "envelope_from|%s\n", recps->envelope_from);
		}

	  	for (i=0; i<num_tokens(recps->recp_internet, '|'); ++i) {
			tmp = strlen(instr);
			extract_token(recipient, recps->recp_internet, i, '|', sizeof recipient);
			if ((tmp + strlen(recipient) + 32) > instr_alloc) {
				instr_alloc = instr_alloc * 2;
				instr = realloc(instr, instr_alloc);
			}
			snprintf(&instr[tmp], instr_alloc - tmp, "remote|%s|0||\n", recipient);
		}

		imsg = malloc(sizeof(struct CtdlMessage));
		memset(imsg, 0, sizeof(struct CtdlMessage));
		imsg->cm_magic = CTDLMESSAGE_MAGIC;
		imsg->cm_anon_type = MES_NORMAL;
		imsg->cm_format_type = FMT_RFC822;
		imsg->cm_fields['A'] = strdup("Citadel");
		imsg->cm_fields['J'] = strdup("do not journal");
		imsg->cm_fields['M'] = instr;	/* imsg owns this memory now */
		CtdlSubmitMsg(imsg, NULL, SMTP_SPOOLOUT_ROOM, QP_EADDR);
		CtdlFreeMessage(imsg);
	}

	/*
	 * Any addresses to harvest for someone's address book?
	 */
	if ( (CCC->logged_in) && (recps != NULL) ) {
		collected_addresses = harvest_collected_addresses(msg);
	}

	if (collected_addresses != NULL) {
		aptr = (struct addresses_to_be_filed *)
			malloc(sizeof(struct addresses_to_be_filed));
		CtdlMailboxName(actual_rm, sizeof actual_rm,
			&CCC->user, USERCONTACTSROOM);
		aptr->roomname = strdup(actual_rm);
		aptr->collected_addresses = collected_addresses;
		begin_critical_section(S_ATBF);
		aptr->next = atbf;
		atbf = aptr;
		end_critical_section(S_ATBF);
	}

	/*
	 * Determine whether this message qualifies for journaling.
	 */
	if (msg->cm_fields['J'] != NULL) {
		qualified_for_journaling = 0;
	}
	else {
		if (recps == NULL) {
			qualified_for_journaling = config.c_journal_pubmsgs;
		}
		else if (recps->num_local + recps->num_ignet + recps->num_internet > 0) {
			qualified_for_journaling = config.c_journal_email;
		}
		else {
			qualified_for_journaling = config.c_journal_pubmsgs;
		}
	}

	/*
	 * Do we have to perform journaling?  If so, hand off the saved
	 * RFC822 version will be handed off to the journaler for background
	 * submit.  Otherwise, we have to free the memory ourselves.
	 */
	if (saved_rfc822_version != NULL) {
		if (qualified_for_journaling) {
			JournalBackgroundSubmit(msg, saved_rfc822_version, recps);
		}
		else {
			free(saved_rfc822_version);
		}
	}

	/* Done. */
	return(newmsgid);
}



void aide_message (char *text, char *subject)
{
	quickie_message("Citadel",NULL,NULL,AIDEROOM,text,FMT_CITADEL,subject);
}


/*
 * Convenience function for generating small administrative messages.
 */
void quickie_message(const char *from, const char *fromaddr, char *to, char *room, const char *text, 
			int format_type, const char *subject)
{
	struct CtdlMessage *msg;
	struct recptypes *recp = NULL;

	msg = malloc(sizeof(struct CtdlMessage));
	memset(msg, 0, sizeof(struct CtdlMessage));
	msg->cm_magic = CTDLMESSAGE_MAGIC;
	msg->cm_anon_type = MES_NORMAL;
	msg->cm_format_type = format_type;

	if (from != NULL) {
		msg->cm_fields['A'] = strdup(from);
	}
	else if (fromaddr != NULL) {
		msg->cm_fields['A'] = strdup(fromaddr);
		if (strchr(msg->cm_fields['A'], '@')) {
			*strchr(msg->cm_fields['A'], '@') = 0;
		}
	}
	else {
		msg->cm_fields['A'] = strdup("Citadel");
	}

	if (fromaddr != NULL) msg->cm_fields['F'] = strdup(fromaddr);
	if (room != NULL) msg->cm_fields['O'] = strdup(room);
	msg->cm_fields['N'] = strdup(NODENAME);
	if (to != NULL) {
		msg->cm_fields['R'] = strdup(to);
		recp = validate_recipients(to, NULL, 0);
	}
	if (subject != NULL) {
		msg->cm_fields['U'] = strdup(subject);
	}
	msg->cm_fields['M'] = strdup(text);

	CtdlSubmitMsg(msg, recp, room, 0);
	CtdlFreeMessage(msg);
	if (recp != NULL) free_recipients(recp);
}



/*
 * Back end function used by CtdlMakeMessage() and similar functions
 */
char *CtdlReadMessageBody(char *terminator,	/* token signalling EOT */
			size_t maxlen,		/* maximum message length */
			char *exist,		/* if non-null, append to it;
						   exist is ALWAYS freed  */
			int crlf,		/* CRLF newlines instead of LF */
			int sock		/* socket handle or 0 for this session's client socket */
			) {
	char buf[1024];
	int linelen;
	size_t message_len = 0;
	size_t buffer_len = 0;
	char *ptr;
	char *m;
	int flushing = 0;
	int finished = 0;
	int dotdot = 0;

	if (exist == NULL) {
		m = malloc(4096);
		m[0] = 0;
		buffer_len = 4096;
		message_len = 0;
	}
	else {
		message_len = strlen(exist);
		buffer_len = message_len + 4096;
		m = realloc(exist, buffer_len);
		if (m == NULL) {
			free(exist);
			return m;
		}
	}

	/* Do we need to change leading ".." to "." for SMTP escaping? */
	if (!strcmp(terminator, ".")) {
		dotdot = 1;
	}

	/* flush the input if we have nowhere to store it */
	if (m == NULL) {
		flushing = 1;
	}

	/* read in the lines of message text one by one */
	do {
		if (sock > 0) {
			if (sock_getln(sock, buf, (sizeof buf - 3)) < 0) finished = 1;
		}
		else {
			if (client_getln(buf, (sizeof buf - 3)) < 1) finished = 1;
		}
		if (!strcmp(buf, terminator)) finished = 1;
		if (crlf) {
			strcat(buf, "\r\n");
		}
		else {
			strcat(buf, "\n");
		}

		/* Unescape SMTP-style input of two dots at the beginning of the line */
		if (dotdot) {
			if (!strncmp(buf, "..", 2)) {
				strcpy(buf, &buf[1]);
			}
		}

		if ( (!flushing) && (!finished) ) {
			/* Measure the line */
			linelen = strlen(buf);
	
			/* augment the buffer if we have to */
			if ((message_len + linelen) >= buffer_len) {
				ptr = realloc(m, (buffer_len * 2) );
				if (ptr == NULL) {	/* flush if can't allocate */
					flushing = 1;
				} else {
					buffer_len = (buffer_len * 2);
					m = ptr;
					CtdlLogPrintf(CTDL_DEBUG, "buffer_len is now %ld\n", (long)buffer_len);
				}
			}
	
			/* Add the new line to the buffer.  NOTE: this loop must avoid
		 	* using functions like strcat() and strlen() because they
		 	* traverse the entire buffer upon every call, and doing that
		 	* for a multi-megabyte message slows it down beyond usability.
		 	*/
			strcpy(&m[message_len], buf);
			message_len += linelen;
		}

		/* if we've hit the max msg length, flush the rest */
		if (message_len >= maxlen) flushing = 1;

	} while (!finished);
	return(m);
}




/*
 * Build a binary message to be saved on disk.
 * (NOTE: if you supply 'preformatted_text', the buffer you give it
 * will become part of the message.  This means you are no longer
 * responsible for managing that memory -- it will be freed along with
 * the rest of the fields when CtdlFreeMessage() is called.)
 */

struct CtdlMessage *CtdlMakeMessage(
	struct ctdluser *author,	/* author's user structure */
	char *recipient,		/* NULL if it's not mail */
	char *recp_cc,			/* NULL if it's not mail */
	char *room,			/* room where it's going */
	int type,			/* see MES_ types in header file */
	int format_type,		/* variformat, plain text, MIME... */
	char *fake_name,		/* who we're masquerading as */
	char *my_email,			/* which of my email addresses to use (empty is ok) */
	char *subject,			/* Subject (optional) */
	char *supplied_euid,		/* ...or NULL if this is irrelevant */
	char *preformatted_text,	/* ...or NULL to read text from client */
	char *references		/* Thread references */
) {
	char dest_node[256];
	char buf[1024];
	struct CtdlMessage *msg;

	msg = malloc(sizeof(struct CtdlMessage));
	memset(msg, 0, sizeof(struct CtdlMessage));
	msg->cm_magic = CTDLMESSAGE_MAGIC;
	msg->cm_anon_type = type;
	msg->cm_format_type = format_type;

	/* Don't confuse the poor folks if it's not routed mail. */
	strcpy(dest_node, "");

	if (recipient != NULL) striplt(recipient);
	if (recp_cc != NULL) striplt(recp_cc);

	/* Path or Return-Path */
	if (my_email == NULL) my_email = "";

	if (!IsEmptyStr(my_email)) {
		msg->cm_fields['P'] = strdup(my_email);
	}
	else {
		snprintf(buf, sizeof buf, "%s", author->fullname);
		msg->cm_fields['P'] = strdup(buf);
	}
	convert_spaces_to_underscores(msg->cm_fields['P']);

	snprintf(buf, sizeof buf, "%ld", (long)time(NULL));	/* timestamp */
	msg->cm_fields['T'] = strdup(buf);

	if ((fake_name != NULL) && (fake_name[0])) {		/* author */
		msg->cm_fields['A'] = strdup(fake_name);
	}
	else {
		msg->cm_fields['A'] = strdup(author->fullname);
	}

	if (CC->room.QRflags & QR_MAILBOX) {		/* room */
		msg->cm_fields['O'] = strdup(&CC->room.QRname[11]);
	}
	else {
		msg->cm_fields['O'] = strdup(CC->room.QRname);
	}

	msg->cm_fields['N'] = strdup(NODENAME);		/* nodename */
	msg->cm_fields['H'] = strdup(HUMANNODE);		/* hnodename */

	if ((recipient != NULL) && (recipient[0] != 0)) {
		msg->cm_fields['R'] = strdup(recipient);
	}
	if ((recp_cc != NULL) && (recp_cc[0] != 0)) {
		msg->cm_fields['Y'] = strdup(recp_cc);
	}
	if (dest_node[0] != 0) {
		msg->cm_fields['D'] = strdup(dest_node);
	}

	if (!IsEmptyStr(my_email)) {
		msg->cm_fields['F'] = strdup(my_email);
	}
	else if ( (author == &CC->user) && (!IsEmptyStr(CC->cs_inet_email)) ) {
		msg->cm_fields['F'] = strdup(CC->cs_inet_email);
	}

	if (subject != NULL) {
		long length;
		striplt(subject);
		length = strlen(subject);
		if (length > 0) {
			long i;
			long IsAscii;
			IsAscii = -1;
			i = 0;
			while ((subject[i] != '\0') &&
			       (IsAscii = isascii(subject[i]) != 0 ))
				i++;
			if (IsAscii != 0)
				msg->cm_fields['U'] = strdup(subject);
			else /* ok, we've got utf8 in the string. */
			{
				msg->cm_fields['U'] = rfc2047encode(subject, length);
			}

		}
	}

	if (supplied_euid != NULL) {
		msg->cm_fields['E'] = strdup(supplied_euid);
	}

	if (references != NULL) {
		if (!IsEmptyStr(references)) {
			msg->cm_fields['W'] = strdup(references);
		}
	}

	if (preformatted_text != NULL) {
		msg->cm_fields['M'] = preformatted_text;
	}
	else {
		msg->cm_fields['M'] = CtdlReadMessageBody("000", config.c_maxmsglen, NULL, 0, 0);
	}

	return(msg);
}


/*
 * Check to see whether we have permission to post a message in the current
 * room.  Returns a *CITADEL ERROR CODE* and puts a message in errmsgbuf, or
 * returns 0 on success.
 */
int CtdlDoIHavePermissionToPostInThisRoom(char *errmsgbuf, 
					  size_t n, 
					  const char* RemoteIdentifier,
					  int PostPublic) {
	int ra;

	if (!(CC->logged_in) && 
	    (PostPublic == POST_LOGGED_IN)) {
		snprintf(errmsgbuf, n, "Not logged in.");
		return (ERROR + NOT_LOGGED_IN);
	}
	else if (PostPublic == CHECK_EXISTANCE) {
		return (0); // We're Evaling whether a recipient exists
	}
	else if (!(CC->logged_in)) {
		
		if ((CC->room.QRflags & QR_READONLY)) {
			snprintf(errmsgbuf, n, "Not logged in.");
			return (ERROR + NOT_LOGGED_IN);
		}
		if (CC->room.QRflags2 & QR2_MODERATED) {
			snprintf(errmsgbuf, n, "Not logged in Moderation feature not yet implemented!");
			return (ERROR + NOT_LOGGED_IN);
		}
		if ((PostPublic!=POST_LMTP) &&(CC->room.QRflags2 & QR2_SMTP_PUBLIC) == 0) {
			SpoolControl *sc;
			char filename[SIZ];
			int found;

			if (RemoteIdentifier == NULL)
			{
				snprintf(errmsgbuf, n, "Need sender to permit access.");
				return (ERROR + USERNAME_REQUIRED);
			}

			assoc_file_name(filename, sizeof filename, &CC->room, ctdl_netcfg_dir);
			begin_critical_section(S_NETCONFIGS);
			if (!read_spoolcontrol_file(&sc, filename))
			{
				end_critical_section(S_NETCONFIGS);
				snprintf(errmsgbuf, n,
					"This mailing list only accepts posts from subscribers.");
				return (ERROR + NO_SUCH_USER);
			}
			end_critical_section(S_NETCONFIGS);
			found = is_recipient (sc, RemoteIdentifier);
			free_spoolcontrol_struct(&sc);
			if (found) {
				return (0);
			}
			else {
				snprintf(errmsgbuf, n,
					"This mailing list only accepts posts from subscribers.");
				return (ERROR + NO_SUCH_USER);
			}
		}
		return (0);

	}

	if ((CC->user.axlevel < 2)
	    && ((CC->room.QRflags & QR_MAILBOX) == 0)) {
		snprintf(errmsgbuf, n, "Need to be validated to enter "
				"(except in %s> to sysop)", MAILROOM);
		return (ERROR + HIGHER_ACCESS_REQUIRED);
	}

	CtdlRoomAccess(&CC->room, &CC->user, &ra, NULL);
	if (!(ra & UA_POSTALLOWED)) {
		snprintf(errmsgbuf, n, "Higher access is required to post in this room.");
		return (ERROR + HIGHER_ACCESS_REQUIRED);
	}

	strcpy(errmsgbuf, "Ok");
	return(0);
}


/*
 * Check to see if the specified user has Internet mail permission
 * (returns nonzero if permission is granted)
 */
int CtdlCheckInternetMailPermission(struct ctdluser *who) {

	/* Do not allow twits to send Internet mail */
	if (who->axlevel <= 2) return(0);

	/* Globally enabled? */
	if (config.c_restrict == 0) return(1);

	/* User flagged ok? */
	if (who->flags & US_INTERNET) return(2);

	/* Aide level access? */
	if (who->axlevel >= 6) return(3);

	/* No mail for you! */
	return(0);
}


/*
 * Validate recipients, count delivery types and errors, and handle aliasing
 * FIXME check for dupes!!!!!
 *
 * Returns 0 if all addresses are ok, ret->num_error = -1 if no addresses 
 * were specified, or the number of addresses found invalid.
 *
 * Caller needs to free the result using free_recipients()
 */
struct recptypes *validate_recipients(char *supplied_recipients, 
				      const char *RemoteIdentifier, 
				      int Flags) {
	struct recptypes *ret;
	char *recipients = NULL;
	char this_recp[256];
	char this_recp_cooked[256];
	char append[SIZ];
	int num_recps = 0;
	int i, j;
	int mailtype;
	int invalid;
	struct ctdluser tempUS;
	struct ctdlroom tempQR;
	struct ctdlroom tempQR2;
	int err = 0;
	char errmsg[SIZ];
	int in_quotes = 0;

	/* Initialize */
	ret = (struct recptypes *) malloc(sizeof(struct recptypes));
	if (ret == NULL) return(NULL);

	/* Set all strings to null and numeric values to zero */
	memset(ret, 0, sizeof(struct recptypes));

	if (supplied_recipients == NULL) {
		recipients = strdup("");
	}
	else {
		recipients = strdup(supplied_recipients);
	}

	/* Allocate some memory.  Yes, this allocates 500% more memory than we will
	 * actually need, but it's healthier for the heap than doing lots of tiny
	 * realloc() calls instead.
	 */

	ret->errormsg = malloc(strlen(recipients) + 1024);
	ret->recp_local = malloc(strlen(recipients) + 1024);
	ret->recp_internet = malloc(strlen(recipients) + 1024);
	ret->recp_ignet = malloc(strlen(recipients) + 1024);
	ret->recp_room = malloc(strlen(recipients) + 1024);
	ret->display_recp = malloc(strlen(recipients) + 1024);

	ret->errormsg[0] = 0;
	ret->recp_local[0] = 0;
	ret->recp_internet[0] = 0;
	ret->recp_ignet[0] = 0;
	ret->recp_room[0] = 0;
	ret->display_recp[0] = 0;

	ret->recptypes_magic = RECPTYPES_MAGIC;

	/* Change all valid separator characters to commas */
	for (i=0; !IsEmptyStr(&recipients[i]); ++i) {
		if ((recipients[i] == ';') || (recipients[i] == '|')) {
			recipients[i] = ',';
		}
	}

	/* Now start extracting recipients... */

	while (!IsEmptyStr(recipients)) {

		for (i=0; i<=strlen(recipients); ++i) {
			if (recipients[i] == '\"') in_quotes = 1 - in_quotes;
			if ( ( (recipients[i] == ',') && (!in_quotes) ) || (recipients[i] == 0) ) {
				safestrncpy(this_recp, recipients, i+1);
				this_recp[i] = 0;
				if (recipients[i] == ',') {
					strcpy(recipients, &recipients[i+1]);
				}
				else {
					strcpy(recipients, "");
				}
				break;
			}
		}

		striplt(this_recp);
		if (IsEmptyStr(this_recp))
			break;
		CtdlLogPrintf(CTDL_DEBUG, "Evaluating recipient #%d: %s\n", num_recps, this_recp);
		++num_recps;
		mailtype = alias(this_recp);
		mailtype = alias(this_recp);
		mailtype = alias(this_recp);
		j = 0;
		for (j=0; !IsEmptyStr(&this_recp[j]); ++j) {
			if (this_recp[j]=='_') {
				this_recp_cooked[j] = ' ';
			}
			else {
				this_recp_cooked[j] = this_recp[j];
			}
		}
		this_recp_cooked[j] = '\0';
		invalid = 0;
		errmsg[0] = 0;
		switch(mailtype) {
			case MES_LOCAL:
				if (!strcasecmp(this_recp, "sysop")) {
					++ret->num_room;
					strcpy(this_recp, config.c_aideroom);
					if (!IsEmptyStr(ret->recp_room)) {
						strcat(ret->recp_room, "|");
					}
					strcat(ret->recp_room, this_recp);
				}
				else if ( (!strncasecmp(this_recp, "room_", 5))
				      && (!CtdlGetRoom(&tempQR, &this_recp_cooked[5])) ) {

				      	/* Save room so we can restore it later */
					tempQR2 = CC->room;
					CC->room = tempQR;
					
					/* Check permissions to send mail to this room */
					err = CtdlDoIHavePermissionToPostInThisRoom(errmsg, 
										    sizeof errmsg, 
										    RemoteIdentifier,
										    Flags
					);
					if (err)
					{
						++ret->num_error;
						invalid = 1;
					} 
					else {
						++ret->num_room;
						if (!IsEmptyStr(ret->recp_room)) {
							strcat(ret->recp_room, "|");
						}
						strcat(ret->recp_room, &this_recp_cooked[5]);
					}
					
					/* Restore room in case something needs it */
					CC->room = tempQR2;

				}
				else if (CtdlGetUser(&tempUS, this_recp) == 0) {
					++ret->num_local;
					strcpy(this_recp, tempUS.fullname);
					if (!IsEmptyStr(ret->recp_local)) {
						strcat(ret->recp_local, "|");
					}
					strcat(ret->recp_local, this_recp);
				}
				else if (CtdlGetUser(&tempUS, this_recp_cooked) == 0) {
					++ret->num_local;
					strcpy(this_recp, tempUS.fullname);
					if (!IsEmptyStr(ret->recp_local)) {
						strcat(ret->recp_local, "|");
					}
					strcat(ret->recp_local, this_recp);
				}
				else {
					++ret->num_error;
					invalid = 1;
				}
				break;
			case MES_INTERNET:
				/* Yes, you're reading this correctly: if the target
				 * domain points back to the local system or an attached
				 * Citadel directory, the address is invalid.  That's
				 * because if the address were valid, we would have
				 * already translated it to a local address by now.
				 */
				if (IsDirectory(this_recp, 0)) {
					++ret->num_error;
					invalid = 1;
				}
				else {
					++ret->num_internet;
					if (!IsEmptyStr(ret->recp_internet)) {
						strcat(ret->recp_internet, "|");
					}
					strcat(ret->recp_internet, this_recp);
				}
				break;
			case MES_IGNET:
				++ret->num_ignet;
				if (!IsEmptyStr(ret->recp_ignet)) {
					strcat(ret->recp_ignet, "|");
				}
				strcat(ret->recp_ignet, this_recp);
				break;
			case MES_ERROR:
				++ret->num_error;
				invalid = 1;
				break;
		}
		if (invalid) {
			if (IsEmptyStr(errmsg)) {
				snprintf(append, sizeof append, "Invalid recipient: %s", this_recp);
			}
			else {
				snprintf(append, sizeof append, "%s", errmsg);
			}
			if ( (strlen(ret->errormsg) + strlen(append) + 3) < SIZ) {
				if (!IsEmptyStr(ret->errormsg)) {
					strcat(ret->errormsg, "; ");
				}
				strcat(ret->errormsg, append);
			}
		}
		else {
			if (IsEmptyStr(ret->display_recp)) {
				strcpy(append, this_recp);
			}
			else {
				snprintf(append, sizeof append, ", %s", this_recp);
			}
			if ( (strlen(ret->display_recp)+strlen(append)) < SIZ) {
				strcat(ret->display_recp, append);
			}
		}
	}

	if ((ret->num_local + ret->num_internet + ret->num_ignet +
	   ret->num_room + ret->num_error) == 0) {
		ret->num_error = (-1);
		strcpy(ret->errormsg, "No recipients specified.");
	}

	CtdlLogPrintf(CTDL_DEBUG, "validate_recipients()\n");
	CtdlLogPrintf(CTDL_DEBUG, " local: %d <%s>\n", ret->num_local, ret->recp_local);
	CtdlLogPrintf(CTDL_DEBUG, "  room: %d <%s>\n", ret->num_room, ret->recp_room);
	CtdlLogPrintf(CTDL_DEBUG, "  inet: %d <%s>\n", ret->num_internet, ret->recp_internet);
	CtdlLogPrintf(CTDL_DEBUG, " ignet: %d <%s>\n", ret->num_ignet, ret->recp_ignet);
	CtdlLogPrintf(CTDL_DEBUG, " error: %d <%s>\n", ret->num_error, ret->errormsg);

	free(recipients);
	return(ret);
}


/*
 * Destructor for struct recptypes
 */
void free_recipients(struct recptypes *valid) {

	if (valid == NULL) {
		return;
	}

	if (valid->recptypes_magic != RECPTYPES_MAGIC) {
		CtdlLogPrintf(CTDL_EMERG, "Attempt to call free_recipients() on some other data type!\n");
		abort();
	}

	if (valid->errormsg != NULL)		free(valid->errormsg);
	if (valid->recp_local != NULL)		free(valid->recp_local);
	if (valid->recp_internet != NULL)	free(valid->recp_internet);
	if (valid->recp_ignet != NULL)		free(valid->recp_ignet);
	if (valid->recp_room != NULL)		free(valid->recp_room);
	if (valid->display_recp != NULL)	free(valid->display_recp);
	if (valid->bounce_to != NULL)		free(valid->bounce_to);
	if (valid->envelope_from != NULL)	free(valid->envelope_from);
	free(valid);
}



/*
 * message entry  -  mode 0 (normal)
 */
void cmd_ent0(char *entargs)
{
	int post = 0;
	char recp[SIZ];
	char cc[SIZ];
	char bcc[SIZ];
	char supplied_euid[128];
	int anon_flag = 0;
	int format_type = 0;
	char newusername[256];
	char newuseremail[256];
	struct CtdlMessage *msg;
	int anonymous = 0;
	char errmsg[SIZ];
	int err = 0;
	struct recptypes *valid = NULL;
	struct recptypes *valid_to = NULL;
	struct recptypes *valid_cc = NULL;
	struct recptypes *valid_bcc = NULL;
	char subject[SIZ];
	int subject_required = 0;
	int do_confirm = 0;
	long msgnum;
	int i, j;
	char buf[256];
	int newuseremail_ok = 0;
	char references[SIZ];
	char *ptr;

	unbuffer_output();

	post = extract_int(entargs, 0);
	extract_token(recp, entargs, 1, '|', sizeof recp);
	anon_flag = extract_int(entargs, 2);
	format_type = extract_int(entargs, 3);
	extract_token(subject, entargs, 4, '|', sizeof subject);
	extract_token(newusername, entargs, 5, '|', sizeof newusername);
	do_confirm = extract_int(entargs, 6);
	extract_token(cc, entargs, 7, '|', sizeof cc);
	extract_token(bcc, entargs, 8, '|', sizeof bcc);
	switch(CC->room.QRdefaultview) {
		case VIEW_NOTES:
		case VIEW_WIKI:
			extract_token(supplied_euid, entargs, 9, '|', sizeof supplied_euid);
			break;
		default:
			supplied_euid[0] = 0;
			break;
	}
	extract_token(newuseremail, entargs, 10, '|', sizeof newuseremail);
	extract_token(references, entargs, 11, '|', sizeof references);
	for (ptr=references; *ptr != 0; ++ptr) {
		if (*ptr == '!') *ptr = '|';
	}

	/* first check to make sure the request is valid. */

	err = CtdlDoIHavePermissionToPostInThisRoom(errmsg, sizeof errmsg, NULL, POST_LOGGED_IN);
	if (err)
	{
		cprintf("%d %s\n", err, errmsg);
		return;
	}

	/* Check some other permission type things. */

	if (IsEmptyStr(newusername)) {
		strcpy(newusername, CC->user.fullname);
	}
	if (  (CC->user.axlevel < 6)
	   && (strcasecmp(newusername, CC->user.fullname))
	   && (strcasecmp(newusername, CC->cs_inet_fn))
	) {	
		cprintf("%d You don't have permission to author messages as '%s'.\n",
			ERROR + HIGHER_ACCESS_REQUIRED,
			newusername
		);
		return;
	}


	if (IsEmptyStr(newuseremail)) {
		newuseremail_ok = 1;
	}

	if (!IsEmptyStr(newuseremail)) {
		if (!strcasecmp(newuseremail, CC->cs_inet_email)) {
			newuseremail_ok = 1;
		}
		else if (!IsEmptyStr(CC->cs_inet_other_emails)) {
			j = num_tokens(CC->cs_inet_other_emails, '|');
			for (i=0; i<j; ++i) {
				extract_token(buf, CC->cs_inet_other_emails, i, '|', sizeof buf);
				if (!strcasecmp(newuseremail, buf)) {
					newuseremail_ok = 1;
				}
			}
		}
	}

	if (!newuseremail_ok) {
		cprintf("%d You don't have permission to author messages as '%s'.\n",
			ERROR + HIGHER_ACCESS_REQUIRED,
			newuseremail
		);
		return;
	}

	CC->cs_flags |= CS_POSTING;

	/* In mailbox rooms we have to behave a little differently --
	 * make sure the user has specified at least one recipient.  Then
	 * validate the recipient(s).  We do this for the Mail> room, as
	 * well as any room which has the "Mailbox" view set.
	 */

	if (  ( (CC->room.QRflags & QR_MAILBOX) && (!strcasecmp(&CC->room.QRname[11], MAILROOM)) )
	   || ( (CC->room.QRflags & QR_MAILBOX) && (CC->curr_view == VIEW_MAILBOX) )
	) {
		if (CC->user.axlevel < 2) {
			strcpy(recp, "sysop");
			strcpy(cc, "");
			strcpy(bcc, "");
		}

		valid_to = validate_recipients(recp, NULL, 0);
		if (valid_to->num_error > 0) {
			cprintf("%d %s\n", ERROR + NO_SUCH_USER, valid_to->errormsg);
			free_recipients(valid_to);
			return;
		}

		valid_cc = validate_recipients(cc, NULL, 0);
		if (valid_cc->num_error > 0) {
			cprintf("%d %s\n", ERROR + NO_SUCH_USER, valid_cc->errormsg);
			free_recipients(valid_to);
			free_recipients(valid_cc);
			return;
		}

		valid_bcc = validate_recipients(bcc, NULL, 0);
		if (valid_bcc->num_error > 0) {
			cprintf("%d %s\n", ERROR + NO_SUCH_USER, valid_bcc->errormsg);
			free_recipients(valid_to);
			free_recipients(valid_cc);
			free_recipients(valid_bcc);
			return;
		}

		/* Recipient required, but none were specified */
		if ( (valid_to->num_error < 0) && (valid_cc->num_error < 0) && (valid_bcc->num_error < 0) ) {
			free_recipients(valid_to);
			free_recipients(valid_cc);
			free_recipients(valid_bcc);
			cprintf("%d At least one recipient is required.\n", ERROR + NO_SUCH_USER);
			return;
		}

		if (valid_to->num_internet + valid_cc->num_internet + valid_bcc->num_internet > 0) {
			if (CtdlCheckInternetMailPermission(&CC->user)==0) {
				cprintf("%d You do not have permission "
					"to send Internet mail.\n",
					ERROR + HIGHER_ACCESS_REQUIRED);
				free_recipients(valid_to);
				free_recipients(valid_cc);
				free_recipients(valid_bcc);
				return;
			}
		}

		if ( ( (valid_to->num_internet + valid_to->num_ignet + valid_cc->num_internet + valid_cc->num_ignet + valid_bcc->num_internet + valid_bcc->num_ignet) > 0)
		   && (CC->user.axlevel < 4) ) {
			cprintf("%d Higher access required for network mail.\n",
				ERROR + HIGHER_ACCESS_REQUIRED);
			free_recipients(valid_to);
			free_recipients(valid_cc);
			free_recipients(valid_bcc);
			return;
		}
	
		if ((RESTRICT_INTERNET == 1)
		    && (valid_to->num_internet + valid_cc->num_internet + valid_bcc->num_internet > 0)
		    && ((CC->user.flags & US_INTERNET) == 0)
		    && (!CC->internal_pgm)) {
			cprintf("%d You don't have access to Internet mail.\n",
				ERROR + HIGHER_ACCESS_REQUIRED);
			free_recipients(valid_to);
			free_recipients(valid_cc);
			free_recipients(valid_bcc);
			return;
		}

	}

	/* Is this a room which has anonymous-only or anonymous-option? */
	anonymous = MES_NORMAL;
	if (CC->room.QRflags & QR_ANONONLY) {
		anonymous = MES_ANONONLY;
	}
	if (CC->room.QRflags & QR_ANONOPT) {
		if (anon_flag == 1) {	/* only if the user requested it */
			anonymous = MES_ANONOPT;
		}
	}

	if ((CC->room.QRflags & QR_MAILBOX) == 0) {
		recp[0] = 0;
	}

	/* Recommend to the client that the use of a message subject is
	 * strongly recommended in this room, if either the SUBJECTREQ flag
	 * is set, or if there is one or more Internet email recipients.
	 */
	if (CC->room.QRflags2 & QR2_SUBJECTREQ) subject_required = 1;
	if ((valid_to)	&& (valid_to->num_internet > 0))	subject_required = 1;
	if ((valid_cc)	&& (valid_cc->num_internet > 0))	subject_required = 1;
	if ((valid_bcc)	&& (valid_bcc->num_internet > 0))	subject_required = 1;

	/* If we're only checking the validity of the request, return
	 * success without creating the message.
	 */
	if (post == 0) {
		cprintf("%d %s|%d\n", CIT_OK,
			((valid_to != NULL) ? valid_to->display_recp : ""), 
			subject_required);
		free_recipients(valid_to);
		free_recipients(valid_cc);
		free_recipients(valid_bcc);
		return;
	}

	/* We don't need these anymore because we'll do it differently below */
	free_recipients(valid_to);
	free_recipients(valid_cc);
	free_recipients(valid_bcc);

	/* Read in the message from the client. */
	if (do_confirm) {
		cprintf("%d send message\n", START_CHAT_MODE);
	} else {
		cprintf("%d send message\n", SEND_LISTING);
	}

	msg = CtdlMakeMessage(&CC->user, recp, cc,
		CC->room.QRname, anonymous, format_type,
		newusername, newuseremail, subject,
		((!IsEmptyStr(supplied_euid)) ? supplied_euid : NULL),
		NULL, references);

	/* Put together one big recipients struct containing to/cc/bcc all in
	 * one.  This is for the envelope.
	 */
	char *all_recps = malloc(SIZ * 3);
	strcpy(all_recps, recp);
	if (!IsEmptyStr(cc)) {
		if (!IsEmptyStr(all_recps)) {
			strcat(all_recps, ",");
		}
		strcat(all_recps, cc);
	}
	if (!IsEmptyStr(bcc)) {
		if (!IsEmptyStr(all_recps)) {
			strcat(all_recps, ",");
		}
		strcat(all_recps, bcc);
	}
	if (!IsEmptyStr(all_recps)) {
		valid = validate_recipients(all_recps, NULL, 0);
	}
	else {
		valid = NULL;
	}
	free(all_recps);

	if (msg != NULL) {
		msgnum = CtdlSubmitMsg(msg, valid, "", QP_EADDR);

		if (do_confirm) {
			cprintf("%ld\n", msgnum);
			if (msgnum >= 0L) {
				cprintf("Message accepted.\n");
			}
			else {
				cprintf("Internal error.\n");
			}
			if (msg->cm_fields['E'] != NULL) {
				cprintf("%s\n", msg->cm_fields['E']);
			} else {
				cprintf("\n");
			}
			cprintf("000\n");
		}

		CtdlFreeMessage(msg);
	}
	if (valid != NULL) {
		free_recipients(valid);
	}
	return;
}



/*
 * API function to delete messages which match a set of criteria
 * (returns the actual number of messages deleted)
 */
int CtdlDeleteMessages(char *room_name,		/* which room */
			long *dmsgnums,		/* array of msg numbers to be deleted */
			int num_dmsgnums,	/* number of msgs to be deleted, or 0 for "any" */
			char *content_type	/* or "" for any.  regular expressions expected. */
)
{
	struct ctdlroom qrbuf;
	struct cdbdata *cdbfr;
	long *msglist = NULL;
	long *dellist = NULL;
	int num_msgs = 0;
	int i, j;
	int num_deleted = 0;
	int delete_this;
	struct MetaData smi;
	regex_t re;
	regmatch_t pm;
	int need_to_free_re = 0;

	if (content_type) if (!IsEmptyStr(content_type)) {
		regcomp(&re, content_type, 0);
		need_to_free_re = 1;
	}
	CtdlLogPrintf(CTDL_DEBUG, "CtdlDeleteMessages(%s, %d msgs, %s)\n",
		room_name, num_dmsgnums, content_type);

	/* get room record, obtaining a lock... */
	if (CtdlGetRoomLock(&qrbuf, room_name) != 0) {
		CtdlLogPrintf(CTDL_ERR, "CtdlDeleteMessages(): Room <%s> not found\n",
			room_name);
		if (need_to_free_re) regfree(&re);
		return (0);	/* room not found */
	}
	cdbfr = cdb_fetch(CDB_MSGLISTS, &qrbuf.QRnumber, sizeof(long));

	if (cdbfr != NULL) {
		dellist = malloc(cdbfr->len);
		msglist = (long *) cdbfr->ptr;
		cdbfr->ptr = NULL;	/* CtdlDeleteMessages() now owns this memory */
		num_msgs = cdbfr->len / sizeof(long);
		cdb_free(cdbfr);
	}
	if (num_msgs > 0) {
		for (i = 0; i < num_msgs; ++i) {
			delete_this = 0x00;

			/* Set/clear a bit for each criterion */

			/* 0 messages in the list or a null list means that we are
			 * interested in deleting any messages which meet the other criteria.
			 */
			if ((num_dmsgnums == 0) || (dmsgnums == NULL)) {
				delete_this |= 0x01;
			}
			else {
				for (j=0; j<num_dmsgnums; ++j) {
					if (msglist[i] == dmsgnums[j]) {
						delete_this |= 0x01;
					}
				}
			}

			if (IsEmptyStr(content_type)) {
				delete_this |= 0x02;
			} else {
				GetMetaData(&smi, msglist[i]);
				if (regexec(&re, smi.meta_content_type, 1, &pm, 0) == 0) {
					delete_this |= 0x02;
				}
			}

			/* Delete message only if all bits are set */
			if (delete_this == 0x03) {
				dellist[num_deleted++] = msglist[i];
				msglist[i] = 0L;
			}
		}

		num_msgs = sort_msglist(msglist, num_msgs);
		cdb_store(CDB_MSGLISTS, &qrbuf.QRnumber, (int)sizeof(long),
			  msglist, (int)(num_msgs * sizeof(long)));

		qrbuf.QRhighest = msglist[num_msgs - 1];
	}
	CtdlPutRoomLock(&qrbuf);

	/* Go through the messages we pulled out of the index, and decrement
	 * their reference counts by 1.  If this is the only room the message
	 * was in, the reference count will reach zero and the message will
	 * automatically be deleted from the database.  We do this in a
	 * separate pass because there might be plug-in hooks getting called,
	 * and we don't want that happening during an S_ROOMS critical
	 * section.
	 */
	if (num_deleted) for (i=0; i<num_deleted; ++i) {
		PerformDeleteHooks(qrbuf.QRname, dellist[i]);
		AdjRefCount(dellist[i], -1);
	}

	/* Now free the memory we used, and go away. */
	if (msglist != NULL) free(msglist);
	if (dellist != NULL) free(dellist);
	CtdlLogPrintf(CTDL_DEBUG, "%d message(s) deleted.\n", num_deleted);
	if (need_to_free_re) regfree(&re);
	return (num_deleted);
}



/*
 * Check whether the current user has permission to delete messages from
 * the current room (returns 1 for yes, 0 for no)
 */
int CtdlDoIHavePermissionToDeleteMessagesFromThisRoom(void) {
	int ra;
	CtdlRoomAccess(&CC->room, &CC->user, &ra, NULL);
	if (ra & UA_DELETEALLOWED) return(1);
	return(0);
}




/*
 * Delete message from current room
 */
void cmd_dele(char *args)
{
	int num_deleted;
	int i;
	char msgset[SIZ];
	char msgtok[32];
	long *msgs;
	int num_msgs = 0;

	extract_token(msgset, args, 0, '|', sizeof msgset);
	num_msgs = num_tokens(msgset, ',');
	if (num_msgs < 1) {
		cprintf("%d Nothing to do.\n", CIT_OK);
		return;
	}

	if (CtdlDoIHavePermissionToDeleteMessagesFromThisRoom() == 0) {
		cprintf("%d Higher access required.\n",
			ERROR + HIGHER_ACCESS_REQUIRED);
		return;
	}

	/*
	 * Build our message set to be moved/copied
	 */
	msgs = malloc(num_msgs * sizeof(long));
	for (i=0; i<num_msgs; ++i) {
		extract_token(msgtok, msgset, i, ',', sizeof msgtok);
		msgs[i] = atol(msgtok);
	}

	num_deleted = CtdlDeleteMessages(CC->room.QRname, msgs, num_msgs, "");
	free(msgs);

	if (num_deleted) {
		cprintf("%d %d message%s deleted.\n", CIT_OK,
			num_deleted, ((num_deleted != 1) ? "s" : ""));
	} else {
		cprintf("%d Message not found.\n", ERROR + MESSAGE_NOT_FOUND);
	}
}




/*
 * move or copy a message to another room
 */
void cmd_move(char *args)
{
	char msgset[SIZ];
	char msgtok[32];
	long *msgs;
	int num_msgs = 0;

	char targ[ROOMNAMELEN];
	struct ctdlroom qtemp;
	int err;
	int is_copy = 0;
	int ra;
	int permit = 0;
	int i;

	extract_token(msgset, args, 0, '|', sizeof msgset);
	num_msgs = num_tokens(msgset, ',');
	if (num_msgs < 1) {
		cprintf("%d Nothing to do.\n", CIT_OK);
		return;
	}

	extract_token(targ, args, 1, '|', sizeof targ);
	convert_room_name_macros(targ, sizeof targ);
	targ[ROOMNAMELEN - 1] = 0;
	is_copy = extract_int(args, 2);

	if (CtdlGetRoom(&qtemp, targ) != 0) {
		cprintf("%d '%s' does not exist.\n", ERROR + ROOM_NOT_FOUND, targ);
		return;
	}

	if (!strcasecmp(qtemp.QRname, CC->room.QRname)) {
		cprintf("%d Source and target rooms are the same.\n", ERROR + ALREADY_EXISTS);
		return;
	}

	CtdlGetUser(&CC->user, CC->curr_user);
	CtdlRoomAccess(&qtemp, &CC->user, &ra, NULL);

	/* Check for permission to perform this operation.
	 * Remember: "CC->room" is source, "qtemp" is target.
	 */
	permit = 0;

	/* Aides can move/copy */
	if (CC->user.axlevel >= 6) permit = 1;

	/* Room aides can move/copy */
	if (CC->user.usernum == CC->room.QRroomaide) permit = 1;

	/* Permit move/copy from personal rooms */
	if ((CC->room.QRflags & QR_MAILBOX)
	   && (qtemp.QRflags & QR_MAILBOX)) permit = 1;

	/* Permit only copy from public to personal room */
	if ( (is_copy)
	   && (!(CC->room.QRflags & QR_MAILBOX))
	   && (qtemp.QRflags & QR_MAILBOX)) permit = 1;

	/* Permit message removal from collaborative delete rooms */
	if (CC->room.QRflags2 & QR2_COLLABDEL) permit = 1;

	/* Users allowed to post into the target room may move into it too. */
	if ((CC->room.QRflags & QR_MAILBOX) && 
	    (qtemp.QRflags & UA_POSTALLOWED))  permit = 1;

	/* User must have access to target room */
	if (!(ra & UA_KNOWN))  permit = 0;

	if (!permit) {
		cprintf("%d Higher access required.\n",
			ERROR + HIGHER_ACCESS_REQUIRED);
		return;
	}

	/*
	 * Build our message set to be moved/copied
	 */
	msgs = malloc(num_msgs * sizeof(long));
	for (i=0; i<num_msgs; ++i) {
		extract_token(msgtok, msgset, i, ',', sizeof msgtok);
		msgs[i] = atol(msgtok);
	}

	/*
	 * Do the copy
	 */
	err = CtdlSaveMsgPointersInRoom(targ, msgs, num_msgs, 1, NULL);
	if (err != 0) {
		cprintf("%d Cannot store message(s) in %s: error %d\n",
			err, targ, err);
		free(msgs);
		return;
	}

	/* Now delete the message from the source room,
	 * if this is a 'move' rather than a 'copy' operation.
	 */
	if (is_copy == 0) {
		CtdlDeleteMessages(CC->room.QRname, msgs, num_msgs, "");
	}
	free(msgs);

	cprintf("%d Message(s) %s.\n", CIT_OK, (is_copy ? "copied" : "moved") );
}



/*
 * GetMetaData()  -  Get the supplementary record for a message
 */
void GetMetaData(struct MetaData *smibuf, long msgnum)
{

	struct cdbdata *cdbsmi;
	long TheIndex;

	memset(smibuf, 0, sizeof(struct MetaData));
	smibuf->meta_msgnum = msgnum;
	smibuf->meta_refcount = 1;	/* Default reference count is 1 */

	/* Use the negative of the message number for its supp record index */
	TheIndex = (0L - msgnum);

	cdbsmi = cdb_fetch(CDB_MSGMAIN, &TheIndex, sizeof(long));
	if (cdbsmi == NULL) {
		return;		/* record not found; go with defaults */
	}
	memcpy(smibuf, cdbsmi->ptr,
	       ((cdbsmi->len > sizeof(struct MetaData)) ?
		sizeof(struct MetaData) : cdbsmi->len));
	cdb_free(cdbsmi);
	return;
}


/*
 * PutMetaData()  -  (re)write supplementary record for a message
 */
void PutMetaData(struct MetaData *smibuf)
{
	long TheIndex;

	/* Use the negative of the message number for the metadata db index */
	TheIndex = (0L - smibuf->meta_msgnum);

	cdb_store(CDB_MSGMAIN,
		  &TheIndex, (int)sizeof(long),
		  smibuf, (int)sizeof(struct MetaData));

}

/*
 * AdjRefCount  -  submit an adjustment to the reference count for a message.
 *                 (These are just queued -- we actually process them later.)
 */
void AdjRefCount(long msgnum, int incr)
{
	struct arcq new_arcq;
	int rv = 0;

	begin_critical_section(S_SUPPMSGMAIN);
	if (arcfp == NULL) {
		arcfp = fopen(file_arcq, "ab+");
	}
	end_critical_section(S_SUPPMSGMAIN);

	/* msgnum < 0 means that we're trying to close the file */
	if (msgnum < 0) {
		CtdlLogPrintf(CTDL_DEBUG, "Closing the AdjRefCount queue file\n");
		begin_critical_section(S_SUPPMSGMAIN);
		if (arcfp != NULL) {
			fclose(arcfp);
			arcfp = NULL;
		}
		end_critical_section(S_SUPPMSGMAIN);
		return;
	}

	/*
	 * If we can't open the queue, perform the operation synchronously.
	 */
	if (arcfp == NULL) {
		TDAP_AdjRefCount(msgnum, incr);
		return;
	}

	new_arcq.arcq_msgnum = msgnum;
	new_arcq.arcq_delta = incr;
	rv = fwrite(&new_arcq, sizeof(struct arcq), 1, arcfp);
	fflush(arcfp);

	return;
}


/*
 * TDAP_ProcessAdjRefCountQueue()
 *
 * Process the queue of message count adjustments that was created by calls
 * to AdjRefCount() ... by reading the queue and calling TDAP_AdjRefCount()
 * for each one.  This should be an "off hours" operation.
 */
int TDAP_ProcessAdjRefCountQueue(void)
{
	char file_arcq_temp[PATH_MAX];
	int r;
	FILE *fp;
	struct arcq arcq_rec;
	int num_records_processed = 0;

	snprintf(file_arcq_temp, sizeof file_arcq_temp, "%s.%04x", file_arcq, rand());

	begin_critical_section(S_SUPPMSGMAIN);
	if (arcfp != NULL) {
		fclose(arcfp);
		arcfp = NULL;
	}

	r = link(file_arcq, file_arcq_temp);
	if (r != 0) {
		CtdlLogPrintf(CTDL_CRIT, "%s: %s\n", file_arcq_temp, strerror(errno));
		end_critical_section(S_SUPPMSGMAIN);
		return(num_records_processed);
	}

	unlink(file_arcq);
	end_critical_section(S_SUPPMSGMAIN);

	fp = fopen(file_arcq_temp, "rb");
	if (fp == NULL) {
		CtdlLogPrintf(CTDL_CRIT, "%s: %s\n", file_arcq_temp, strerror(errno));
		return(num_records_processed);
	}

	while (fread(&arcq_rec, sizeof(struct arcq), 1, fp) == 1) {
		TDAP_AdjRefCount(arcq_rec.arcq_msgnum, arcq_rec.arcq_delta);
		++num_records_processed;
	}

	fclose(fp);
	r = unlink(file_arcq_temp);
	if (r != 0) {
		CtdlLogPrintf(CTDL_CRIT, "%s: %s\n", file_arcq_temp, strerror(errno));
	}

	return(num_records_processed);
}



/*
 * TDAP_AdjRefCount  -  adjust the reference count for a message.
 *                      This one does it "for real" because it's called by
 *                      the autopurger function that processes the queue
 *                      created by AdjRefCount().   If a message's reference
 *                      count becomes zero, we also delete the message from
 *                      disk and de-index it.
 */
void TDAP_AdjRefCount(long msgnum, int incr)
{

	struct MetaData smi;
	long delnum;

	/* This is a *tight* critical section; please keep it that way, as
	 * it may get called while nested in other critical sections.  
	 * Complicating this any further will surely cause deadlock!
	 */
	begin_critical_section(S_SUPPMSGMAIN);
	GetMetaData(&smi, msgnum);
	smi.meta_refcount += incr;
	PutMetaData(&smi);
	end_critical_section(S_SUPPMSGMAIN);
	CtdlLogPrintf(CTDL_DEBUG, "msg %ld ref count delta %+d, is now %d\n",
		msgnum, incr, smi.meta_refcount);

	/* If the reference count is now zero, delete the message
	 * (and its supplementary record as well).
	 */
	if (smi.meta_refcount == 0) {
		CtdlLogPrintf(CTDL_DEBUG, "Deleting message <%ld>\n", msgnum);
		
		/* Call delete hooks with NULL room to show it has gone altogether */
		PerformDeleteHooks(NULL, msgnum);

		/* Remove from message base */
		delnum = msgnum;
		cdb_delete(CDB_MSGMAIN, &delnum, (int)sizeof(long));
		cdb_delete(CDB_BIGMSGS, &delnum, (int)sizeof(long));

		/* Remove metadata record */
		delnum = (0L - msgnum);
		cdb_delete(CDB_MSGMAIN, &delnum, (int)sizeof(long));
	}

}

/*
 * Write a generic object to this room
 *
 * Note: this could be much more efficient.  Right now we use two temporary
 * files, and still pull the message into memory as with all others.
 */
void CtdlWriteObject(char *req_room,			/* Room to stuff it in */
			char *content_type,		/* MIME type of this object */
			char *raw_message,		/* Data to be written */
			off_t raw_length,		/* Size of raw_message */
			struct ctdluser *is_mailbox,	/* Mailbox room? */
			int is_binary,			/* Is encoding necessary? */
			int is_unique,			/* Del others of this type? */
			unsigned int flags		/* Internal save flags */
			)
{

	struct ctdlroom qrbuf;
	char roomname[ROOMNAMELEN];
	struct CtdlMessage *msg;
	char *encoded_message = NULL;

	if (is_mailbox != NULL) {
		CtdlMailboxName(roomname, sizeof roomname, is_mailbox, req_room);
	}
	else {
		safestrncpy(roomname, req_room, sizeof(roomname));
	}

	CtdlLogPrintf(CTDL_DEBUG, "Raw length is %ld\n", (long)raw_length);

	if (is_binary) {
		encoded_message = malloc((size_t) (((raw_length * 134) / 100) + 4096 ) );
	}
	else {
		encoded_message = malloc((size_t)(raw_length + 4096));
	}

	sprintf(encoded_message, "Content-type: %s\n", content_type);

	if (is_binary) {
		sprintf(&encoded_message[strlen(encoded_message)],
			"Content-transfer-encoding: base64\n\n"
		);
	}
	else {
		sprintf(&encoded_message[strlen(encoded_message)],
			"Content-transfer-encoding: 7bit\n\n"
		);
	}

	if (is_binary) {
		CtdlEncodeBase64(
			&encoded_message[strlen(encoded_message)],
			raw_message,
			(int)raw_length,
			0
		);
	}
	else {
		memcpy(
			&encoded_message[strlen(encoded_message)],
			raw_message,
			(int)(raw_length+1)
		);
	}

	CtdlLogPrintf(CTDL_DEBUG, "Allocating\n");
	msg = malloc(sizeof(struct CtdlMessage));
	memset(msg, 0, sizeof(struct CtdlMessage));
	msg->cm_magic = CTDLMESSAGE_MAGIC;
	msg->cm_anon_type = MES_NORMAL;
	msg->cm_format_type = 4;
	msg->cm_fields['A'] = strdup(CC->user.fullname);
	msg->cm_fields['O'] = strdup(req_room);
	msg->cm_fields['N'] = strdup(config.c_nodename);
	msg->cm_fields['H'] = strdup(config.c_humannode);
	msg->cm_flags = flags;
	
	msg->cm_fields['M'] = encoded_message;

	/* Create the requested room if we have to. */
	if (CtdlGetRoom(&qrbuf, roomname) != 0) {
		CtdlCreateRoom(roomname, 
			( (is_mailbox != NULL) ? 5 : 3 ),
			"", 0, 1, 0, VIEW_BBS);
	}
	/* If the caller specified this object as unique, delete all
	 * other objects of this type that are currently in the room.
	 */
	if (is_unique) {
		CtdlLogPrintf(CTDL_DEBUG, "Deleted %d other msgs of this type\n",
			CtdlDeleteMessages(roomname, NULL, 0, content_type)
		);
	}
	/* Now write the data */
	CtdlSubmitMsg(msg, NULL, roomname, 0);
	CtdlFreeMessage(msg);
}






void CtdlGetSysConfigBackend(long msgnum, void *userdata) {
	config_msgnum = msgnum;
}


char *CtdlGetSysConfig(char *sysconfname) {
	char hold_rm[ROOMNAMELEN];
	long msgnum;
	char *conf;
	struct CtdlMessage *msg;
	char buf[SIZ];
	
	strcpy(hold_rm, CC->room.QRname);
	if (CtdlGetRoom(&CC->room, SYSCONFIGROOM) != 0) {
		CtdlGetRoom(&CC->room, hold_rm);
		return NULL;
	}


	/* We want the last (and probably only) config in this room */
	begin_critical_section(S_CONFIG);
	config_msgnum = (-1L);
	CtdlForEachMessage(MSGS_LAST, 1, NULL, sysconfname, NULL,
		CtdlGetSysConfigBackend, NULL);
	msgnum = config_msgnum;
	end_critical_section(S_CONFIG);

	if (msgnum < 0L) {
		conf = NULL;
	}
	else {
		msg = CtdlFetchMessage(msgnum, 1);
		if (msg != NULL) {
			conf = strdup(msg->cm_fields['M']);
			CtdlFreeMessage(msg);
		}
		else {
			conf = NULL;
		}
	}

	CtdlGetRoom(&CC->room, hold_rm);

	if (conf != NULL) do {
		extract_token(buf, conf, 0, '\n', sizeof buf);
		strcpy(conf, &conf[strlen(buf)+1]);
	} while ( (!IsEmptyStr(conf)) && (!IsEmptyStr(buf)) );

	return(conf);
}


void CtdlPutSysConfig(char *sysconfname, char *sysconfdata) {
	CtdlWriteObject(SYSCONFIGROOM, sysconfname, sysconfdata, (strlen(sysconfdata)+1), NULL, 0, 1, 0);
}


/*
 * Determine whether a given Internet address belongs to the current user
 */
int CtdlIsMe(char *addr, int addr_buf_len)
{
	struct recptypes *recp;
	int i;

	recp = validate_recipients(addr, NULL, 0);
	if (recp == NULL) return(0);

	if (recp->num_local == 0) {
		free_recipients(recp);
		return(0);
	}

	for (i=0; i<recp->num_local; ++i) {
		extract_token(addr, recp->recp_local, i, '|', addr_buf_len);
		if (!strcasecmp(addr, CC->user.fullname)) {
			free_recipients(recp);
			return(1);
		}
	}

	free_recipients(recp);
	return(0);
}


/*
 * Citadel protocol command to do the same
 */
void cmd_isme(char *argbuf) {
	char addr[256];

	if (CtdlAccessCheck(ac_logged_in)) return;
	extract_token(addr, argbuf, 0, '|', sizeof addr);

	if (CtdlIsMe(addr, sizeof addr)) {
		cprintf("%d %s\n", CIT_OK, addr);
	}
	else {
		cprintf("%d Not you.\n", ERROR + ILLEGAL_VALUE);
	}

}


/*****************************************************************************/
/*                      MODULE INITIALIZATION STUFF                          */
/*****************************************************************************/

CTDL_MODULE_INIT(msgbase)
{
	if (!threading) {
		CtdlRegisterProtoHook(cmd_msgs, "MSGS", "Output a list of messages in the current room");
		CtdlRegisterProtoHook(cmd_msg0, "MSG0", "Output a message in plain text format");
		CtdlRegisterProtoHook(cmd_msg2, "MSG2", "Output a message in RFC822 format");
		CtdlRegisterProtoHook(cmd_msg3, "MSG3", "Output a message in raw format (deprecated)");
		CtdlRegisterProtoHook(cmd_msg4, "MSG4", "Output a message in the client's preferred format");
		CtdlRegisterProtoHook(cmd_msgp, "MSGP", "Select preferred format for MSG4 output");
		CtdlRegisterProtoHook(cmd_opna, "OPNA", "Open an attachment for download");
		CtdlRegisterProtoHook(cmd_dlat, "DLAT", "Download an attachment");
		CtdlRegisterProtoHook(cmd_ent0, "ENT0", "Enter a message");
		CtdlRegisterProtoHook(cmd_dele, "DELE", "Delete a message");
		CtdlRegisterProtoHook(cmd_move, "MOVE", "Move or copy a message to another room");
		CtdlRegisterProtoHook(cmd_isme, "ISME", "Determine whether an email address belongs to a user");
	}

        /* return our Subversion id for the Log */
	return "$Id$";
}
