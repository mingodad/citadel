/*
 * $Id$ 
 *
 * IMAP server for the Citadel system
 * Copyright (C) 2000-2007 by Art Cancro and others.
 * This code is released under the terms of the GNU General Public License.
 *
 * WARNING: the IMAP protocol is badly designed.  No implementation of it
 * is perfect.  Indeed, with so much gratuitous complexity, *all* IMAP
 * implementations have bugs.
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <pwd.h>
#include <errno.h>
#include <sys/types.h>

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

#include <sys/wait.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include "citadel.h"
#include "server.h"
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "serv_extensions.h"
#include "room_ops.h"
#include "user_ops.h"
#include "policy.h"
#include "database.h"
#include "msgbase.h"
#include "tools.h"
#include "internet_addressing.h"
#include "serv_imap.h"
#include "imap_tools.h"
#include "imap_fetch.h"
#include "imap_search.h"
#include "imap_store.h"
#include "imap_acl.h"
#include "imap_misc.h"

#ifdef HAVE_OPENSSL
#include "serv_crypto.h"
#endif

/* imap_rename() uses this struct containing list of rooms to rename */
struct irl {
	struct irl *next;
	char irl_oldroom[ROOMNAMELEN];
	char irl_newroom[ROOMNAMELEN];
	int irl_newfloor;
};

/* Data which is passed between imap_rename() and imap_rename_backend() */
struct irlparms {
	char *oldname;
	char *newname;
	struct irl **irl;
};


/*
 * If there is a message ID map in memory, free it
 */
void imap_free_msgids(void)
{
	if (IMAP->msgids != NULL) {
		free(IMAP->msgids);
		IMAP->msgids = NULL;
		IMAP->num_msgs = 0;
		IMAP->num_alloc = 0;
	}
	if (IMAP->flags != NULL) {
		free(IMAP->flags);
		IMAP->flags = NULL;
	}
	IMAP->last_mtime = (-1);
}


/*
 * If there is a transmitted message in memory, free it
 */
void imap_free_transmitted_message(void)
{
	if (IMAP->transmitted_message != NULL) {
		free(IMAP->transmitted_message);
		IMAP->transmitted_message = NULL;
		IMAP->transmitted_length = 0;
	}
}


/*
 * Set the \Seen, \Recent. and \Answered flags, based on the sequence
 * sets stored in the visit record for this user/room.  Note that we have
 * to parse each sequence set manually here, because calling the utility
 * function is_msg_in_sequence_set() over and over again is too expensive.
 *
 * first_msg should be set to 0 to rescan the flags for every message in the
 * room, or some other value if we're only interested in an incremental
 * update.
 */
void imap_set_seen_flags(int first_msg)
{
	struct visit vbuf;
	int i;
	int num_sets;
	int s;
	char setstr[64], lostr[64], histr[64];
	long lo, hi;

	if (IMAP->num_msgs < 1) return;
	CtdlGetRelationship(&vbuf, &CC->user, &CC->room);

	for (i = first_msg; i < IMAP->num_msgs; ++i) {
		IMAP->flags[i] = IMAP->flags[i] & ~IMAP_SEEN;
		IMAP->flags[i] |= IMAP_RECENT;
		IMAP->flags[i] = IMAP->flags[i] & ~IMAP_ANSWERED;
	}

	/*
	 * Do the "\Seen" flag.
	 * (Any message not "\Seen" is considered "\Recent".)
	 */
	num_sets = num_tokens(vbuf.v_seen, ',');
	for (s=0; s<num_sets; ++s) {
		extract_token(setstr, vbuf.v_seen, s, ',', sizeof setstr);

		extract_token(lostr, setstr, 0, ':', sizeof lostr);
		if (num_tokens(setstr, ':') >= 2) {
			extract_token(histr, setstr, 1, ':', sizeof histr);
			if (!strcmp(histr, "*")) {
				snprintf(histr, sizeof histr, "%ld", LONG_MAX);
			}
		} 
		else {
			strcpy(histr, lostr);
		}
		lo = atol(lostr);
		hi = atol(histr);

		for (i = first_msg; i < IMAP->num_msgs; ++i) {
			if ((IMAP->msgids[i] >= lo) && (IMAP->msgids[i] <= hi)){
				IMAP->flags[i] |= IMAP_SEEN;
				IMAP->flags[i] = IMAP->flags[i] & ~IMAP_RECENT;
			}
		}
	}

	/* Do the ANSWERED flag */
	num_sets = num_tokens(vbuf.v_answered, ',');
	for (s=0; s<num_sets; ++s) {
		extract_token(setstr, vbuf.v_answered, s, ',', sizeof setstr);

		extract_token(lostr, setstr, 0, ':', sizeof lostr);
		if (num_tokens(setstr, ':') >= 2) {
			extract_token(histr, setstr, 1, ':', sizeof histr);
			if (!strcmp(histr, "*")) {
				snprintf(histr, sizeof histr, "%ld", LONG_MAX);
			}
		} 
		else {
			strcpy(histr, lostr);
		}
		lo = atol(lostr);
		hi = atol(histr);

		for (i = first_msg; i < IMAP->num_msgs; ++i) {
			if ((IMAP->msgids[i] >= lo) && (IMAP->msgids[i] <= hi)){
				IMAP->flags[i] |= IMAP_ANSWERED;
			}
		}
	}

}



/*
 * Back end for imap_load_msgids()
 *
 * Optimization: instead of calling realloc() to add each message, we
 * allocate space in the list for REALLOC_INCREMENT messages at a time.  This
 * allows the mapping to proceed much faster.
 */
void imap_add_single_msgid(long msgnum, void *userdata)
{

	++IMAP->num_msgs;
	if (IMAP->num_msgs > IMAP->num_alloc) {
		IMAP->num_alloc += REALLOC_INCREMENT;
		IMAP->msgids = realloc(IMAP->msgids,
					(IMAP->num_alloc * sizeof(long)) );
		IMAP->flags = realloc(IMAP->flags,
					(IMAP->num_alloc * sizeof(long)) );
	}
	IMAP->msgids[IMAP->num_msgs - 1] = msgnum;
	IMAP->flags[IMAP->num_msgs - 1] = 0;
}



