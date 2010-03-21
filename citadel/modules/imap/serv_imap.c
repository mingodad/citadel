/*
 * $Id$ 
 *
 * IMAP server for the Citadel system
 * Copyright (C) 2000-2009 by Art Cancro and others.
 * This code is released under the terms of the GNU General Public License.
 *
 * WARNING: the IMAP protocol is badly designed.  No implementation of it
 * is perfect.  Indeed, with so much gratuitous complexity, *all* IMAP
 * implementations have bugs.
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
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "user_ops.h"
#include "policy.h"
#include "database.h"
#include "msgbase.h"
#include "internet_addressing.h"
#include "imap_tools.h"
#include "serv_imap.h"
#include "imap_list.h"
#include "imap_fetch.h"
#include "imap_search.h"
#include "imap_store.h"
#include "imap_acl.h"
#include "imap_metadata.h"
#include "imap_misc.h"

#include "ctdl_module.h"


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
	FreeStrBuf(&IMAP->TransmittedMessage);
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
		IMAP->msgids = realloc(IMAP->msgids, (IMAP->num_alloc * sizeof(long)) );
		IMAP->flags = realloc(IMAP->flags, (IMAP->num_alloc * sizeof(long)) );
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
		CtdlLogPrintf(CTDL_ERR,
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
		CtdlLogPrintf(CTDL_ERR, "imap_load_msgids() can't run; no room selected\n");
		return;
	}

	/*
	 * Check to see if the room's contents have changed.
	 * If not, we can avoid this rescan.
	 */
	CtdlGetRoom(&CC->room, CC->room.QRname);
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
			CtdlLogPrintf(CTDL_CRIT, "malloc() failed\n");
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
	 * This logic is probably the cause of Bug # 368
	 * [ http://bugzilla.citadel.org/show_bug.cgi?id=368 ]
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

	CtdlLogPrintf(CTDL_DEBUG, "Performing IMAP cleanup hook\n");
	imap_free_msgids();
	imap_free_transmitted_message();

	if (IMAP->cached_rfc822 != NULL) {
		FreeStrBuf(&IMAP->cached_rfc822);
		IMAP->cached_rfc822_msgnum = (-1);
		IMAP->cached_rfc822_withbody = 0;
	}

	if (IMAP->cached_body != NULL) {
		free(IMAP->cached_body);
		IMAP->cached_body = NULL;
		IMAP->cached_body_len = 0;
		IMAP->cached_bodymsgnum = (-1);
	}
	FreeStrBuf(&IMAP->Cmd.CmdBuf);
	if (IMAP->Cmd.Params != NULL) free(IMAP->Cmd.Params);
	free(IMAP);
	CtdlLogPrintf(CTDL_DEBUG, "Finished IMAP cleanup hook\n");
}


/*
 * Does the actual work of the CAPABILITY command (because we need to
 * output this stuff in other places as well)
 */
void imap_output_capability_string(void) {
	cprintf("CAPABILITY IMAP4REV1 NAMESPACE ID AUTH=PLAIN AUTH=LOGIN UIDPLUS");

#ifdef HAVE_OPENSSL
	if (!CC->redirect_ssl) cprintf(" STARTTLS");
#endif

#ifndef DISABLE_IMAP_ACL
	cprintf(" ACL");
#endif

	/* We are building a partial implementation of METADATA for the sole purpose
	 * of interoperating with the ical/vcard version of the Bynari Insight Connector.
	 * It is not a full RFC5464 implementation, but it should refuse non-Bynari
	 * metadata in a compatible and graceful way.
	 */
	cprintf(" METADATA");

	/*
	 * LIST-EXTENDED was originally going to be required by the METADATA extension.
	 * It was mercifully removed prior to the finalization of RFC5464.  We started
	 * implementing this but stopped when we learned that it would not be needed.
	 * If you uncomment this declaration you are responsible for writing a lot of new
	 * code.
	 *
	 * cprintf(" LIST-EXTENDED")
	 */
}


/*
 * implements the CAPABILITY command
 */
