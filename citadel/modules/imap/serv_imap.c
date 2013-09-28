/*
 * IMAP server for the Citadel system
 *
 * Copyright (C) 2000-2011 by Art Cancro and others.
 * This code is released under the terms of the GNU General Public License.
 *
 * WARNING: the IMAP protocol is badly designed.  No implementation of it
 * is perfect.  Indeed, with so much gratuitous complexity, *all* IMAP
 * implementations have bugs.
 *
 * This program is open source software; you can redistribute it and/or modify
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
#include "database.h"
#include "msgbase.h"
#include "internet_addressing.h"
#include "serv_imap.h"
#include "imap_tools.h"
#include "imap_list.h"
#include "imap_fetch.h"
#include "imap_search.h"
#include "imap_store.h"
#include "imap_acl.h"
#include "imap_metadata.h"
#include "imap_misc.h"

#include "ctdl_module.h"
int IMAPDebugEnabled = 0;
HashList *ImapCmds = NULL;
void registerImapCMD(const char *First, long FLen, 
		     const char *Second, long SLen,
		     imap_handler H,
		     int Flags)
{
	imap_handler_hook *h;

	h = (imap_handler_hook*) malloc(sizeof(imap_handler_hook));
	memset(h, 0, sizeof(imap_handler_hook));

	h->Flags = Flags;
	h->h = H;
	if (SLen == 0) {
		Put(ImapCmds, First, FLen, h, NULL);
	}
	else {
		char CMD[SIZ];
		memcpy(CMD, First, FLen);
		memcpy(CMD+FLen, Second, SLen);
		CMD[FLen+SLen] = '\0';
		Put(ImapCmds, CMD, FLen + SLen, h, NULL);
	}
}

void imap_cleanup(void)
{
	DeleteHash(&ImapCmds);
}

const imap_handler_hook *imap_lookup(int num_parms, ConstStr *Params)
{
	struct CitContext *CCC = CC;
	void *v;
	citimap *Imap = CCCIMAP;

	if (num_parms < 1)
		return NULL;

	/* we abuse the Reply-buffer for uppercasing... */
	StrBufPlain(Imap->Reply, CKEY(Params[1]));
	StrBufUpCase(Imap->Reply);

	IMAP_syslog(LOG_DEBUG, "---- Looking up [%s] -----", 
		      ChrPtr(Imap->Reply));
	if (GetHash(ImapCmds, SKEY(Imap->Reply), &v))
	{
		IMAPM_syslog(LOG_DEBUG, "Found."); 
		FlushStrBuf(Imap->Reply);
		return (imap_handler_hook *) v;
	}

	if (num_parms == 1)
	{
		IMAPM_syslog(LOG_DEBUG, "NOT Found."); 
		FlushStrBuf(Imap->Reply);
		return NULL;
	}
	
	IMAP_syslog(LOG_DEBUG, "---- Looking up [%s] -----", 
		      ChrPtr(Imap->Reply));
	StrBufAppendBufPlain(Imap->Reply, CKEY(Params[2]), 0);
	StrBufUpCase(Imap->Reply);
	if (GetHash(ImapCmds, SKEY(Imap->Reply), &v))
	{
		IMAPM_syslog(LOG_DEBUG, "Found."); 
		FlushStrBuf(Imap->Reply);
		return (imap_handler_hook *) v;
	}
	IMAPM_syslog(LOG_DEBUG, "NOT Found."); 
	FlushStrBuf(Imap->Reply);
       	return NULL;
}

/* imap_rename() uses this struct containing list of rooms to rename */
struct irl {
	struct irl *next;
	char irl_oldroom[ROOMNAMELEN];
	char irl_newroom[ROOMNAMELEN];
	int irl_newfloor;
};

/* Data which is passed between imap_rename() and imap_rename_backend() */
typedef struct __irlparms {
	const char *oldname;
	long oldnamelen;
	const char *newname;
	long newnamelen;
	struct irl **irl;
}irlparms;


/*
 * If there is a message ID map in memory, free it
 */