/*
 * Set up a message ID map for the current room (folder)
 */
void imap_load_msgids(void)
{
	struct cdbdata *cdbfr;

	if (IMAP->selected == 0) {
		lprintf(CTDL_ERR,
			"imap_load_msgids() can't run; no room selected\n");
		return;
	}

	imap_free_msgids();	/* If there was already a map, free it */

	/* Load the message list */
	cdbfr = cdb_fetch(CDB_MSGLISTS, &CC->room.QRnumber, sizeof(long));
	if (cdbfr != NULL) {
		IMAP->msgids = malloc(cdbfr->len);
		memcpy(IMAP->msgids, cdbfr->ptr, cdbfr->len);
		IMAP->num_msgs = cdbfr->len / sizeof(long);
		IMAP->num_alloc = cdbfr->len / sizeof(long);
		cdb_free(cdbfr);
	}

	if (IMAP->num_msgs) {
		IMAP->flags = malloc(IMAP->num_alloc * sizeof(long));
		memset(IMAP->flags, 0, (IMAP->num_alloc * sizeof(long)) );
	}

	imap_set_seen_flags(0);
}


/*
 * Re-scan the selected room (folder) and see if it's been changed at all
 */
void imap_rescan_msgids(void)
{

	int original_num_msgs = 0;
	long original_highest = 0L;
	int i, j, jstart;
	int message_still_exists;
	struct cdbdata *cdbfr;
	long *msglist = NULL;
	int num_msgs = 0;
	int num_recent = 0;

	if (IMAP->selected == 0) {
		lprintf(CTDL_ERR,
			"imap_load_msgids() can't run; no room selected\n");
		return;
	}

	/*
	 * Check to see if the room's contents have changed.
	 * If not, we can avoid this rescan.
	 */
	getroom(&CC->room, CC->room.QRname);
	if (IMAP->last_mtime == CC->room.QRmtime) {	/* No changes! */
		return;
	}

	/* Load the *current* message list from disk, so we can compare it
	 * to what we have in memory.
	 */
	cdbfr = cdb_fetch(CDB_MSGLISTS, &CC->room.QRnumber, sizeof(long));
	if (cdbfr != NULL) {
		msglist = malloc(cdbfr->len);
		if (msglist == NULL) {
			lprintf(CTDL_CRIT, "malloc() failed\n");
			abort();
		}
		memcpy(msglist, cdbfr->ptr, (size_t)cdbfr->len);
		num_msgs = cdbfr->len / sizeof(long);
		cdb_free(cdbfr);
	} else {
		num_msgs = 0;
	}

	/*
	 * Check to see if any of the messages we know about have been expunged
	 */
	if (IMAP->num_msgs > 0) {
		jstart = 0;
		for (i = 0; i < IMAP->num_msgs; ++i) {

			message_still_exists = 0;
			if (num_msgs > 0) {
				for (j = jstart; j < num_msgs; ++j) {
					if (msglist[j] == IMAP->msgids[i]) {
						message_still_exists = 1;
						jstart = j;
						break;
					}
				}
			}

			if (message_still_exists == 0) {
				cprintf("* %d EXPUNGE\r\n", i + 1);

				/* Here's some nice stupid nonsense.  When a
				 * message is expunged, we have to slide all
				 * the existing messages up in the message
				 * array.
				 */
				--IMAP->num_msgs;
				memcpy(&IMAP->msgids[i],
				       &IMAP->msgids[i + 1],
				       (sizeof(long) *
					(IMAP->num_msgs - i)));
				memcpy(&IMAP->flags[i],
				       &IMAP->flags[i + 1],
				       (sizeof(long) *
					(IMAP->num_msgs - i)));

				--i;
			}

		}
	}

	/*
	 * Remember how many messages were here before we re-scanned.
	 */
	original_num_msgs = IMAP->num_msgs;
	if (IMAP->num_msgs > 0) {
		original_highest = IMAP->msgids[IMAP->num_msgs - 1];
	} else {
		original_highest = 0L;
	}

	/*
	 * Now peruse the room for *new* messages only.
	 */
	if (num_msgs > 0) {
		for (j = 0; j < num_msgs; ++j) {
			if (msglist[j] > original_highest) {
				imap_add_single_msgid(msglist[j], NULL);
			}
		}
	}
	imap_set_seen_flags(original_num_msgs);

	/*
	 * If new messages have arrived, tell the client about them.
	 */
	if (IMAP->num_msgs > original_num_msgs) {

		for (j = 0; j < num_msgs; ++j) {
			if (IMAP->flags[j] & IMAP_RECENT) {
				++num_recent;
			}
		}

		cprintf("* %d EXISTS\r\n", IMAP->num_msgs);
		cprintf("* %d RECENT\r\n", num_recent);
	}

	if (num_msgs != 0) {
		free(msglist);
	}
	IMAP->last_mtime = CC->room.QRmtime;
}







/*
 * This cleanup function blows away the temporary memory and files used by
 * the IMAP server.
 */
void imap_cleanup_function(void)
{

	/* Don't do this stuff if this is not a IMAP session! */
	if (CC->h_command_function != imap_command_loop)
		return;

	/* If there is a mailbox selected, auto-expunge it. */
	if (IMAP->selected) {
		imap_do_expunge();
	}

	lprintf(CTDL_DEBUG, "Performing IMAP cleanup hook\n");
	imap_free_msgids();
	imap_free_transmitted_message();

	if (IMAP->cached_rfc822_data != NULL) {
		free(IMAP->cached_rfc822_data);
		IMAP->cached_rfc822_data = NULL;
		IMAP->cached_rfc822_msgnum = (-1);
		IMAP->cached_rfc822_withbody = 0;
	}

	if (IMAP->cached_body != NULL) {
		free(IMAP->cached_body);
		IMAP->cached_body = NULL;
		IMAP->cached_body_len = 0;
		IMAP->cached_bodymsgnum = (-1);
	}

	free(IMAP);
	lprintf(CTDL_DEBUG, "Finished IMAP cleanup hook\n");
}