void imap_capability(int num_parms, ConstStr *Params)
{
	cprintf("* ");
	imap_output_capability_string();
	cprintf("\r\n");
	cprintf("%s OK CAPABILITY completed\r\n", Params[0].Key);
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
void imap_id(int num_parms, ConstStr *Params)
{
	cprintf("* ID NIL\r\n");
	cprintf("%s OK ID completed\r\n", Params[0].Key);
}


/*
 * Here's where our IMAP session begins its happy day.
 */
void imap_greeting(void)
{

	strcpy(CC->cs_clientname, "IMAP session");
	CC->session_specific_data = malloc(sizeof(citimap));
	memset(IMAP, 0, sizeof(citimap));
	IMAP->authstate = imap_as_normal;
	IMAP->cached_rfc822_msgnum = (-1);
	IMAP->cached_rfc822_withbody = 0;

	if (CC->nologin)
	{
		cprintf("* BYE; Server busy, try later\r\n");
		CC->kill_me = 1;
		return;
	}
	cprintf("* OK [");
	imap_output_capability_string();
	cprintf("] %s IMAP4rev1 %s ready\r\n", config.c_fqdn, CITADEL);
}


/*
 * IMAPS is just like IMAP, except it goes crypto right away.
 */
void imaps_greeting(void) {
	CtdlModuleStartCryptoMsgs(NULL, NULL, NULL);
#ifdef HAVE_OPENSSL
	if (!CC->redirect_ssl) CC->kill_me = 1;		/* kill session if no crypto */
#endif
	imap_greeting();
}


/*
 * implements the LOGIN command (ordinary username/password login)
 */
void imap_login(int num_parms, ConstStr *Params)
{

	switch (num_parms) {
	case 3:
		if (Params[2].Key[0] == '{') {
			cprintf("+ go ahead\r\n");
			IMAP->authstate = imap_as_expecting_multilineusername;
			strcpy(IMAP->authseq, Params[0].Key);
			return;
		}
		else {
			cprintf("%s BAD incorrect number of parameters\r\n", Params[0].Key);
			return;
		}
	case 4:
		if (CtdlLoginExistingUser(NULL, Params[2].Key) == login_ok) {
			if (CtdlTryPassword(Params[3].Key) == pass_ok) {
				cprintf("%s OK [", Params[0].Key);
				imap_output_capability_string();
				cprintf("] Hello, %s\r\n", CC->user.fullname);
				return;
			}
		}

		cprintf("%s BAD Login incorrect\r\n", Params[0].Key);
	default:
		cprintf("%s BAD incorrect number of parameters\r\n", Params[0].Key);
		return;
	}

}


/*
 * Implements the AUTHENTICATE command
 */
void imap_authenticate(int num_parms, ConstStr *Params)
{
	char UsrBuf[SIZ];

	if (num_parms != 3) {
		cprintf("%s BAD incorrect number of parameters\r\n",
			Params[0].Key);
		return;
	}

	if (CC->logged_in) {
		cprintf("%s BAD Already logged in.\r\n", Params[0].Key);
		return;
	}

	if (!strcasecmp(Params[2].Key, "LOGIN")) {
		CtdlEncodeBase64(UsrBuf, "Username:", 9, 0);
		cprintf("+ %s\r\n", UsrBuf);
		IMAP->authstate = imap_as_expecting_username;
		strcpy(IMAP->authseq, Params[0].Key);
		return;
	}

	if (!strcasecmp(Params[2].Key, "PLAIN")) {
		// CtdlEncodeBase64(UsrBuf, "Username:", 9, 0);
		// cprintf("+ %s\r\n", UsrBuf);
		cprintf("+ \r\n");
		IMAP->authstate = imap_as_expecting_plainauth;
		strcpy(IMAP->authseq, Params[0].Key);
		return;
	}

	else {
		cprintf("%s NO AUTHENTICATE %s failed\r\n",
			Params[0].Key, Params[1].Key);
	}
}


void imap_auth_plain(void)
{
	const char *decoded_authstring;
	char ident[256];
	char user[256];
	char pass[256];
	int result;

	memset(pass, 0, sizeof(pass));
	StrBufDecodeBase64(IMAP->Cmd.CmdBuf);

	decoded_authstring = ChrPtr(IMAP->Cmd.CmdBuf);
	safestrncpy(ident, decoded_authstring, sizeof ident);
	safestrncpy(user, &decoded_authstring[strlen(ident) + 1], sizeof user);
	safestrncpy(pass, &decoded_authstring[strlen(ident) + strlen(user) + 2], sizeof pass);

	IMAP->authstate = imap_as_normal;

	if (!IsEmptyStr(ident)) {
		result = CtdlLoginExistingUser(user, ident);
	}
	else {
		result = CtdlLoginExistingUser(NULL, user);
	}

	if (result == login_ok) {
		if (CtdlTryPassword(pass) == pass_ok) {
			cprintf("%s OK authentication succeeded\r\n", IMAP->authseq);
			return;
		}
	}
	cprintf("%s NO authentication failed\r\n", IMAP->authseq);
}


void imap_auth_login_user(long state)
{
	char PWBuf[SIZ];
	citimap *Imap = IMAP;

	switch (state){
	case imap_as_expecting_username:
		StrBufDecodeBase64(Imap->Cmd.CmdBuf);
		CtdlLoginExistingUser(NULL, ChrPtr(Imap->Cmd.CmdBuf));
		CtdlEncodeBase64(PWBuf, "Password:", 9, 0);
		cprintf("+ %s\r\n", PWBuf);
		
		Imap->authstate = imap_as_expecting_password;
		return;
	case imap_as_expecting_multilineusername:
		extract_token(PWBuf, ChrPtr(Imap->Cmd.CmdBuf), 1, ' ', sizeof(PWBuf));
		CtdlLoginExistingUser(NULL, ChrPtr(Imap->Cmd.CmdBuf));
		cprintf("+ go ahead\r\n");
		IMAP->authstate = imap_as_expecting_multilinepassword;
		return;
	}
}


void imap_auth_login_pass(long state)
{
	citimap *Imap = IMAP;
	const char *pass = NULL;

	switch (state) {
	default:
	case imap_as_expecting_password:
		StrBufDecodeBase64(Imap->Cmd.CmdBuf);
		pass = ChrPtr(Imap->Cmd.CmdBuf);
		break;
	case imap_as_expecting_multilinepassword:
		pass = ChrPtr(Imap->Cmd.CmdBuf);
		break;
	}
	if (CtdlTryPassword(pass) == pass_ok) {
		cprintf("%s OK authentication succeeded\r\n", IMAP->authseq);
	} else {
		cprintf("%s NO authentication failed\r\n", IMAP->authseq);
	}
	IMAP->authstate = imap_as_normal;
	return;
}


/*
 * implements the STARTTLS command (Citadel API version)
 */
void imap_starttls(int num_parms, ConstStr *Params)
{
	char ok_response[SIZ];
	char nosup_response[SIZ];
	char error_response[SIZ];

	snprintf(ok_response, SIZ,	"%s OK begin TLS negotiation now\r\n",	Params[0].Key);
	snprintf(nosup_response, SIZ,	"%s NO TLS not supported here\r\n",	Params[0].Key);
	snprintf(error_response, SIZ,	"%s BAD Internal error\r\n",		Params[0].Key);
	CtdlModuleStartCryptoMsgs(ok_response, nosup_response, error_response);
}


/*
 * implements the SELECT command
 */
void imap_select(int num_parms, ConstStr *Params)
{
	char towhere[ROOMNAMELEN];
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
	i = imap_roomname(towhere, sizeof towhere, Params[2].Key);
	if (i < 0) {
		cprintf("%s NO Invalid mailbox name.\r\n", Params[0].Key);
		IMAP->selected = 0;
		return;
	}
	floornum = (i & 0x00ff);
	roomflags = (i & 0xff00);

	/* First try a regular match */
	c = CtdlGetRoom(&QRscratch, towhere);

	/* Then try a mailbox name match */
	if (c != 0) {
		CtdlMailboxName(augmented_roomname, sizeof augmented_roomname, &CC->user, towhere);
		c = CtdlGetRoom(&QRscratch, augmented_roomname);
		if (c == 0) {
			safestrncpy(towhere, augmented_roomname, sizeof(towhere));
		}
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
		cprintf("%s NO ... no such room, or access denied\r\n", Params[0].Key);
		return;
	}

	/* If we already had some other folder selected, auto-expunge it */
	imap_do_expunge();

	/*
	 * CtdlUserGoto() formally takes us to the desired room, happily returning
	 * the number of messages and number of new messages.
	 */
	memcpy(&CC->room, &QRscratch, sizeof(struct ctdlroom));
	CtdlUserGoto(NULL, 0, 0, &msgs, &new);
	IMAP->selected = 1;

	if (!strcasecmp(Params[1].Key, "EXAMINE")) {
		IMAP->readonly = 1;
	} else {
		IMAP->readonly = 0;
	}

	imap_load_msgids();
	IMAP->last_mtime = CC->room.QRmtime;

	cprintf("* %d EXISTS\r\n", msgs);
	cprintf("* %d RECENT\r\n", new);

	cprintf("* OK [UIDVALIDITY %ld] UID validity status\r\n", GLOBAL_UIDVALIDITY_VALUE);
	cprintf("* OK [UIDNEXT %ld] Predicted next UID\r\n", CitControl.MMhighest + 1);

	/* Technically, \Deleted is a valid flag, but not a permanent flag,
	 * because we don't maintain its state across sessions.  Citadel
	 * automatically expunges mailboxes when they are de-selected.
	 * 
	 * Unfortunately, omitting \Deleted as a PERMANENTFLAGS flag causes
	 * some clients (particularly Thunderbird) to misbehave -- they simply
	 * elect not to transmit the flag at all.  So we have to advertise
	 * \Deleted as a PERMANENTFLAGS flag, even though it technically isn't.
	 */
	cprintf("* FLAGS (\\Deleted \\Seen \\Answered)\r\n");
	cprintf("* OK [PERMANENTFLAGS (\\Deleted \\Seen \\Answered)] permanent flags\r\n");

	cprintf("%s OK [%s] %s completed\r\n",
		Params[0].Key,
		(IMAP->readonly ? "READ-ONLY" : "READ-WRITE"), Params[1].Key
	);
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

	CtdlLogPrintf(CTDL_DEBUG, "imap_do_expunge() called\n");
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

	CtdlLogPrintf(CTDL_DEBUG, "Expunged %d messages from <%s>\n", num_expunged, CC->room.QRname);
	return (num_expunged);
}


/*
 * implements the EXPUNGE command syntax
 */
void imap_expunge(int num_parms, ConstStr *Params)
{
	int num_expunged = 0;

	num_expunged = imap_do_expunge();
	cprintf("%s OK expunged %d messages.\r\n", Params[0].Key, num_expunged);
}


/*
 * implements the CLOSE command
 */
void imap_close(int num_parms, ConstStr *Params)
{

	/* Yes, we always expunge on close. */
	if (IMAP->selected) {
		imap_do_expunge();
	}

	IMAP->selected = 0;
	IMAP->readonly = 0;
	imap_free_msgids();
	cprintf("%s OK CLOSE completed\r\n", Params[0].Key);
}


/*
 * Implements the NAMESPACE command.
 */
void imap_namespace(int num_parms, ConstStr *Params)
{
	int i;
	struct floor *fl;
	int floors = 0;
	char Namespace[SIZ];

	cprintf("* NAMESPACE ");

	/* All personal folders are subordinate to INBOX. */
	cprintf("((\"INBOX/\" \"/\")) ");

	/* Other users' folders ... coming soon! FIXME */
	cprintf("NIL ");

	/* Show all floors as shared namespaces.  Neato! */
	cprintf("(");
	for (i = 0; i < MAXFLOORS; ++i) {
		fl = CtdlGetCachedFloor(i);
		if (fl->f_flags & F_INUSE) {
			if (floors > 0) cprintf(" ");
			cprintf("(");
			snprintf(Namespace, sizeof(Namespace), "%s/", fl->f_name);
			plain_imap_strout(Namespace);
			cprintf(" \"/\")");
			++floors;
		}
	}
	cprintf(")");

	/* Wind it up with a newline and a completion message. */
	cprintf("\r\n");
	cprintf("%s OK NAMESPACE completed\r\n", Params[0].Key);
}


/*
 * Implements the CREATE command
 *
 */
void imap_create(int num_parms, ConstStr *Params)
{
	int ret;
	char roomname[ROOMNAMELEN];
	int floornum;
	int flags;
	int newroomtype = 0;
	int newroomview = 0;
	char *notification_message = NULL;

	if (num_parms < 3) {
		cprintf("%s NO A foder name must be specified\r\n", Params[0].Key);
		return;
	}

	if (strchr(Params[2].Key, '\\') != NULL) {
		cprintf("%s NO Invalid character in folder name\r\n", Params[0].Key);
		CtdlLogPrintf(CTDL_DEBUG, "invalid character in folder name\n");
		return;
	}

	ret = imap_roomname(roomname, sizeof roomname, Params[2].Key);
	if (ret < 0) {
		cprintf("%s NO Invalid mailbox name or location\r\n",
			Params[0].Key);
		CtdlLogPrintf(CTDL_DEBUG, "invalid mailbox name or location\n");
		return;
	}
	floornum = (ret & 0x00ff);	/* lower 8 bits = floor number */
	flags = (ret & 0xff00);	/* upper 8 bits = flags        */

	if (flags & IR_MAILBOX) {
		if (strncasecmp(Params[2].Key, "INBOX/", 6)) {
			cprintf("%s NO Personal folders must be created under INBOX\r\n", Params[0].Key);
			CtdlLogPrintf(CTDL_DEBUG, "not subordinate to inbox\n");
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

	CtdlLogPrintf(CTDL_INFO, "Create new room <%s> on floor <%d> with type <%d>\n",
		roomname, floornum, newroomtype);

	ret = CtdlCreateRoom(roomname, newroomtype, "", floornum, 1, 0, newroomview);
	if (ret == 0) {
		/*** DO NOT CHANGE THIS ERROR MESSAGE IN ANY WAY!  BYNARI CONNECTOR DEPENDS ON IT! ***/
		cprintf("%s NO Mailbox already exists, or create failed\r\n", Params[0].Key);
	} else {
		cprintf("%s OK CREATE completed\r\n", Params[0].Key);
		/* post a message in Aide> describing the new room */
		notification_message = malloc(1024);
		snprintf(notification_message, 1024,
			"A new room called \"%s\" has been created by %s%s%s%s\n",
			roomname,
			CC->user.fullname,
			((ret & QR_MAILBOX) ? " [personal]" : ""),
			((ret & QR_PRIVATE) ? " [private]" : ""),
			((ret & QR_GUESSNAME) ? " [hidden]" : "")
		);
		CtdlAideMessage(notification_message, "Room Creation Message");
		free(notification_message);
	}
	CtdlLogPrintf(CTDL_DEBUG, "imap_create() completed\n");
}


/*
 * Locate a room by its IMAP folder name, and check access to it.
 * If zapped_ok is nonzero, we can also look for the room in the zapped list.
 */
int imap_grabroom(char *returned_roomname, const char *foldername, int zapped_ok)
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
	c = CtdlGetRoom(&QRscratch, roomname);

	/* Then try a mailbox name match */
	if (c != 0) {
		CtdlMailboxName(augmented_roomname, sizeof augmented_roomname,
			    &CC->user, roomname);
		c = CtdlGetRoom(&QRscratch, augmented_roomname);
		if (c == 0)
			safestrncpy(roomname, augmented_roomname, sizeof(roomname));
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
		safestrncpy(returned_roomname, QRscratch.QRname, ROOMNAMELEN);
		return (0);
	}
}


/*
 * Implements the STATUS command (sort of)
 *
 */
void imap_status(int num_parms, ConstStr *Params)
{
	int ret;
	char roomname[ROOMNAMELEN];
	char imaproomname[SIZ];
	char savedroom[ROOMNAMELEN];
	int msgs, new;

	ret = imap_grabroom(roomname, Params[2].Key, 1);
	if (ret != 0) {
		cprintf
		    ("%s NO Invalid mailbox name or location, or access denied\r\n",
		     Params[0].Key);
		return;
	}

	/*
	 * CtdlUserGoto() formally takes us to the desired room, happily returning
	 * the number of messages and number of new messages.  (If another
	 * folder is selected, save its name so we can return there!!!!!)
	 */
	if (IMAP->selected) {
		strcpy(savedroom, CC->room.QRname);
	}
	CtdlUserGoto(roomname, 0, 0, &msgs, &new);

	/*
	 * Tell the client what it wants to know.  In fact, tell it *more* than
	 * it wants to know.  We happily IGnore the supplied status data item
	 * names and simply spew all possible data items.  It's far easier to
	 * code and probably saves us some processing time too.
	 */
	imap_mailboxname(imaproomname, sizeof imaproomname, &CC->room);
	cprintf("* STATUS ");
	plain_imap_strout(imaproomname);
	cprintf(" (MESSAGES %d ", msgs);
	cprintf("RECENT %d ", new);	/* Initially, new==recent */
	cprintf("UIDNEXT %ld ", CitControl.MMhighest + 1);
	cprintf("UNSEEN %d)\r\n", new);

	/*
	 * If another folder is selected, go back to that room so we can resume
	 * our happy day without violent explosions.
	 */
	if (IMAP->selected) {
		CtdlUserGoto(savedroom, 0, 0, &msgs, &new);
	}

	/*
	 * Oooh, look, we're done!
	 */
	cprintf("%s OK STATUS completed\r\n", Params[0].Key);
}


/*
 * Implements the SUBSCRIBE command
 *
 */
void imap_subscribe(int num_parms, ConstStr *Params)
{
	int ret;
	char roomname[ROOMNAMELEN];
	char savedroom[ROOMNAMELEN];
	int msgs, new;

	ret = imap_grabroom(roomname, Params[2].Key, 1);
	if (ret != 0) {
		cprintf(
			"%s NO Error %d: invalid mailbox name or location, or access denied\r\n",
			Params[0].Key,
			ret
		);
		return;
	}

	/*
	 * CtdlUserGoto() formally takes us to the desired room, which has the side
	 * effect of marking the room as not-zapped ... exactly the effect
	 * we're looking for.
	 */
	if (IMAP->selected) {
		strcpy(savedroom, CC->room.QRname);
	}
	CtdlUserGoto(roomname, 0, 0, &msgs, &new);

	/*
	 * If another folder is selected, go back to that room so we can resume
	 * our happy day without violent explosions.
	 */
	if (IMAP->selected) {
		CtdlUserGoto(savedroom, 0, 0, &msgs, &new);
	}

	cprintf("%s OK SUBSCRIBE completed\r\n", Params[0].Key);
}


/*
 * Implements the UNSUBSCRIBE command
 *
 */
void imap_unsubscribe(int num_parms, ConstStr *Params)
{
	int ret;
	char roomname[ROOMNAMELEN];
	char savedroom[ROOMNAMELEN];
	int msgs, new;

	ret = imap_grabroom(roomname, Params[2].Key, 1);
	if (ret != 0) {
		cprintf
		    ("%s NO Invalid mailbox name or location, or access denied\r\n",
		     Params[0].Key);
		return;
	}

	/*
	 * CtdlUserGoto() formally takes us to the desired room.
	 */
	if (IMAP->selected) {
		strcpy(savedroom, CC->room.QRname);
	}
	CtdlUserGoto(roomname, 0, 0, &msgs, &new);

	/* 
	 * Now make the API call to zap the room
	 */
	if (CtdlForgetThisRoom() == 0) {
		cprintf("%s OK UNSUBSCRIBE completed\r\n", Params[0].Key);
	} else {
		cprintf
		    ("%s NO You may not unsubscribe from this folder.\r\n",
		     Params[0].Key);
	}

	/*
	 * If another folder is selected, go back to that room so we can resume
	 * our happy day without violent explosions.
	 */
	if (IMAP->selected) {
		CtdlUserGoto(savedroom, 0, 0, &msgs, &new);
	}
}


/*
 * Implements the DELETE command
 *
 */
void imap_delete(int num_parms, ConstStr *Params)
{
	int ret;
	char roomname[ROOMNAMELEN];
	char savedroom[ROOMNAMELEN];
	int msgs, new;

	ret = imap_grabroom(roomname, Params[2].Key, 1);
	if (ret != 0) {
		cprintf("%s NO Invalid mailbox name, or access denied\r\n",
			Params[0].Key);
		return;
	}

	/*
	 * CtdlUserGoto() formally takes us to the desired room, happily returning
	 * the number of messages and number of new messages.  (If another
	 * folder is selected, save its name so we can return there!!!!!)
	 */
	if (IMAP->selected) {
		strcpy(savedroom, CC->room.QRname);
	}
	CtdlUserGoto(roomname, 0, 0, &msgs, &new);

	/*
	 * Now delete the room.
	 */
	if (CtdlDoIHavePermissionToDeleteThisRoom(&CC->room)) {
		CtdlScheduleRoomForDeletion(&CC->room);
		cprintf("%s OK DELETE completed\r\n", Params[0].Key);
	} else {
		cprintf("%s NO Can't delete this folder.\r\n", Params[0].Key);
	}

	/*
	 * If another folder is selected, go back to that room so we can resume
	 * our happy day without violent explosions.
	 */
	if (IMAP->selected) {
		CtdlUserGoto(savedroom, 0, 0, &msgs, &new);
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
void imap_rename(int num_parms, ConstStr *Params)
{
	char old_room[ROOMNAMELEN];
	char new_room[ROOMNAMELEN];
	int oldr, newr;
	int new_floor;
	int r;
	struct irl *irl = NULL;	/* the list */
	struct irl *irlp = NULL;	/* scratch pointer */
	struct irlparms irlparms;
	char aidemsg[1024];

	if (strchr(Params[3].Key, '\\') != NULL) {
		cprintf("%s NO Invalid character in folder name\r\n",
			Params[0].Key);
		return;
	}

	oldr = imap_roomname(old_room, sizeof old_room, Params[2].Key);
	newr = imap_roomname(new_room, sizeof new_room, Params[3].Key);
	new_floor = (newr & 0xFF);

	r = CtdlRenameRoom(old_room, new_room, new_floor);

	if (r == crr_room_not_found) {
		cprintf("%s NO Could not locate this folder\r\n",
			Params[0].Key);
		return;
	}
	if (r == crr_already_exists) {
		cprintf("%s NO '%s' already exists.\r\n", Params[0].Key, Params[2].Key);
		return;
	}
	if (r == crr_noneditable) {
		cprintf("%s NO This folder is not editable.\r\n", Params[0].Key);
		return;
	}
	if (r == crr_invalid_floor) {
		cprintf("%s NO Folder root does not exist.\r\n", Params[0].Key);
		return;
	}
	if (r == crr_access_denied) {
		cprintf("%s NO You do not have permission to edit this folder.\r\n",
			Params[0].Key);
		return;
	}
	if (r != crr_ok) {
		cprintf("%s NO Rename failed - undefined error %d\r\n",
			Params[0].Key, r);
		return;
	}

	/* If this is the INBOX, then RFC2060 says we have to just move the
	 * contents.  In a Citadel environment it's easier to rename the room
	 * (already did that) and create a new inbox.
	 */
	if (!strcasecmp(Params[2].Key, "INBOX")) {
		CtdlCreateRoom(MAILROOM, 4, "", 0, 1, 0, VIEW_MAILBOX);
	}

	/* Otherwise, do the subfolders.  Build a list of rooms to rename... */
	else {
		irlparms.oldname = Params[2].Key;
		irlparms.newname = Params[3].Key;
		irlparms.irl = &irl;
		CtdlForEachRoom(imap_rename_backend, (void *) &irlparms);

		/* ... and now rename them. */
		while (irl != NULL) {
			r = CtdlRenameRoom(irl->irl_oldroom,
					   irl->irl_newroom,
					   irl->irl_newfloor);
			if (r != crr_ok) {
				/* FIXME handle error returns better */
				CtdlLogPrintf(CTDL_ERR, "CtdlRenameRoom() error %d\n", r);
			}
			irlp = irl;
			irl = irl->next;
			free(irlp);
		}
	}

	snprintf(aidemsg, sizeof aidemsg, "IMAP folder \"%s\" renamed to \"%s\" by %s\n",
		Params[2].Key,
		Params[3].Key,
		CC->curr_user
	);
	CtdlAideMessage(aidemsg, "IMAP folder rename");

	cprintf("%s OK RENAME completed\r\n", Params[0].Key);
}


/* 
 * Main command loop for IMAP sessions.
 */
void imap_command_loop(void)
{
	struct timeval tv1, tv2;
	suseconds_t total_time = 0;
	int untagged_ok = 1;
	citimap *Imap;
	const char *pchs, *pche;

	gettimeofday(&tv1, NULL);
	CC->lastcmd = time(NULL);
	Imap = IMAP;

	flush_output();
	if (Imap->Cmd.CmdBuf == NULL)
		Imap->Cmd.CmdBuf = NewStrBufPlain(NULL, SIZ);
	else
		FlushStrBuf(Imap->Cmd.CmdBuf);

	if (CtdlClientGetLine(Imap->Cmd.CmdBuf) < 1) {
		CtdlLogPrintf(CTDL_ERR, "Client disconnected: ending session.\r\n");
		CC->kill_me = 1;
		return;
	}

	if (Imap->authstate == imap_as_expecting_password) {
		CtdlLogPrintf(CTDL_INFO, "IMAP: <password>\n");
	}
	else if (Imap->authstate == imap_as_expecting_plainauth) {
		CtdlLogPrintf(CTDL_INFO, "IMAP: <plain_auth>\n");
	}
	else if ((Imap->authstate == imap_as_expecting_multilineusername) || 
		 bmstrcasestr(ChrPtr(Imap->Cmd.CmdBuf), " LOGIN ")) {
		CtdlLogPrintf(CTDL_INFO, "IMAP: LOGIN...\n");
	}
	else {
		CtdlLogPrintf(CTDL_INFO, "IMAP: %s\n", ChrPtr(Imap->Cmd.CmdBuf));
	}

	pchs = ChrPtr(Imap->Cmd.CmdBuf);
	pche = pchs + StrLength(Imap->Cmd.CmdBuf);

	while ((pche > pchs) &&
	       ((*pche == '\n') ||
		(*pche == '\r')))
	{
		pche --;
		StrBufCutRight(Imap->Cmd.CmdBuf, 1);
	}
	StrBufTrim(Imap->Cmd.CmdBuf);

	/* If we're in the middle of a multi-line command, handle that */
	switch (Imap->authstate){
	case imap_as_expecting_username:
		imap_auth_login_user(imap_as_expecting_username);
		return;
	case imap_as_expecting_multilineusername:
		imap_auth_login_user(imap_as_expecting_multilineusername);
		return;
	case imap_as_expecting_plainauth:
		imap_auth_plain();
		return;
	case imap_as_expecting_password:
		imap_auth_login_pass(imap_as_expecting_password);
		return;
	case imap_as_expecting_multilinepassword:
		imap_auth_login_pass(imap_as_expecting_multilinepassword);
		return;
	default:
		break;
	}


	/* Ok, at this point we're in normal command mode.
	 * If the command just submitted does not contain a literal, we
	 * might think about delivering some untagged stuff...
	 */
	if (*(ChrPtr(Imap->Cmd.CmdBuf) + StrLength(Imap->Cmd.CmdBuf) - 1)
	    == '}') {
		untagged_ok = 0;
	}

	/* Grab the tag, command, and parameters. */
	imap_parameterize(&Imap->Cmd);
#if 0 
/* debug output the parsed vector */
	{
		int i;
		CtdlLogPrintf(CTDL_DEBUG, "----- %ld params \n",
			      Imap->Cmd.num_parms);

	for (i=0; i < Imap->Cmd.num_parms; i++) {
		if (Imap->Cmd.Params[i].len != strlen(Imap->Cmd.Params[i].Key))
			CtdlLogPrintf(CTDL_DEBUG, "*********** %ld != %ld : %s\n",
				      Imap->Cmd.Params[i].len, 
				      strlen(Imap->Cmd.Params[i].Key),
				      Imap->Cmd.Params[i].Key);
		else
			CtdlLogPrintf(CTDL_DEBUG, "%ld : %s\n",
				      Imap->Cmd.Params[i].len, 
				      Imap->Cmd.Params[i].Key);
	}}

#endif
	/* RFC3501 says that we cannot output untagged data during these commands */
	if (Imap->Cmd.num_parms >= 2) {
		if (  (!strcasecmp(Imap->Cmd.Params[1].Key, "FETCH"))
		   || (!strcasecmp(Imap->Cmd.Params[1].Key, "STORE"))
		   || (!strcasecmp(Imap->Cmd.Params[1].Key, "SEARCH"))
		) {
			untagged_ok = 0;
		}
	}
	
	if (untagged_ok) {

		/* we can put any additional untagged stuff right here in the future */

		/*
		 * Before processing the command that was just entered... if we happen
		 * to have a folder selected, we'd like to rescan that folder for new
		 * messages, and for deletions/changes of existing messages.  This
		 * could probably be optimized better with some deep thought...
		 */
		if (Imap->selected) {
			imap_rescan_msgids();
		}
	}

	/* Now for the command set. */

	if (Imap->Cmd.num_parms < 2) {
		cprintf("BAD syntax error\r\n");
	}

	/* The commands below may be executed in any state */

	else if ((!strcasecmp(Imap->Cmd.Params[1].Key, "NOOP"))
		 || (!strcasecmp(Imap->Cmd.Params[1].Key, "CHECK"))) {
		cprintf("%s OK No operation\r\n",
			Imap->Cmd.Params[0].Key);
	}

	else if (!strcasecmp(Imap->Cmd.Params[1].Key, "ID")) {
		imap_id(Imap->Cmd.num_parms, Imap->Cmd.Params);
	}


	else if (!strcasecmp(Imap->Cmd.Params[1].Key, "LOGOUT")) {
		if (Imap->selected) {
			imap_do_expunge();	/* yes, we auto-expunge at logout */
		}
		cprintf("* BYE %s logging out\r\n", config.c_fqdn);
		cprintf("%s OK Citadel IMAP session ended.\r\n",
			Imap->Cmd.Params[0].Key);
		CC->kill_me = 1;
		return;
	}

	else if (!strcasecmp(Imap->Cmd.Params[1].Key, "LOGIN")) {
		imap_login(Imap->Cmd.num_parms, Imap->Cmd.Params);
	}

	else if (!strcasecmp(Imap->Cmd.Params[1].Key, "AUTHENTICATE")) {
		imap_authenticate(Imap->Cmd.num_parms, Imap->Cmd.Params);
	}

	else if (!strcasecmp(Imap->Cmd.Params[1].Key, "CAPABILITY")) {
		imap_capability(Imap->Cmd.num_parms, Imap->Cmd.Params);
	}
#ifdef HAVE_OPENSSL
	else if (!strcasecmp(Imap->Cmd.Params[1].Key, "STARTTLS")) {
		imap_starttls(Imap->Cmd.num_parms, Imap->Cmd.Params);
	}
#endif
	else if (!CC->logged_in) {
		cprintf("%s BAD Not logged in.\r\n", Imap->Cmd.Params[0].Key);
	}

	/* The commans below require a logged-in state */

	else if (!strcasecmp(Imap->Cmd.Params[1].Key, "SELECT")) {
		imap_select(Imap->Cmd.num_parms, Imap->Cmd.Params);
	}

	else if (!strcasecmp(Imap->Cmd.Params[1].Key, "EXAMINE")) {
		imap_select(Imap->Cmd.num_parms, Imap->Cmd.Params);
	}

	else if (!strcasecmp(Imap->Cmd.Params[1].Key, "LSUB")) {
		imap_list(Imap->Cmd.num_parms, Imap->Cmd.Params);
	}

	else if (!strcasecmp(Imap->Cmd.Params[1].Key, "LIST")) {
		imap_list(Imap->Cmd.num_parms, Imap->Cmd.Params);
	}

	else if (!strcasecmp(Imap->Cmd.Params[1].Key, "CREATE")) {
		imap_create(Imap->Cmd.num_parms, Imap->Cmd.Params);
	}

	else if (!strcasecmp(Imap->Cmd.Params[1].Key, "DELETE")) {
		imap_delete(Imap->Cmd.num_parms, Imap->Cmd.Params);
	}

	else if (!strcasecmp(Imap->Cmd.Params[1].Key, "RENAME")) {
		imap_rename(Imap->Cmd.num_parms, Imap->Cmd.Params);
	}

	else if (!strcasecmp(Imap->Cmd.Params[1].Key, "STATUS")) {
		imap_status(Imap->Cmd.num_parms, Imap->Cmd.Params);
	}

	else if (!strcasecmp(Imap->Cmd.Params[1].Key, "SUBSCRIBE")) {
		imap_subscribe(Imap->Cmd.num_parms, Imap->Cmd.Params);
	}

	else if (!strcasecmp(Imap->Cmd.Params[1].Key, "UNSUBSCRIBE")) {
		imap_unsubscribe(Imap->Cmd.num_parms, Imap->Cmd.Params);
	}

	else if (!strcasecmp(Imap->Cmd.Params[1].Key, "APPEND")) {
		imap_append(Imap->Cmd.num_parms, Imap->Cmd.Params);
	}

	else if (!strcasecmp(Imap->Cmd.Params[1].Key, "NAMESPACE")) {
		imap_namespace(Imap->Cmd.num_parms, Imap->Cmd.Params);
	}

	else if (!strcasecmp(Imap->Cmd.Params[1].Key, "SETACL")) {
		imap_setacl(Imap->Cmd.num_parms, Imap->Cmd.Params);
	}

	else if (!strcasecmp(Imap->Cmd.Params[1].Key, "DELETEACL")) {
		imap_deleteacl(Imap->Cmd.num_parms, Imap->Cmd.Params);
	}

	else if (!strcasecmp(Imap->Cmd.Params[1].Key, "GETACL")) {
		imap_getacl(Imap->Cmd.num_parms, Imap->Cmd.Params);
	}

	else if (!strcasecmp(Imap->Cmd.Params[1].Key, "LISTRIGHTS")) {
		imap_listrights(Imap->Cmd.num_parms, Imap->Cmd.Params);
	}

	else if (!strcasecmp(Imap->Cmd.Params[1].Key, "MYRIGHTS")) {
		imap_myrights(Imap->Cmd.num_parms, Imap->Cmd.Params);
	}

	else if (!strcasecmp(Imap->Cmd.Params[1].Key, "GETMETADATA")) {
		imap_getmetadata(Imap->Cmd.num_parms, Imap->Cmd.Params);
	}

	else if (!strcasecmp(Imap->Cmd.Params[1].Key, "SETMETADATA")) {
		imap_setmetadata(Imap->Cmd.num_parms, Imap->Cmd.Params);
	}

	else if (Imap->selected == 0) {
		cprintf("%s BAD no folder selected\r\n", Imap->Cmd.Params[0].Key);
	}

	/* The commands below require the SELECT state on a mailbox */

	else if (!strcasecmp(Imap->Cmd.Params[1].Key, "FETCH")) {
		imap_fetch(Imap->Cmd.num_parms, Imap->Cmd.Params);
	}

	else if ((!strcasecmp(Imap->Cmd.Params[1].Key, "UID"))
		 && (!strcasecmp(Imap->Cmd.Params[2].Key, "FETCH"))) {
		imap_uidfetch(Imap->Cmd.num_parms, Imap->Cmd.Params);
	}

	else if (!strcasecmp(Imap->Cmd.Params[1].Key, "SEARCH")) {
		imap_search(Imap->Cmd.num_parms, Imap->Cmd.Params);
	}

	else if ((!strcasecmp(Imap->Cmd.Params[1].Key, "UID"))
		 && (!strcasecmp(Imap->Cmd.Params[2].Key, "SEARCH"))) {
		imap_uidsearch(Imap->Cmd.num_parms, Imap->Cmd.Params);
	}

	else if (!strcasecmp(Imap->Cmd.Params[1].Key, "STORE")) {
		imap_store(Imap->Cmd.num_parms, Imap->Cmd.Params);
	}

	else if ((!strcasecmp(Imap->Cmd.Params[1].Key, "UID"))
		 && (!strcasecmp(Imap->Cmd.Params[2].Key, "STORE"))) {
		imap_uidstore(Imap->Cmd.num_parms, Imap->Cmd.Params);
	}

	else if (!strcasecmp(Imap->Cmd.Params[1].Key, "COPY")) {
		imap_copy(Imap->Cmd.num_parms, Imap->Cmd.Params);
	}

	else if ((!strcasecmp(Imap->Cmd.Params[1].Key, "UID")) && (!strcasecmp(Imap->Cmd.Params[2].Key, "COPY"))) {
		imap_uidcopy(Imap->Cmd.num_parms, Imap->Cmd.Params);
	}

	else if (!strcasecmp(Imap->Cmd.Params[1].Key, "EXPUNGE")) {
		imap_expunge(Imap->Cmd.num_parms, Imap->Cmd.Params);
	}

	else if ((!strcasecmp(Imap->Cmd.Params[1].Key, "UID")) && (!strcasecmp(Imap->Cmd.Params[2].Key, "EXPUNGE"))) {
		imap_expunge(Imap->Cmd.num_parms, Imap->Cmd.Params);
	}

	else if (!strcasecmp(Imap->Cmd.Params[1].Key, "CLOSE")) {
		imap_close(Imap->Cmd.num_parms, Imap->Cmd.Params);
	}

	/* End of commands.  If we get here, the command is either invalid
	 * or unimplemented.
	 */

	else {
		cprintf("%s BAD command unrecognized\r\n", Imap->Cmd.Params[0].Key);
	}

	/* If the client transmitted a message we can free it now */
	imap_free_transmitted_message();

	gettimeofday(&tv2, NULL);
	total_time = (tv2.tv_usec + (tv2.tv_sec * 1000000)) - (tv1.tv_usec + (tv1.tv_sec * 1000000));
	CtdlLogPrintf(CTDL_DEBUG, "IMAP command completed in %ld.%ld seconds\n",
		(total_time / 1000000),
		(total_time % 1000000)
	);
}


const char *CitadelServiceIMAP="IMAP";
const char *CitadelServiceIMAPS="IMAPS";

/*
 * This function is called to register the IMAP extension with Citadel.
 */
CTDL_MODULE_INIT(imap)
{
	if (!threading)
	{
		CtdlRegisterServiceHook(config.c_imap_port,
					NULL, imap_greeting, imap_command_loop, NULL, CitadelServiceIMAP);
#ifdef HAVE_OPENSSL
		CtdlRegisterServiceHook(config.c_imaps_port,
					NULL, imaps_greeting, imap_command_loop, NULL, CitadelServiceIMAPS);
#endif
		CtdlRegisterSessionHook(imap_cleanup_function, EVT_STOP);
	}
	
	/* return our Subversion id for the Log */
	return "$Id$";
}