void imap_free_msgids(void)
{
	citimap *Imap = IMAP;
	if (Imap->msgids != NULL) {
		free(Imap->msgids);
		Imap->msgids = NULL;
		Imap->num_msgs = 0;
		Imap->num_alloc = 0;
	}
	if (Imap->flags != NULL) {
		free(Imap->flags);
		Imap->flags = NULL;
	}
	Imap->last_mtime = (-1);
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
	citimap *Imap = IMAP;
	visit vbuf;
	int i;
	int num_sets;
	int s;
	char setstr[64], lostr[64], histr[64];
	long lo, hi;

	if (Imap->num_msgs < 1) return;
	CtdlGetRelationship(&vbuf, &CC->user, &CC->room);

	for (i = first_msg; i < Imap->num_msgs; ++i) {
		Imap->flags[i] = Imap->flags[i] & ~IMAP_SEEN;
		Imap->flags[i] |= IMAP_RECENT;
		Imap->flags[i] = Imap->flags[i] & ~IMAP_ANSWERED;
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

		for (i = first_msg; i < Imap->num_msgs; ++i) {
			if ((Imap->msgids[i] >= lo) && (Imap->msgids[i] <= hi)){
				Imap->flags[i] |= IMAP_SEEN;
				Imap->flags[i] = Imap->flags[i] & ~IMAP_RECENT;
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

		for (i = first_msg; i < Imap->num_msgs; ++i) {
			if ((Imap->msgids[i] >= lo) && (Imap->msgids[i] <= hi)){
				Imap->flags[i] |= IMAP_ANSWERED;
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
	citimap *Imap = IMAP;

	++Imap->num_msgs;
	if (Imap->num_msgs > Imap->num_alloc) {
		Imap->num_alloc += REALLOC_INCREMENT;
		Imap->msgids = realloc(Imap->msgids, (Imap->num_alloc * sizeof(long)) );
		Imap->flags = realloc(Imap->flags, (Imap->num_alloc * sizeof(unsigned int *)) );
	}
	Imap->msgids[Imap->num_msgs - 1] = msgnum;
	Imap->flags[Imap->num_msgs - 1] = 0;
}



/*
 * Set up a message ID map for the current room (folder)
 */
void imap_load_msgids(void)
{
	struct CitContext *CCC = CC;
	struct cdbdata *cdbfr;
	citimap *Imap = CCCIMAP;

	if (Imap->selected == 0) {
		IMAPM_syslog(LOG_ERR, "imap_load_msgids() can't run; no room selected");
		return;
	}

	imap_free_msgids();	/* If there was already a map, free it */

	/* Load the message list */
	cdbfr = cdb_fetch(CDB_MSGLISTS, &CC->room.QRnumber, sizeof(long));
	if (cdbfr != NULL) {
		Imap->msgids = malloc(cdbfr->len);
		memcpy(Imap->msgids, cdbfr->ptr, cdbfr->len);
		Imap->num_msgs = cdbfr->len / sizeof(long);
		Imap->num_alloc = cdbfr->len / sizeof(long);
		cdb_free(cdbfr);
	}

	if (Imap->num_msgs) {
		Imap->flags = malloc(Imap->num_alloc * sizeof(unsigned int *));
		memset(Imap->flags, 0, (Imap->num_alloc * sizeof(long)) );
	}

	imap_set_seen_flags(0);
}


/*
 * Re-scan the selected room (folder) and see if it's been changed at all
 */
void imap_rescan_msgids(void)
{
	struct CitContext *CCC = CC;
	citimap *Imap = CCCIMAP;
	int original_num_msgs = 0;
	long original_highest = 0L;
	int i, j, jstart;
	int message_still_exists;
	struct cdbdata *cdbfr;
	long *msglist = NULL;
	int num_msgs = 0;
	int num_recent = 0;

	if (Imap->selected == 0) {
		IMAPM_syslog(LOG_ERR, "imap_load_msgids() can't run; no room selected");
		return;
	}

	/*
	 * Check to see if the room's contents have changed.
	 * If not, we can avoid this rescan.
	 */
	CtdlGetRoom(&CC->room, CC->room.QRname);
	if (Imap->last_mtime == CC->room.QRmtime) {	/* No changes! */
		return;
	}

	/* Load the *current* message list from disk, so we can compare it
	 * to what we have in memory.
	 */
	cdbfr = cdb_fetch(CDB_MSGLISTS, &CC->room.QRnumber, sizeof(long));
	if (cdbfr != NULL) {
		msglist = malloc(cdbfr->len + 1);
		if (msglist == NULL) {
			IMAPM_syslog(LOG_CRIT, "malloc() failed");
			CC->kill_me = KILLME_MALLOC_FAILED;
			return;
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
	if (Imap->num_msgs > 0) {
		jstart = 0;
		for (i = 0; i < Imap->num_msgs; ++i) {

			message_still_exists = 0;
			if (num_msgs > 0) {
				for (j = jstart; j < num_msgs; ++j) {
					if (msglist[j] == Imap->msgids[i]) {
						message_still_exists = 1;
						jstart = j;
						break;
					}
				}
			}

			if (message_still_exists == 0) {
				IAPrintf("* %d EXPUNGE\r\n", i + 1);

				/* Here's some nice stupid nonsense.  When a
				 * message is expunged, we have to slide all
				 * the existing messages up in the message
				 * array.
				 */
				--Imap->num_msgs;
				memmove(&Imap->msgids[i],
					&Imap->msgids[i + 1],
					(sizeof(long) *
					 (Imap->num_msgs - i)));
				memmove(&Imap->flags[i],
					&Imap->flags[i + 1],
					(sizeof(long) *
					 (Imap->num_msgs - i)));

				--i;
			}

		}
	}

	/*
	 * Remember how many messages were here before we re-scanned.
	 */
	original_num_msgs = Imap->num_msgs;
	if (Imap->num_msgs > 0) {
		original_highest = Imap->msgids[Imap->num_msgs - 1];
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
	if (Imap->num_msgs > original_num_msgs) {

		for (j = 0; j < num_msgs; ++j) {
			if (Imap->flags[j] & IMAP_RECENT) {
				++num_recent;
			}
		}

		IAPrintf("* %d EXISTS\r\n", Imap->num_msgs);
		IAPrintf("* %d RECENT\r\n", num_recent);
	}

	if (msglist != NULL) {
		free(msglist);
	}
	Imap->last_mtime = CC->room.QRmtime;
}


/*
 * This cleanup function blows away the temporary memory and files used by
 * the IMAP server.
 */
void imap_cleanup_function(void)
{
	struct CitContext *CCC = CC;
	citimap *Imap = CCCIMAP;

	/* Don't do this stuff if this is not a Imap session! */
	if (CC->h_command_function != imap_command_loop)
		return;

	/* If there is a mailbox selected, auto-expunge it. */
	if (Imap->selected) {
		imap_do_expunge();
	}

	IMAPM_syslog(LOG_DEBUG, "Performing IMAP cleanup hook");
	imap_free_msgids();
	imap_free_transmitted_message();

	if (Imap->cached_rfc822 != NULL) {
		FreeStrBuf(&Imap->cached_rfc822);
		Imap->cached_rfc822_msgnum = (-1);
		Imap->cached_rfc822_withbody = 0;
	}

	if (Imap->cached_body != NULL) {
		free(Imap->cached_body);
		Imap->cached_body = NULL;
		Imap->cached_body_len = 0;
		Imap->cached_bodymsgnum = (-1);
	}
	FreeStrBuf(&Imap->Cmd.CmdBuf);
	FreeStrBuf(&Imap->Reply);
	if (Imap->Cmd.Params != NULL) free(Imap->Cmd.Params);
	free(Imap);
	IMAPM_syslog(LOG_DEBUG, "Finished IMAP cleanup hook");
}


/*
 * Does the actual work of the CAPABILITY command (because we need to
 * output this stuff in other places as well)
 */
void imap_output_capability_string(void) {
	IAPuts("CAPABILITY IMAP4REV1 NAMESPACE ID AUTH=PLAIN AUTH=LOGIN UIDPLUS");

#ifdef HAVE_OPENSSL
	if (!CC->redirect_ssl) IAPuts(" STARTTLS");
#endif

#ifndef DISABLE_IMAP_ACL
	IAPuts(" ACL");
#endif

	/* We are building a partial implementation of METADATA for the sole purpose
	 * of interoperating with the ical/vcard version of the Bynari Insight Connector.
	 * It is not a full RFC5464 implementation, but it should refuse non-Bynari
	 * metadata in a compatible and graceful way.
	 */
	IAPuts(" METADATA");

	/*
	 * LIST-EXTENDED was originally going to be required by the METADATA extension.
	 * It was mercifully removed prior to the finalization of RFC5464.  We started
	 * implementing this but stopped when we learned that it would not be needed.
	 * If you uncomment this declaration you are responsible for writing a lot of new
	 * code.
	 *
	 * IAPuts(" LIST-EXTENDED")
	 */
}


/*
 * implements the CAPABILITY command
 */
void imap_capability(int num_parms, ConstStr *Params)
{
	IAPuts("* ");
	imap_output_capability_string();
	IAPuts("\r\n");
	IReply("OK CAPABILITY completed");
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
	IAPuts("* ID NIL\r\n");
	IReply("OK ID completed");
}


/*
 * Here's where our IMAP session begins its happy day.
 */
void imap_greeting(void)
{
	citimap *Imap;
	CitContext *CCC = CC;

	strcpy(CCC->cs_clientname, "IMAP session");
	CCC->session_specific_data = malloc(sizeof(citimap));
	Imap = (citimap *)CCC->session_specific_data;
	memset(Imap, 0, sizeof(citimap));
	Imap->authstate = imap_as_normal;
	Imap->cached_rfc822_msgnum = (-1);
	Imap->cached_rfc822_withbody = 0;
	Imap->Reply = NewStrBufPlain(NULL, SIZ * 10); /* 40k */

	if (CCC->nologin)
	{
		IAPuts("* BYE; Server busy, try later\r\n");
		CCC->kill_me = KILLME_NOLOGIN;
		IUnbuffer();
		return;
	}

	IAPuts("* OK [");
	imap_output_capability_string();
	IAPrintf("] %s IMAP4rev1 %s ready\r\n", config.c_fqdn, CITADEL);
	IUnbuffer();
}


/*
 * IMAPS is just like IMAP, except it goes crypto right away.
 */
void imaps_greeting(void) {
	CtdlModuleStartCryptoMsgs(NULL, NULL, NULL);
#ifdef HAVE_OPENSSL
	if (!CC->redirect_ssl) CC->kill_me = KILLME_NO_CRYPTO;		/* kill session if no crypto */
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
			IAPuts("+ go ahead\r\n");
			IMAP->authstate = imap_as_expecting_multilineusername;
			strcpy(IMAP->authseq, Params[0].Key);
			return;
		}
		else {
			IReply("BAD incorrect number of parameters");
			return;
		}
	case 4:
		if (CtdlLoginExistingUser(NULL, Params[2].Key) == login_ok) {
			if (CtdlTryPassword(Params[3].Key, Params[3].len) == pass_ok) {
				/* hm, thats not doable by IReply :-( */
				IAPrintf("%s OK [", Params[0].Key);
				imap_output_capability_string();
				IAPrintf("] Hello, %s\r\n", CC->user.fullname);
				return;
			}
			else
			{
				IReplyPrintf("NO AUTHENTICATE %s failed",
					     Params[3].Key);
			}
		}

		IReply("BAD Login incorrect");
	default:
		IReply("BAD incorrect number of parameters");
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
		IReply("BAD incorrect number of parameters");
		return;
	}

	if (CC->logged_in) {
		IReply("BAD Already logged in.");
		return;
	}

	if (!strcasecmp(Params[2].Key, "LOGIN")) {
		CtdlEncodeBase64(UsrBuf, "Username:", 9, 0);
		IAPrintf("+ %s\r\n", UsrBuf);
		IMAP->authstate = imap_as_expecting_username;
		strcpy(IMAP->authseq, Params[0].Key);
		return;
	}

	if (!strcasecmp(Params[2].Key, "PLAIN")) {
		// CtdlEncodeBase64(UsrBuf, "Username:", 9, 0);
		// IAPuts("+ %s\r\n", UsrBuf);
		IAPuts("+ \r\n");
		IMAP->authstate = imap_as_expecting_plainauth;
		strcpy(IMAP->authseq, Params[0].Key);
		return;
	}

	else {
		IReplyPrintf("NO AUTHENTICATE %s failed",
			     Params[1].Key);
	}
}


void imap_auth_plain(void)
{
	citimap *Imap = IMAP;
	const char *decoded_authstring;
	char ident[256];
	char user[256];
	char pass[256];
	int result;
	long len;

	memset(pass, 0, sizeof(pass));
	StrBufDecodeBase64(Imap->Cmd.CmdBuf);

	decoded_authstring = ChrPtr(Imap->Cmd.CmdBuf);
	safestrncpy(ident, decoded_authstring, sizeof ident);
	safestrncpy(user, &decoded_authstring[strlen(ident) + 1], sizeof user);
	len = safestrncpy(pass, &decoded_authstring[strlen(ident) + strlen(user) + 2], sizeof pass);
	if (len < 0)
		len = sizeof(pass) - 1;

	Imap->authstate = imap_as_normal;

	if (!IsEmptyStr(ident)) {
		result = CtdlLoginExistingUser(user, ident);
	}
	else {
		result = CtdlLoginExistingUser(NULL, user);
	}

	if (result == login_ok) {
		if (CtdlTryPassword(pass, len) == pass_ok) {
			IAPrintf("%s OK authentication succeeded\r\n", Imap->authseq);
			return;
		}
	}
	IAPrintf("%s NO authentication failed\r\n", Imap->authseq);
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
		IAPrintf("+ %s\r\n", PWBuf);
		
		Imap->authstate = imap_as_expecting_password;
		return;
	case imap_as_expecting_multilineusername:
		extract_token(PWBuf, ChrPtr(Imap->Cmd.CmdBuf), 1, ' ', sizeof(PWBuf));
		CtdlLoginExistingUser(NULL, ChrPtr(Imap->Cmd.CmdBuf));
		IAPuts("+ go ahead\r\n");
		Imap->authstate = imap_as_expecting_multilinepassword;
		return;
	}
}


void imap_auth_login_pass(long state)
{
	citimap *Imap = IMAP;
	const char *pass = NULL;
	long len = 0;

	switch (state) {
	default:
	case imap_as_expecting_password:
		StrBufDecodeBase64(Imap->Cmd.CmdBuf);
		pass = ChrPtr(Imap->Cmd.CmdBuf);
		len = StrLength(Imap->Cmd.CmdBuf);
		break;
	case imap_as_expecting_multilinepassword:
		pass = ChrPtr(Imap->Cmd.CmdBuf);
		len = StrLength(Imap->Cmd.CmdBuf);
		break;
	}
	if (len > USERNAME_SIZE)
		StrBufCutAt(Imap->Cmd.CmdBuf, USERNAME_SIZE, NULL);

	if (CtdlTryPassword(pass, len) == pass_ok) {
		IAPrintf("%s OK authentication succeeded\r\n", Imap->authseq);
	} else {
		IAPrintf("%s NO authentication failed\r\n", Imap->authseq);
	}
	Imap->authstate = imap_as_normal;
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
	citimap *Imap = IMAP;
	char towhere[ROOMNAMELEN];
	char augmented_roomname[ROOMNAMELEN];
	int c = 0;
	int ok = 0;
	int ra = 0;
	struct ctdlroom QRscratch;
	int msgs, new;
	int i;

	/* Convert the supplied folder name to a roomname */
	i = imap_roomname(towhere, sizeof towhere, Params[2].Key);
	if (i < 0) {
		IReply("NO Invalid mailbox name.");
		Imap->selected = 0;
		return;
	}

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
		IReply("NO ... no such room, or access denied");
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
	Imap->selected = 1;

	if (!strcasecmp(Params[1].Key, "EXAMINE")) {
		Imap->readonly = 1;
	} else {
		Imap->readonly = 0;
	}

	imap_load_msgids();
	Imap->last_mtime = CC->room.QRmtime;

	IAPrintf("* %d EXISTS\r\n", msgs);
	IAPrintf("* %d RECENT\r\n", new);

	IAPrintf("* OK [UIDVALIDITY %ld] UID validity status\r\n", GLOBAL_UIDVALIDITY_VALUE);
	IAPrintf("* OK [UIDNEXT %ld] Predicted next UID\r\n", CitControl.MMhighest + 1);

	/* Technically, \Deleted is a valid flag, but not a permanent flag,
	 * because we don't maintain its state across sessions.  Citadel
	 * automatically expunges mailboxes when they are de-selected.
	 * 
	 * Unfortunately, omitting \Deleted as a PERMANENTFLAGS flag causes
	 * some clients (particularly Thunderbird) to misbehave -- they simply
	 * elect not to transmit the flag at all.  So we have to advertise
	 * \Deleted as a PERMANENTFLAGS flag, even though it technically isn't.
	 */
	IAPuts("* FLAGS (\\Deleted \\Seen \\Answered)\r\n");
	IAPuts("* OK [PERMANENTFLAGS (\\Deleted \\Seen \\Answered)] permanent flags\r\n");

	IReplyPrintf("OK [%s] %s completed",
		(Imap->readonly ? "READ-ONLY" : "READ-WRITE"), Params[1].Key
	);
}


/*
 * Does the real work for expunge.
 */
int imap_do_expunge(void)
{
	struct CitContext *CCC = CC;
	citimap *Imap = CCCIMAP;
	int i;
	int num_expunged = 0;
	long *delmsgs = NULL;
	int num_delmsgs = 0;

	IMAPM_syslog(LOG_DEBUG, "imap_do_expunge() called");
	if (Imap->selected == 0) {
		return (0);
	}

	if (Imap->num_msgs > 0) {
		delmsgs = malloc(Imap->num_msgs * sizeof(long));
		for (i = 0; i < Imap->num_msgs; ++i) {
			if (Imap->flags[i] & IMAP_DELETED) {
				delmsgs[num_delmsgs++] = Imap->msgids[i];
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

	IMAP_syslog(LOG_DEBUG, "Expunged %d messages from <%s>", num_expunged, CC->room.QRname);
	return (num_expunged);
}


/*
 * implements the EXPUNGE command syntax
 */
void imap_expunge(int num_parms, ConstStr *Params)
{
	int num_expunged = 0;

	num_expunged = imap_do_expunge();
	IReplyPrintf("OK expunged %d messages.", num_expunged);
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
	IReply("OK CLOSE completed");
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

	IAPuts("* NAMESPACE ");

	/* All personal folders are subordinate to INBOX. */
	IAPuts("((\"INBOX/\" \"/\")) ");

	/* Other users' folders ... coming soon! FIXME */
	IAPuts("NIL ");

	/* Show all floors as shared namespaces.  Neato! */
	IAPuts("(");
	for (i = 0; i < MAXFLOORS; ++i) {
		fl = CtdlGetCachedFloor(i);
		if (fl->f_flags & F_INUSE) {
			/* if (floors > 0) IAPuts(" "); samjam says this confuses javamail */
			IAPuts("(");
			snprintf(Namespace, sizeof(Namespace), "%s/", fl->f_name);
			plain_imap_strout(Namespace);
			IAPuts(" \"/\")");
			++floors;
		}
	}
	IAPuts(")");

	/* Wind it up with a newline and a completion message. */
	IAPuts("\r\n");
	IReply("OK NAMESPACE completed");
}


/*
 * Implements the CREATE command
 *
 */
void imap_create(int num_parms, ConstStr *Params)
{
	struct CitContext *CCC = CC;
	int ret;
	char roomname[ROOMNAMELEN];
	int floornum;
	int flags;
	int newroomtype = 0;
	int newroomview = 0;
	char *notification_message = NULL;

	if (num_parms < 3) {
		IReply("NO A foder name must be specified");
		return;
	}

	if (strchr(Params[2].Key, '\\') != NULL) {
		IReply("NO Invalid character in folder name");
		IMAPM_syslog(LOG_ERR, "invalid character in folder name");
		return;
	}

	ret = imap_roomname(roomname, sizeof roomname, Params[2].Key);
	if (ret < 0) {
		IReply("NO Invalid mailbox name or location");
		IMAPM_syslog(LOG_ERR, "invalid mailbox name or location");
		return;
	}
	floornum = (ret & 0x00ff);	/* lower 8 bits = floor number */
	flags = (ret & 0xff00);	/* upper 8 bits = flags        */

	if (flags & IR_MAILBOX) {
		if (strncasecmp(Params[2].Key, "INBOX/", 6)) {
			IReply("NO Personal folders must be created under INBOX");
			IMAPM_syslog(LOG_ERR, "not subordinate to inbox");
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

	IMAP_syslog(LOG_INFO, "Create new room <%s> on floor <%d> with type <%d>",
		    roomname, floornum, newroomtype);

	ret = CtdlCreateRoom(roomname, newroomtype, "", floornum, 1, 0, newroomview);
	if (ret == 0) {
		/*** DO NOT CHANGE THIS ERROR MESSAGE IN ANY WAY!  BYNARI CONNECTOR DEPENDS ON IT! ***/
		IReply("NO Mailbox already exists, or create failed");
	} else {
		IReply("OK CREATE completed");
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
	IMAPM_syslog(LOG_DEBUG, "imap_create() completed");
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
		IReply("NO Invalid mailbox name or location, or access denied");
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
	IAPuts("* STATUS ");
	       plain_imap_strout(imaproomname);
	IAPrintf(" (MESSAGES %d ", msgs);
	IAPrintf("RECENT %d ", new);	/* Initially, new==recent */
	IAPrintf("UIDNEXT %ld ", CitControl.MMhighest + 1);
	IAPrintf("UNSEEN %d)\r\n", new);
	
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
	IReply("OK STATUS completed");
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
		IReplyPrintf(
			"NO Error %d: invalid mailbox name or location, or access denied",
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

	IReply("OK SUBSCRIBE completed");
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
		IReply("NO Invalid mailbox name or location, or access denied");
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
		IReply("OK UNSUBSCRIBE completed");
	} else {
		IReply("NO You may not unsubscribe from this folder.");
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
		IReply("NO Invalid mailbox name, or access denied");
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
		IReply("OK DELETE completed");
	} else {
		IReply("NO Can't delete this folder.");
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
	irlparms *myirlparms;

	myirlparms = (irlparms *) data;
	imap_mailboxname(foldername, sizeof foldername, qrbuf);

	/* Rename subfolders */
	if ((!strncasecmp(foldername, myirlparms->oldname,
			  myirlparms->oldnamelen)
	    && (foldername[myirlparms->oldnamelen] == '/'))) {

		sprintf(newfoldername, "%s/%s",
			myirlparms->newname,
			&foldername[myirlparms->oldnamelen + 1]
		    );

		newfloor = imap_roomname(newroomname,
					 sizeof newroomname,
					 newfoldername) & 0xFF;

		irlp = (struct irl *) malloc(sizeof(struct irl));
		strcpy(irlp->irl_newroom, newroomname);
		strcpy(irlp->irl_oldroom, qrbuf->QRname);
		irlp->irl_newfloor = newfloor;
		irlp->next = *(myirlparms->irl);
		*(myirlparms->irl) = irlp;
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
	int newr;
	int new_floor;
	int r;
	struct irl *irl = NULL;	/* the list */
	struct irl *irlp = NULL;	/* scratch pointer */
	irlparms irlparms;
	char aidemsg[1024];

	if (strchr(Params[3].Key, '\\') != NULL) {
		IReply("NO Invalid character in folder name");
		return;
	}

	imap_roomname(old_room, sizeof old_room, Params[2].Key);
	newr = imap_roomname(new_room, sizeof new_room, Params[3].Key);
	new_floor = (newr & 0xFF);

	r = CtdlRenameRoom(old_room, new_room, new_floor);

	if (r == crr_room_not_found) {
		IReply("NO Could not locate this folder");
		return;
	}
	if (r == crr_already_exists) {
		IReplyPrintf("NO '%s' already exists.");
		return;
	}
	if (r == crr_noneditable) {
		IReply("NO This folder is not editable.");
		return;
	}
	if (r == crr_invalid_floor) {
		IReply("NO Folder root does not exist.");
		return;
	}
	if (r == crr_access_denied) {
		IReply("NO You do not have permission to edit this folder.");
		return;
	}
	if (r != crr_ok) {
		IReplyPrintf("NO Rename failed - undefined error %d", r);
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
		irlparms.oldnamelen = Params[2].len;
		irlparms.newname = Params[3].Key;
		irlparms.newnamelen = Params[3].len;
		irlparms.irl = &irl;
		CtdlForEachRoom(imap_rename_backend, (void *) &irlparms);

		/* ... and now rename them. */
		while (irl != NULL) {
			r = CtdlRenameRoom(irl->irl_oldroom,
					   irl->irl_newroom,
					   irl->irl_newfloor);
			if (r != crr_ok) {
				struct CitContext *CCC = CC;
				/* FIXME handle error returns better */
				IMAP_syslog(LOG_ERR, "CtdlRenameRoom() error %d", r);
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

	IReply("OK RENAME completed");
}


/* 
 * Main command loop for IMAP sessions.
 */
void imap_command_loop(void)
{
	struct CitContext *CCC = CC;
	struct timeval tv1, tv2;
	suseconds_t total_time = 0;
	citimap *Imap;
	const char *pchs, *pche;
	const imap_handler_hook *h;

	gettimeofday(&tv1, NULL);
	CCC->lastcmd = time(NULL);
	Imap = CCCIMAP;

	flush_output();
	if (Imap->Cmd.CmdBuf == NULL)
		Imap->Cmd.CmdBuf = NewStrBufPlain(NULL, SIZ);
	else
		FlushStrBuf(Imap->Cmd.CmdBuf);

	if (CtdlClientGetLine(Imap->Cmd.CmdBuf) < 1) {
		IMAPM_syslog(LOG_ERR, "client disconnected: ending session.");
		CC->kill_me = KILLME_CLIENT_DISCONNECTED;
		return;
	}

	if (Imap->authstate == imap_as_expecting_password) {
		IMAPM_syslog(LOG_INFO, "<password>");
	}
	else if (Imap->authstate == imap_as_expecting_plainauth) {
		IMAPM_syslog(LOG_INFO, "<plain_auth>");
	}
	else if ((Imap->authstate == imap_as_expecting_multilineusername) || 
		 cbmstrcasestr(ChrPtr(Imap->Cmd.CmdBuf), " LOGIN ")) {
		IMAPM_syslog(LOG_INFO, "LOGIN...");
	}
	else {
		IMAP_syslog(LOG_DEBUG, "%s", ChrPtr(Imap->Cmd.CmdBuf));
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
		IUnbuffer();
		return;
	case imap_as_expecting_multilineusername:
		imap_auth_login_user(imap_as_expecting_multilineusername);
		IUnbuffer();
		return;
	case imap_as_expecting_plainauth:
		imap_auth_plain();
		IUnbuffer();
		return;
	case imap_as_expecting_password:
		imap_auth_login_pass(imap_as_expecting_password);
		IUnbuffer();
		return;
	case imap_as_expecting_multilinepassword:
		imap_auth_login_pass(imap_as_expecting_multilinepassword);
		IUnbuffer();
		return;
	default:
		break;
	}

	/* Ok, at this point we're in normal command mode.
	 * If the command just submitted does not contain a literal, we
	 * might think about delivering some untagged stuff...
	 */

	/* Grab the tag, command, and parameters. */
	imap_parameterize(&Imap->Cmd);
#if 0 
/* debug output the parsed vector */
	{
		int i;
		IMAP_syslog(LOG_DEBUG, "----- %ld params", Imap->Cmd.num_parms);

	for (i=0; i < Imap->Cmd.num_parms; i++) {
		if (Imap->Cmd.Params[i].len != strlen(Imap->Cmd.Params[i].Key))
			IMAP_syslog(LOG_DEBUG, "*********** %ld != %ld : %s",
				    Imap->Cmd.Params[i].len, 
				    strlen(Imap->Cmd.Params[i].Key),
				      Imap->Cmd.Params[i].Key);
		else
			IMAP_syslog(LOG_DEBUG, "%ld : %s",
				    Imap->Cmd.Params[i].len, 
				    Imap->Cmd.Params[i].Key);
	}}
#endif

	/* Now for the command set. */
	h = imap_lookup(Imap->Cmd.num_parms, Imap->Cmd.Params);

	if (h == NULL)
	{
		IReply("BAD command unrecognized");
		goto BAIL;
	}

	/* RFC3501 says that we cannot output untagged data during these commands */
	if ((h->Flags & I_FLAG_UNTAGGED) == 0) {

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

	/* does our command require a logged-in state */
	if ((!CC->logged_in) && ((h->Flags & I_FLAG_LOGGED_IN) != 0)) {
		IReply("BAD Not logged in.");
		goto BAIL;
	}

	/* does our command require the SELECT state on a mailbox */
	if ((Imap->selected == 0) && ((h->Flags & I_FLAG_SELECT) != 0)){
		IReply("BAD no folder selected");
		goto BAIL;
	}
	h->h(Imap->Cmd.num_parms, Imap->Cmd.Params);

	/* If the client transmitted a message we can free it now */

BAIL:
	IUnbuffer();

	imap_free_transmitted_message();

	gettimeofday(&tv2, NULL);
	total_time = (tv2.tv_usec + (tv2.tv_sec * 1000000)) - (tv1.tv_usec + (tv1.tv_sec * 1000000));
	IMAP_syslog(LOG_DEBUG, "IMAP command completed in %ld.%ld seconds",
		    (total_time / 1000000),
		    (total_time % 1000000)
		);
}

void imap_noop (int num_parms, ConstStr *Params)
{
	IReply("OK No operation");
}

void imap_logout(int num_parms, ConstStr *Params)
{
	if (IMAP->selected) {
		imap_do_expunge();	/* yes, we auto-expunge at logout */
	}
	IAPrintf("* BYE %s logging out\r\n", config.c_fqdn);
	IReply("OK Citadel IMAP session ended.");
	CC->kill_me = KILLME_CLIENT_LOGGED_OUT;
	return;
}

const char *CitadelServiceIMAP="IMAP";
const char *CitadelServiceIMAPS="IMAPS";

void SetIMAPDebugEnabled(const int n)
{
	IMAPDebugEnabled = n;
}
/*
 * This function is called to register the IMAP extension with Citadel.
 */
CTDL_MODULE_INIT(imap)
{
	if (ImapCmds == NULL)
		ImapCmds = NewHash(1, NULL);

	RegisterImapCMD("NOOP", "", imap_noop, I_FLAG_NONE);
	RegisterImapCMD("CHECK", "", imap_noop, I_FLAG_NONE);
	RegisterImapCMD("ID", "", imap_id, I_FLAG_NONE);
	RegisterImapCMD("LOGOUT", "", imap_logout, I_FLAG_NONE);
	RegisterImapCMD("LOGIN", "", imap_login, I_FLAG_NONE);
	RegisterImapCMD("AUTHENTICATE", "", imap_authenticate, I_FLAG_NONE);
	RegisterImapCMD("CAPABILITY", "", imap_capability, I_FLAG_NONE);
#ifdef HAVE_OPENSSL
	RegisterImapCMD("STARTTLS", "", imap_starttls, I_FLAG_NONE);
#endif

	/* The commans below require a logged-in state */
	RegisterImapCMD("SELECT", "", imap_select, I_FLAG_LOGGED_IN);
	RegisterImapCMD("EXAMINE", "", imap_select, I_FLAG_LOGGED_IN);
	RegisterImapCMD("LSUB", "", imap_list, I_FLAG_LOGGED_IN);
	RegisterImapCMD("LIST", "", imap_list, I_FLAG_LOGGED_IN);
	RegisterImapCMD("CREATE", "", imap_create, I_FLAG_LOGGED_IN);
	RegisterImapCMD("DELETE", "", imap_delete, I_FLAG_LOGGED_IN);
	RegisterImapCMD("RENAME", "", imap_rename, I_FLAG_LOGGED_IN);
	RegisterImapCMD("STATUS", "", imap_status, I_FLAG_LOGGED_IN);
	RegisterImapCMD("SUBSCRIBE", "", imap_subscribe, I_FLAG_LOGGED_IN);
	RegisterImapCMD("UNSUBSCRIBE", "", imap_unsubscribe, I_FLAG_LOGGED_IN);
	RegisterImapCMD("APPEND", "", imap_append, I_FLAG_LOGGED_IN);
	RegisterImapCMD("NAMESPACE", "", imap_namespace, I_FLAG_LOGGED_IN);
	RegisterImapCMD("SETACL", "", imap_setacl, I_FLAG_LOGGED_IN);
	RegisterImapCMD("DELETEACL", "", imap_deleteacl, I_FLAG_LOGGED_IN);
	RegisterImapCMD("GETACL", "", imap_getacl, I_FLAG_LOGGED_IN);
	RegisterImapCMD("LISTRIGHTS", "", imap_listrights, I_FLAG_LOGGED_IN);
	RegisterImapCMD("MYRIGHTS", "", imap_myrights, I_FLAG_LOGGED_IN);
	RegisterImapCMD("GETMETADATA", "", imap_getmetadata, I_FLAG_LOGGED_IN);
	RegisterImapCMD("SETMETADATA", "", imap_setmetadata, I_FLAG_LOGGED_IN);

	/* The commands below require the SELECT state on a mailbox */
	RegisterImapCMD("FETCH", "", imap_fetch, I_FLAG_LOGGED_IN | I_FLAG_SELECT | I_FLAG_UNTAGGED);
	RegisterImapCMD("UID", "FETCH", imap_uidfetch, I_FLAG_LOGGED_IN | I_FLAG_SELECT);
	RegisterImapCMD("SEARCH", "", imap_search, I_FLAG_LOGGED_IN | I_FLAG_SELECT | I_FLAG_UNTAGGED);
	RegisterImapCMD("UID", "SEARCH", imap_uidsearch, I_FLAG_LOGGED_IN | I_FLAG_SELECT);
	RegisterImapCMD("STORE", "", imap_store, I_FLAG_LOGGED_IN | I_FLAG_SELECT | I_FLAG_UNTAGGED);
	RegisterImapCMD("UID", "STORE", imap_uidstore, I_FLAG_LOGGED_IN | I_FLAG_SELECT);
	RegisterImapCMD("COPY", "", imap_copy, I_FLAG_LOGGED_IN | I_FLAG_SELECT);
	RegisterImapCMD("UID", "COPY", imap_uidcopy, I_FLAG_LOGGED_IN | I_FLAG_SELECT);
	RegisterImapCMD("EXPUNGE", "", imap_expunge, I_FLAG_LOGGED_IN | I_FLAG_SELECT);
	RegisterImapCMD("UID", "EXPUNGE", imap_expunge, I_FLAG_LOGGED_IN | I_FLAG_SELECT);
	RegisterImapCMD("CLOSE", "", imap_close, I_FLAG_LOGGED_IN | I_FLAG_SELECT);

	if (!threading)
	{
		CtdlRegisterDebugFlagHook(HKEY("imapsrv"), SetIMAPDebugEnabled, &IMAPDebugEnabled);
		CtdlRegisterServiceHook(config.c_imap_port,
					NULL, imap_greeting, imap_command_loop, NULL, CitadelServiceIMAP);
#ifdef HAVE_OPENSSL
		CtdlRegisterServiceHook(config.c_imaps_port,
					NULL, imaps_greeting, imap_command_loop, NULL, CitadelServiceIMAPS);
#endif
		CtdlRegisterSessionHook(imap_cleanup_function, EVT_STOP, PRIO_STOP + 30);
		CtdlRegisterCleanupHook(imap_cleanup);
	}
	
	/* return our module name for the log */
	return "imap";
}