/*
 * Does the actual work of the CAPABILITY command (because we need to
 * output this stuff in other places as well)
 */
void imap_output_capability_string(void) {
	cprintf("CAPABILITY IMAP4REV1 NAMESPACE ID ACL AUTH=LOGIN");
#ifdef HAVE_OPENSSL
	if (!CC->redirect_ssl) cprintf(" STARTTLS");
#endif
}

/*
 * implements the CAPABILITY command
 */
void imap_capability(int num_parms, char *parms[])
{
	cprintf("* ");
	imap_output_capability_string();
	cprintf("\r\n");
	cprintf("%s OK CAPABILITY completed\r\n", parms[0]);
}



/*
 * Implements the ID command (specified by RFC2971)
 *
 * We ignore the client-supplied information, and output a NIL response.
 * Although this is technically a valid implementation of the extension, it
 * is quite useless.  It exists only so that we may see which clients are
 * making use of this extension.
 * 
 */
void imap_id(int num_parms, char *parms[])
{
	cprintf("* ID NIL\r\n");
	cprintf("%s OK ID completed\r\n", parms[0]);
}



/*
 * Here's where our IMAP session begins its happy day.
 */
void imap_greeting(void)
{

	strcpy(CC->cs_clientname, "IMAP session");
	IMAP = malloc(sizeof (struct citimap));
	memset(IMAP, 0, sizeof(struct citimap));
	IMAP->authstate = imap_as_normal;
	IMAP->cached_rfc822_data = NULL;
	IMAP->cached_rfc822_msgnum = (-1);
	IMAP->cached_rfc822_withbody = 0;

	cprintf("* OK [");
	imap_output_capability_string();
	cprintf("] %s IMAP4rev1 %s ready\r\n", config.c_fqdn, CITADEL);
}

/*
 * IMAPS is just like IMAP, except it goes crypto right away.
 */
#ifdef HAVE_OPENSSL
void imaps_greeting(void) {
	CtdlStartTLS(NULL, NULL, NULL);
	imap_greeting();
}
#endif


/*
 * implements the LOGIN command (ordinary username/password login)
 */
void imap_login(int num_parms, char *parms[])
{
	if (num_parms != 4) {
		cprintf("%s BAD incorrect number of parameters\r\n", parms[0]);
		return;
	}

	if (CtdlLoginExistingUser(parms[2]) == login_ok) {
		if (CtdlTryPassword(parms[3]) == pass_ok) {
			cprintf("%s OK [", parms[0]);
			imap_output_capability_string();
			cprintf("] Hello, %s\r\n", CC->user.fullname);
			return;
		}
	}

	cprintf("%s BAD Login incorrect\r\n", parms[0]);
}


/*
 * Implements the AUTHENTICATE command
 */
void imap_authenticate(int num_parms, char *parms[])
{
	char buf[SIZ];

	if (num_parms != 3) {
		cprintf("%s BAD incorrect number of parameters\r\n",
			parms[0]);
		return;
	}

	if (CC->logged_in) {
		cprintf("%s BAD Already logged in.\r\n", parms[0]);
		return;
	}

	if (!strcasecmp(parms[2], "LOGIN")) {
		CtdlEncodeBase64(buf, "Username:", 9);
		cprintf("+ %s\r\n", buf);
		IMAP->authstate = imap_as_expecting_username;
		strcpy(IMAP->authseq, parms[0]);
		return;
	}

	else {
		cprintf("%s NO AUTHENTICATE %s failed\r\n",
			parms[0], parms[1]);
	}
}

void imap_auth_login_user(char *cmd)
{
	char buf[SIZ];

	CtdlDecodeBase64(buf, cmd, SIZ);
	CtdlLoginExistingUser(buf);
	CtdlEncodeBase64(buf, "Password:", 9);
	cprintf("+ %s\r\n", buf);
	IMAP->authstate = imap_as_expecting_password;
	return;
}

void imap_auth_login_pass(char *cmd)
{
	char buf[SIZ];

	CtdlDecodeBase64(buf, cmd, SIZ);
	if (CtdlTryPassword(buf) == pass_ok) {
		cprintf("%s OK authentication succeeded\r\n",
			IMAP->authseq);
	} else {
		cprintf("%s NO authentication failed\r\n", IMAP->authseq);
	}
	IMAP->authstate = imap_as_normal;
	return;
}


/*
 * implements the STARTTLS command (Citadel API version)
 */
#ifdef HAVE_OPENSSL
void imap_starttls(int num_parms, char *parms[])
{
	char ok_response[SIZ];
	char nosup_response[SIZ];
	char error_response[SIZ];

	sprintf(ok_response,
		"%s OK begin TLS negotiation now\r\n",
		parms[0]);
	sprintf(nosup_response,
		"%s NO TLS not supported here\r\n",
		parms[0]);
	sprintf(error_response,
		"%s BAD Internal error\r\n",
		parms[0]);
	CtdlStartTLS(ok_response, nosup_response, error_response);
}
#endif


/*
 * implements the SELECT command
 */
void imap_select(int num_parms, char *parms[])
{
	char towhere[SIZ];
	char augmented_roomname[ROOMNAMELEN];
	int c = 0;
	int ok = 0;
	int ra = 0;
	struct ctdlroom QRscratch;
	int msgs, new;
	int floornum;
	int roomflags;
	int i;

	/* Convert the supplied folder name to a roomname */
	i = imap_roomname(towhere, sizeof towhere, parms[2]);
	if (i < 0) {
		cprintf("%s NO Invalid mailbox name.\r\n", parms[0]);
		IMAP->selected = 0;
		return;
	}
	floornum = (i & 0x00ff);
	roomflags = (i & 0xff00);

	/* First try a regular match */
	c = getroom(&QRscratch, towhere);

	/* Then try a mailbox name match */
	if (c != 0) {
		MailboxName(augmented_roomname, sizeof augmented_roomname,
			    &CC->user, towhere);
		c = getroom(&QRscratch, augmented_roomname);
		if (c == 0)
			strcpy(towhere, augmented_roomname);
	}

	/* If the room exists, check security/access */
	if (c == 0) {
		/* See if there is an existing user/room relationship */
		CtdlRoomAccess(&QRscratch, &CC->user, &ra, NULL);

		/* normal clients have to pass through security */
		if (ra & UA_KNOWN) {
			ok = 1;
		}
	}

	/* Fail here if no such room */
	if (!ok) {
		cprintf("%s NO ... no such room, or access denied\r\n",
			parms[0]);
		return;
	}

	/* If we already had some other folder selected, auto-expunge it */
	imap_do_expunge();

	/*
	 * usergoto() formally takes us to the desired room, happily returning
	 * the number of messages and number of new messages.
	 */
	memcpy(&CC->room, &QRscratch, sizeof(struct ctdlroom));
	usergoto(NULL, 0, 0, &msgs, &new);
	IMAP->selected = 1;

	if (!strcasecmp(parms[1], "EXAMINE")) {
		IMAP->readonly = 1;
	} else {
		IMAP->readonly = 0;
	}

	imap_load_msgids();
	IMAP->last_mtime = CC->room.QRmtime;

	cprintf("* %d EXISTS\r\n", msgs);
	cprintf("* %d RECENT\r\n", new);

	cprintf("* OK [UIDVALIDITY 1] UID validity status\r\n");
	cprintf("* OK [UIDNEXT %ld] Predicted next UID\r\n", CitControl.MMhighest + 1);

	/* Note that \Deleted is a valid flag, but not a permanent flag,
	 * because we don't maintain its state across sessions.  Citadel
	 * automatically expunges mailboxes when they are de-selected.
	 */
	cprintf("* FLAGS (\\Deleted \\Seen \\Answered)\r\n");
	cprintf("* OK [PERMANENTFLAGS (\\Deleted \\Seen \\Answered)] "
		"permanent flags\r\n");

	cprintf("%s OK [%s] %s completed\r\n",
		parms[0],
		(IMAP->readonly ? "READ-ONLY" : "READ-WRITE"), parms[1]);
}



/*
 * Does the real work for expunge.
 */
int imap_do_expunge(void)
{
	int i;
	int num_expunged = 0;
	long *delmsgs = NULL;
	int num_delmsgs = 0;

	lprintf(CTDL_DEBUG, "imap_do_expunge() called\n");
	if (IMAP->selected == 0) {
		return (0);
	}

	if (IMAP->num_msgs > 0) {
		delmsgs = malloc(IMAP->num_msgs * sizeof(long));
		for (i = 0; i < IMAP->num_msgs; ++i) {
			if (IMAP->flags[i] & IMAP_DELETED) {
				delmsgs[num_delmsgs++] = IMAP->msgids[i];
			}
		}
		if (num_delmsgs > 0) {
			CtdlDeleteMessages(CC->room.QRname, delmsgs, num_delmsgs, "");
		}
		num_expunged += num_delmsgs;
		free(delmsgs);
	}

	if (num_expunged > 0) {
		imap_rescan_msgids();
	}

	lprintf(CTDL_DEBUG, "Expunged %d messages from <%s>\n",
		num_expunged, CC->room.QRname);
	return (num_expunged);
}


/*
 * implements the EXPUNGE command syntax
 */
void imap_expunge(int num_parms, char *parms[])
{
	int num_expunged = 0;

	num_expunged = imap_do_expunge();
	cprintf("%s OK expunged %d messages.\r\n", parms[0], num_expunged);
}


/*
 * implements the CLOSE command
 */
void imap_close(int num_parms, char *parms[])
{

	/* Yes, we always expunge on close. */
	if (IMAP->selected) {
		imap_do_expunge();
	}

	IMAP->selected = 0;
	IMAP->readonly = 0;
	imap_free_msgids();
	cprintf("%s OK CLOSE completed\r\n", parms[0]);
}


/*
 * Implements the NAMESPACE command.
 */
void imap_namespace(int num_parms, char *parms[])
{
	int i;
	struct floor *fl;
	int floors = 0;
	char buf[SIZ];

	cprintf("* NAMESPACE ");

	/* All personal folders are subordinate to INBOX. */
	cprintf("((\"INBOX/\" \"/\")) ");

	/* Other users' folders ... coming soon! FIXME */
	cprintf("NIL ");

	/* Show all floors as shared namespaces.  Neato! */
	cprintf("(");
	for (i = 0; i < MAXFLOORS; ++i) {
		fl = cgetfloor(i);
		if (fl->f_flags & F_INUSE) {
			if (floors > 0) cprintf(" ");
			cprintf("(");
			sprintf(buf, "%s/", fl->f_name);
			imap_strout(buf);
			cprintf(" \"/\")");
			++floors;
		}
	}
	cprintf(")");

	/* Wind it up with a newline and a completion message. */
	cprintf("\r\n");
	cprintf("%s OK NAMESPACE completed\r\n", parms[0]);
}



/*
 * Used by LIST and LSUB to show the floors in the listing
 */
void imap_list_floors(char *cmd, char *pattern)
{
	int i;
	struct floor *fl;

	for (i = 0; i < MAXFLOORS; ++i) {
		fl = cgetfloor(i);
		if (fl->f_flags & F_INUSE) {
			if (imap_mailbox_matches_pattern
			    (pattern, fl->f_name)) {
				cprintf("* %s (\\NoSelect) \"/\" ", cmd);
				imap_strout(fl->f_name);
				cprintf("\r\n");
			}
		}
	}
}



/*
 * Back end for imap_lsub()
 *
 * IMAP "subscribed folder" is equivocated to Citadel "known rooms."  This
 * may or may not be the desired behavior in the future.
 */
void imap_lsub_listroom(struct ctdlroom *qrbuf, void *data)
{
	char buf[SIZ];
	int ra;
	char *pattern;

	pattern = (char *) data;

	/* Only list rooms to which the user has access!! */
	CtdlRoomAccess(qrbuf, &CC->user, &ra, NULL);
	if (ra & UA_KNOWN) {
		imap_mailboxname(buf, sizeof buf, qrbuf);
		if (imap_mailbox_matches_pattern(pattern, buf)) {
			cprintf("* LSUB () \"/\" ");
			imap_strout(buf);
			cprintf("\r\n");
		}
	}
}


/*
 * Implements the LSUB command
 */
void imap_lsub(int num_parms, char *parms[])
{
	char pattern[SIZ];
	if (num_parms < 4) {
		cprintf("%s BAD arguments invalid\r\n", parms[0]);
		return;
	}
	snprintf(pattern, sizeof pattern, "%s%s", parms[2], parms[3]);

	if (strlen(parms[3]) == 0) {
		cprintf("* LIST (\\Noselect) \"/\" \"\"\r\n");
	}

	else {
		imap_list_floors("LSUB", pattern);
		ForEachRoom(imap_lsub_listroom, pattern);
	}

	cprintf("%s OK LSUB completed\r\n", parms[0]);
}



/*
 * Back end for imap_list()
 */
void imap_list_listroom(struct ctdlroom *qrbuf, void *data)
{
	char buf[SIZ];
	int ra;
	char *pattern;

	pattern = (char *) data;

	/* Only list rooms to which the user has access!! */
	CtdlRoomAccess(qrbuf, &CC->user, &ra, NULL);
	if ((ra & UA_KNOWN)
	    || ((ra & UA_GOTOALLOWED) && (ra & UA_ZAPPED))) {
		imap_mailboxname(buf, sizeof buf, qrbuf);
		if (imap_mailbox_matches_pattern(pattern, buf)) {
			cprintf("* LIST () \"/\" ");
			imap_strout(buf);
			cprintf("\r\n");
		}
	}
}


/*
 * Implements the LIST command
 */
void imap_list(int num_parms, char *parms[])
{
	char pattern[SIZ];
	if (num_parms < 4) {
		cprintf("%s BAD arguments invalid\r\n", parms[0]);
		return;
	}
	snprintf(pattern, sizeof pattern, "%s%s", parms[2], parms[3]);

	if (strlen(parms[3]) == 0) {
		cprintf("* LIST (\\Noselect) \"/\" \"\"\r\n");
	}

	else {
		imap_list_floors("LIST", pattern);
		ForEachRoom(imap_list_listroom, pattern);
	}

	cprintf("%s OK LIST completed\r\n", parms[0]);
}



/*
 * Implements the CREATE command
 *
 */
void imap_create(int num_parms, char *parms[])
{
	int ret;
	char roomname[ROOMNAMELEN];
	int floornum;
	int flags;
	int newroomtype = 0;
	int newroomview = 0;

	if (strchr(parms[2], '\\') != NULL) {
		cprintf("%s NO Invalid character in folder name\r\n",
			parms[0]);
		lprintf(CTDL_DEBUG, "invalid character in folder name\n");
		return;
	}

	ret = imap_roomname(roomname, sizeof roomname, parms[2]);
	if (ret < 0) {
		cprintf("%s NO Invalid mailbox name or location\r\n",
			parms[0]);
		lprintf(CTDL_DEBUG, "invalid mailbox name or location\n");
		return;
	}
	floornum = (ret & 0x00ff);	/* lower 8 bits = floor number */
	flags = (ret & 0xff00);	/* upper 8 bits = flags        */

	if (flags & IR_MAILBOX) {
		if (strncasecmp(parms[2], "INBOX/", 6)) {
			cprintf("%s NO Personal folders must be created under INBOX\r\n", parms[0]);
			lprintf(CTDL_DEBUG, "not subordinate to inbox\n");
			return;
		}
	}

	if (flags & IR_MAILBOX) {
		newroomtype = 4;		/* private mailbox */
		newroomview = VIEW_MAILBOX;
	} else {
		newroomtype = 0;		/* public folder */
		newroomview = VIEW_BBS;
	}

	lprintf(CTDL_INFO, "Create new room <%s> on floor <%d> with type <%d>\n",
		roomname, floornum, newroomtype);

	ret = create_room(roomname, newroomtype, "", floornum, 1, 0, newroomview);
	if (ret == 0) {
		cprintf
		    ("%s NO Mailbox already exists, or create failed\r\n",
		     parms[0]);
	} else {
		cprintf("%s OK CREATE completed\r\n", parms[0]);
	}
	lprintf(CTDL_DEBUG, "imap_create() completed\n");
}


/*
 * Locate a room by its IMAP folder name, and check access to it.
 * If zapped_ok is nonzero, we can also look for the room in the zapped list.
 */
int imap_grabroom(char *returned_roomname, char *foldername, int zapped_ok)
{
	int ret;
	char augmented_roomname[ROOMNAMELEN];
	char roomname[ROOMNAMELEN];
	int c;
	struct ctdlroom QRscratch;
	int ra;
	int ok = 0;

	ret = imap_roomname(roomname, sizeof roomname, foldername);
	if (ret < 0) {
		return (1);
	}

	/* First try a regular match */
	c = getroom(&QRscratch, roomname);

	/* Then try a mailbox name match */
	if (c != 0) {
		MailboxName(augmented_roomname, sizeof augmented_roomname,
			    &CC->user, roomname);
		c = getroom(&QRscratch, augmented_roomname);
		if (c == 0)
			strcpy(roomname, augmented_roomname);
	}

	/* If the room exists, check security/access */
	if (c == 0) {
		/* See if there is an existing user/room relationship */
		CtdlRoomAccess(&QRscratch, &CC->user, &ra, NULL);

		/* normal clients have to pass through security */
		if (ra & UA_KNOWN) {
			ok = 1;
		}
		if ((zapped_ok) && (ra & UA_ZAPPED)) {
			ok = 1;
		}
	}

	/* Fail here if no such room */
	if (!ok) {
		strcpy(returned_roomname, "");
		return (2);
	} else {
		strcpy(returned_roomname, QRscratch.QRname);
		return (0);
	}
}


/*
 * Implements the STATUS command (sort of)
 *
 */
void imap_status(int num_parms, char *parms[])
{
	int ret;
	char roomname[ROOMNAMELEN];
	char buf[SIZ];
	char savedroom[ROOMNAMELEN];
	int msgs, new;

	ret = imap_grabroom(roomname, parms[2], 0);
	if (ret != 0) {
		cprintf
		    ("%s NO Invalid mailbox name or location, or access denied\r\n",
		     parms[0]);
		return;
	}

	/*
	 * usergoto() formally takes us to the desired room, happily returning
	 * the number of messages and number of new messages.  (If another
	 * folder is selected, save its name so we can return there!!!!!)
	 */
	if (IMAP->selected) {
		strcpy(savedroom, CC->room.QRname);
	}
	usergoto(roomname, 0, 0, &msgs, &new);

	/*
	 * Tell the client what it wants to know.  In fact, tell it *more* than
	 * it wants to know.  We happily IGnore the supplied status data item
	 * names and simply spew all possible data items.  It's far easier to
	 * code and probably saves us some processing time too.
	 */
	imap_mailboxname(buf, sizeof buf, &CC->room);
	cprintf("* STATUS ");
	imap_strout(buf);
	cprintf(" (MESSAGES %d ", msgs);
	cprintf("RECENT %d ", new);	/* Initially, new==recent */
	cprintf("UIDNEXT %ld ", CitControl.MMhighest + 1);
	cprintf("UNSEEN %d)\r\n", new);

	/*
	 * If another folder is selected, go back to that room so we can resume
	 * our happy day without violent explosions.
	 */
	if (IMAP->selected) {
		usergoto(savedroom, 0, 0, &msgs, &new);
	}

	/*
	 * Oooh, look, we're done!
	 */
	cprintf("%s OK STATUS completed\r\n", parms[0]);
}



/*
 * Implements the SUBSCRIBE command
 *
 */
void imap_subscribe(int num_parms, char *parms[])
{
	int ret;
	char roomname[ROOMNAMELEN];
	char savedroom[ROOMNAMELEN];
	int msgs, new;

	ret = imap_grabroom(roomname, parms[2], 1);
	if (ret != 0) {
		cprintf(
			"%s NO Error %d: invalid mailbox name or location, or access denied\r\n",
			parms[0],
			ret
		);
		return;
	}

	/*
	 * usergoto() formally takes us to the desired room, which has the side
	 * effect of marking the room as not-zapped ... exactly the effect
	 * we're looking for.
	 */
	if (IMAP->selected) {
		strcpy(savedroom, CC->room.QRname);
	}
	usergoto(roomname, 0, 0, &msgs, &new);

	/*
	 * If another folder is selected, go back to that room so we can resume
	 * our happy day without violent explosions.
	 */
	if (IMAP->selected) {
		usergoto(savedroom, 0, 0, &msgs, &new);
	}

	cprintf("%s OK SUBSCRIBE completed\r\n", parms[0]);
}


/*
 * Implements the UNSUBSCRIBE command
 *
 */
void imap_unsubscribe(int num_parms, char *parms[])
{
	int ret;
	char roomname[ROOMNAMELEN];
	char savedroom[ROOMNAMELEN];
	int msgs, new;

	ret = imap_grabroom(roomname, parms[2], 0);
	if (ret != 0) {
		cprintf
		    ("%s NO Invalid mailbox name or location, or access denied\r\n",
		     parms[0]);
		return;
	}

	/*
	 * usergoto() formally takes us to the desired room.
	 */
	if (IMAP->selected) {
		strcpy(savedroom, CC->room.QRname);
	}
	usergoto(roomname, 0, 0, &msgs, &new);

	/* 
	 * Now make the API call to zap the room
	 */
	if (CtdlForgetThisRoom() == 0) {
		cprintf("%s OK UNSUBSCRIBE completed\r\n", parms[0]);
	} else {
		cprintf
		    ("%s NO You may not unsubscribe from this folder.\r\n",
		     parms[0]);
	}

	/*
	 * If another folder is selected, go back to that room so we can resume
	 * our happy day without violent explosions.
	 */
	if (IMAP->selected) {
		usergoto(savedroom, 0, 0, &msgs, &new);
	}
}



/*
 * Implements the DELETE command
 *
 */
void imap_delete(int num_parms, char *parms[])
{
	int ret;
	char roomname[ROOMNAMELEN];
	char savedroom[ROOMNAMELEN];
	int msgs, new;

	ret = imap_grabroom(roomname, parms[2], 1);
	if (ret != 0) {
		cprintf("%s NO Invalid mailbox name, or access denied\r\n",
			parms[0]);
		return;
	}

	/*
	 * usergoto() formally takes us to the desired room, happily returning
	 * the number of messages and number of new messages.  (If another
	 * folder is selected, save its name so we can return there!!!!!)
	 */
	if (IMAP->selected) {
		strcpy(savedroom, CC->room.QRname);
	}
	usergoto(roomname, 0, 0, &msgs, &new);

	/*
	 * Now delete the room.
	 */
	if (CtdlDoIHavePermissionToDeleteThisRoom(&CC->room)) {
		schedule_room_for_deletion(&CC->room);
		cprintf("%s OK DELETE completed\r\n", parms[0]);
	} else {
		cprintf("%s NO Can't delete this folder.\r\n", parms[0]);
	}

	/*
	 * If another folder is selected, go back to that room so we can resume
	 * our happy day without violent explosions.
	 */
	if (IMAP->selected) {
		usergoto(savedroom, 0, 0, &msgs, &new);
	}
}


/*
 * Back end function for imap_rename()
 */
void imap_rename_backend(struct ctdlroom *qrbuf, void *data)
{
	char foldername[SIZ];
	char newfoldername[SIZ];
	char newroomname[ROOMNAMELEN];
	int newfloor = 0;
	struct irl *irlp = NULL;	/* scratch pointer */
	struct irlparms *irlparms;

	irlparms = (struct irlparms *) data;
	imap_mailboxname(foldername, sizeof foldername, qrbuf);

	/* Rename subfolders */
	if ((!strncasecmp(foldername, irlparms->oldname,
			  strlen(irlparms->oldname))
	     && (foldername[strlen(irlparms->oldname)] == '/'))) {

		sprintf(newfoldername, "%s/%s",
			irlparms->newname,
			&foldername[strlen(irlparms->oldname) + 1]
		    );

		newfloor = imap_roomname(newroomname,
					 sizeof newroomname,
					 newfoldername) & 0xFF;

		irlp = (struct irl *) malloc(sizeof(struct irl));
		strcpy(irlp->irl_newroom, newroomname);
		strcpy(irlp->irl_oldroom, qrbuf->QRname);
		irlp->irl_newfloor = newfloor;
		irlp->next = *(irlparms->irl);
		*(irlparms->irl) = irlp;
	}
}


/*
 * Implements the RENAME command
 *
 */
void imap_rename(int num_parms, char *parms[])
{
	char old_room[ROOMNAMELEN];
	char new_room[ROOMNAMELEN];
	int oldr, newr;
	int new_floor;
	int r;
	struct irl *irl = NULL;	/* the list */
	struct irl *irlp = NULL;	/* scratch pointer */
	struct irlparms irlparms;

	if (strchr(parms[3], '\\') != NULL) {
		cprintf("%s NO Invalid character in folder name\r\n",
			parms[0]);
		return;
	}

	oldr = imap_roomname(old_room, sizeof old_room, parms[2]);
	newr = imap_roomname(new_room, sizeof new_room, parms[3]);
	new_floor = (newr & 0xFF);

	r = CtdlRenameRoom(old_room, new_room, new_floor);

	if (r == crr_room_not_found) {
		cprintf("%s NO Could not locate this folder\r\n",
			parms[0]);
		return;
	}
	if (r == crr_already_exists) {
		cprintf("%s '%s' already exists.\r\n", parms[0], parms[2]);
		return;
	}
	if (r == crr_noneditable) {
		cprintf("%s This folder is not editable.\r\n", parms[0]);
		return;
	}
	if (r == crr_invalid_floor) {
		cprintf("%s Folder root does not exist.\r\n", parms[0]);
		return;
	}
	if (r == crr_access_denied) {
		cprintf("%s You do not have permission to edit "
			"this folder.\r\n", parms[0]);
		return;
	}
	if (r != crr_ok) {
		cprintf("%s NO Rename failed - undefined error %d\r\n",
			parms[0], r);
		return;
	}


	/* If this is the INBOX, then RFC2060 says we have to just move the
	 * contents.  In a Citadel environment it's easier to rename the room
	 * (already did that) and create a new inbox.
	 */
	if (!strcasecmp(parms[2], "INBOX")) {
		create_room(MAILROOM, 4, "", 0, 1, 0, VIEW_MAILBOX);
	}

	/* Otherwise, do the subfolders.  Build a list of rooms to rename... */
	else {
		irlparms.oldname = parms[2];
		irlparms.newname = parms[3];
		irlparms.irl = &irl;
		ForEachRoom(imap_rename_backend, (void *) &irlparms);

		/* ... and now rename them. */
		while (irl != NULL) {
			r = CtdlRenameRoom(irl->irl_oldroom,
					   irl->irl_newroom,
					   irl->irl_newfloor);
			if (r != crr_ok) {
				/* FIXME handle error returns better */
				lprintf(CTDL_ERR, "CtdlRenameRoom() error %d\n", r);
			}
			irlp = irl;
			irl = irl->next;
			free(irlp);
		}
	}

	cprintf("%s OK RENAME completed\r\n", parms[0]);
}




/* 
 * Main command loop for IMAP sessions.
 */
void imap_command_loop(void)
{
	char cmdbuf[SIZ];
	char *parms[SIZ];
	int num_parms;
	struct timeval tv1, tv2;
	suseconds_t total_time = 0;

	gettimeofday(&tv1, NULL);
	CC->lastcmd = time(NULL);
	memset(cmdbuf, 0, sizeof cmdbuf);	/* Clear it, just in case */
	flush_output();
	if (client_getln(cmdbuf, sizeof cmdbuf) < 1) {
		lprintf(CTDL_ERR, "Client disconnected: ending session.\r\n");
		CC->kill_me = 1;
		return;
	}

	if (IMAP->authstate == imap_as_expecting_password) {
		lprintf(CTDL_INFO, "IMAP: <password>\n");
	}
	else if (bmstrcasestr(cmdbuf, " LOGIN ")) {
		lprintf(CTDL_INFO, "IMAP: LOGIN...\n");
	}
	else {
		lprintf(CTDL_INFO, "IMAP: %s\n", cmdbuf);
	}

	while (strlen(cmdbuf) < 5)
		strcat(cmdbuf, " ");

	/* strip off l/t whitespace and CRLF */
	if (cmdbuf[strlen(cmdbuf) - 1] == '\n')
		cmdbuf[strlen(cmdbuf) - 1] = 0;
	if (cmdbuf[strlen(cmdbuf) - 1] == '\r')
		cmdbuf[strlen(cmdbuf) - 1] = 0;
	striplt(cmdbuf);

	/* If we're in the middle of a multi-line command, handle that */
	if (IMAP->authstate == imap_as_expecting_username) {
		imap_auth_login_user(cmdbuf);
		return;
	}
	if (IMAP->authstate == imap_as_expecting_password) {
		imap_auth_login_pass(cmdbuf);
		return;
	}

	/* Ok, at this point we're in normal command mode.  The first thing
	 * we do is print any incoming pages (yeah! we really do!)
	 */
	imap_print_instant_messages();

	/*
	 * Before processing the command that was just entered... if we happen
	 * to have a folder selected, we'd like to rescan that folder for new
	 * messages, and for deletions/changes of existing messages.  This
	 * could probably be optimized somehow, but IMAP sucks...
	 */
	if (IMAP->selected) {
		imap_rescan_msgids();
	}

	/* Now for the command set. */

	/* Grab the tag, command, and parameters.  Check syntax. */
	num_parms = imap_parameterize(parms, cmdbuf);
	if (num_parms < 2) {
		cprintf("BAD syntax error\r\n");
	}

	/* The commands below may be executed in any state */

	else if ((!strcasecmp(parms[1], "NOOP"))
		 || (!strcasecmp(parms[1], "CHECK"))) {
		cprintf("%s OK No operation\r\n",
			parms[0]);
	}

	else if (!strcasecmp(parms[1], "ID")) {
		imap_id(num_parms, parms);
	}


	else if (!strcasecmp(parms[1], "LOGOUT")) {
		if (IMAP->selected) {
			imap_do_expunge();	/* yes, we auto-expunge */
		}
		cprintf("* BYE %s logging out\r\n", config.c_fqdn);
		cprintf("%s OK Citadel IMAP session ended.\r\n",
			parms[0]);
		CC->kill_me = 1;
		return;
	}

	else if (!strcasecmp(parms[1], "LOGIN")) {
		imap_login(num_parms, parms);
	}

	else if (!strcasecmp(parms[1], "AUTHENTICATE")) {
		imap_authenticate(num_parms, parms);
	}

	else if (!strcasecmp(parms[1], "CAPABILITY")) {
		imap_capability(num_parms, parms);
	}
#ifdef HAVE_OPENSSL
	else if (!strcasecmp(parms[1], "STARTTLS")) {
		imap_starttls(num_parms, parms);
	}
#endif
	else if (!CC->logged_in) {
		cprintf("%s BAD Not logged in.\r\n", parms[0]);
	}

	/* The commans below require a logged-in state */

	else if (!strcasecmp(parms[1], "SELECT")) {
		imap_select(num_parms, parms);
	}

	else if (!strcasecmp(parms[1], "EXAMINE")) {
		imap_select(num_parms, parms);
	}

	else if (!strcasecmp(parms[1], "LSUB")) {
		imap_lsub(num_parms, parms);
	}

	else if (!strcasecmp(parms[1], "LIST")) {
		imap_list(num_parms, parms);
	}

	else if (!strcasecmp(parms[1], "CREATE")) {
		imap_create(num_parms, parms);
	}

	else if (!strcasecmp(parms[1], "DELETE")) {
		imap_delete(num_parms, parms);
	}

	else if (!strcasecmp(parms[1], "RENAME")) {
		imap_rename(num_parms, parms);
	}

	else if (!strcasecmp(parms[1], "STATUS")) {
		imap_status(num_parms, parms);
	}

	else if (!strcasecmp(parms[1], "SUBSCRIBE")) {
		imap_subscribe(num_parms, parms);
	}

	else if (!strcasecmp(parms[1], "UNSUBSCRIBE")) {
		imap_unsubscribe(num_parms, parms);
	}

	else if (!strcasecmp(parms[1], "APPEND")) {
		imap_append(num_parms, parms);
	}

	else if (!strcasecmp(parms[1], "NAMESPACE")) {
		imap_namespace(num_parms, parms);
	}

	else if (!strcasecmp(parms[1], "SETACL")) {
		imap_setacl(num_parms, parms);
	}

	else if (!strcasecmp(parms[1], "DELETEACL")) {
		imap_deleteacl(num_parms, parms);
	}

	else if (!strcasecmp(parms[1], "GETACL")) {
		imap_getacl(num_parms, parms);
	}

	else if (!strcasecmp(parms[1], "LISTRIGHTS")) {
		imap_listrights(num_parms, parms);
	}

	else if (!strcasecmp(parms[1], "MYRIGHTS")) {
		imap_myrights(num_parms, parms);
	}

	else if (IMAP->selected == 0) {
		cprintf("%s BAD no folder selected\r\n", parms[0]);
	}

	/* The commands below require the SELECT state on a mailbox */

	else if (!strcasecmp(parms[1], "FETCH")) {
		imap_fetch(num_parms, parms);
	}

	else if ((!strcasecmp(parms[1], "UID"))
		 && (!strcasecmp(parms[2], "FETCH"))) {
		imap_uidfetch(num_parms, parms);
	}

	else if (!strcasecmp(parms[1], "SEARCH")) {
		imap_search(num_parms, parms);
	}

	else if ((!strcasecmp(parms[1], "UID"))
		 && (!strcasecmp(parms[2], "SEARCH"))) {
		imap_uidsearch(num_parms, parms);
	}

	else if (!strcasecmp(parms[1], "STORE")) {
		imap_store(num_parms, parms);
	}

	else if ((!strcasecmp(parms[1], "UID"))
		 && (!strcasecmp(parms[2], "STORE"))) {
		imap_uidstore(num_parms, parms);
	}

	else if (!strcasecmp(parms[1], "COPY")) {
		imap_copy(num_parms, parms);
	}

	else if ((!strcasecmp(parms[1], "UID"))
		 && (!strcasecmp(parms[2], "COPY"))) {
		imap_uidcopy(num_parms, parms);
	}

	else if (!strcasecmp(parms[1], "EXPUNGE")) {
		imap_expunge(num_parms, parms);
	}

	else if (!strcasecmp(parms[1], "CLOSE")) {
		imap_close(num_parms, parms);
	}

	/* End of commands.  If we get here, the command is either invalid
	 * or unimplemented.
	 */

	else {
		cprintf("%s BAD command unrecognized\r\n", parms[0]);
	}

	/* If the client transmitted a message we can free it now */
	imap_free_transmitted_message();

	gettimeofday(&tv2, NULL);
	total_time = (tv2.tv_usec + (tv2.tv_sec * 1000000)) - (tv1.tv_usec + (tv1.tv_sec * 1000000));
	lprintf(CTDL_DEBUG, "IMAP command completed in %ld.%ld seconds\n",
		(total_time / 1000000),
		(total_time % 1000000)
	);
}


/*
 * This function is called to register the IMAP extension with Citadel.
 */
char *serv_imap_init(void)
{
	CtdlRegisterServiceHook(config.c_imap_port,
				NULL, imap_greeting, imap_command_loop, NULL);
#ifdef HAVE_OPENSSL
	CtdlRegisterServiceHook(config.c_imaps_port,
				NULL, imaps_greeting, imap_command_loop, NULL);
#endif
	CtdlRegisterSessionHook(imap_cleanup_function, EVT_STOP);
	return "$Id$";
}
