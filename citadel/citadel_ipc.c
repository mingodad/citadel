/* $Id$ */

#include "sysdep.h"
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
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <ctype.h>
#ifdef THREADED_CLIENT
#include <pthread.h>
#endif
#include "citadel.h"
#include "citadel_ipc.h"
#include "citadel_decls.h"
#include "client_crypto.h"
#include "tools.h"

#ifdef THREADED_CLIENT
pthread_mutex_t rwlock;
#endif
char express_msgs = 0;


/*
 * Does nothing.  The server should always return 200.
 */
int CtdlIPCNoop(CtdlIPC *ipc)
{
	char aaa[128];

	return CtdlIPCGenericCommand(ipc, "NOOP", NULL, 0, NULL, NULL, aaa);
}


/*
 * Does nothing interesting.  The server should always return 200
 * along with your string.
 */
int CtdlIPCEcho(CtdlIPC *ipc, const char *arg, char *cret)
{
	register int ret;
	char *aaa;
	
	if (!arg) return -2;
	if (!cret) return -2;

	aaa = (char *)malloc((size_t)(strlen(arg) + 6));
	if (!aaa) return -1;

	sprintf(aaa, "ECHO %s", arg);
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/*
 * Asks the server to close the connecction.
 * Should always return 200.
 */
int CtdlIPCQuit(CtdlIPC *ipc)
{
	register int ret;
	char aaa[128];

	CtdlIPC_lock(ipc);
	CtdlIPC_putline(ipc, "QUIT");
	CtdlIPC_getline(ipc, aaa);
	ret = atoi(aaa);
	CtdlIPC_unlock(ipc);
	return ret;
}


/*
 * Asks the server to logout.  Should always return 200, even if no user
 * was logged in.  The user will not be logged in after this!
 */
int CtdlIPCLogout(CtdlIPC *ipc)
{
	register int ret;
	char aaa[128];

	CtdlIPC_lock(ipc);
	CtdlIPC_putline(ipc, "LOUT");
	CtdlIPC_getline(ipc, aaa);
	ret = atoi(aaa);
	CtdlIPC_unlock(ipc);
	return ret;
}


/*
 * First stage of authentication - pass the username.  Returns 300 if the
 * username is able to log in, with the username correctly spelled in cret.
 * Returns various 500 error codes if the user doesn't exist, etc.
 */
int CtdlIPCTryLogin(CtdlIPC *ipc, const char *username, char *cret)
{
	register int ret;
	char *aaa;

	if (!username) return -2;
	if (!cret) return -2;

	aaa = (char *)malloc((size_t)(strlen(username) + 6));
	if (!aaa) return -1;

	sprintf(aaa, "USER %s", username);
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/*
 * Second stage of authentication - provide password.  The server returns
 * 200 and several arguments in cret relating to the user's account.
 */
int CtdlIPCTryPassword(CtdlIPC *ipc, const char *passwd, char *cret)
{
	register int ret;
	char *aaa;

	if (!passwd) return -2;
	if (!cret) return -2;

	aaa = (char *)malloc((size_t)(strlen(passwd) + 6));
	if (!aaa) return -1;

	sprintf(aaa, "PASS %s", passwd);
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/*
 * Create a new user.  This returns 200 plus the same arguments as TryPassword
 * if selfservice is nonzero, unless there was a problem creating the account.
 * If selfservice is zero, creates a new user but does not log out the existing
 * user - intended for use by system administrators to create accounts on
 * behalf of other users.
 */
int CtdlIPCCreateUser(CtdlIPC *ipc, const char *username, int selfservice, char *cret)
{
	register int ret;
	char *aaa;

	if (!username) return -2;
	if (!cret) return -2;

	aaa = (char *)malloc((size_t)(strlen(username) + 6));
	if (!aaa) return -1;

	sprintf(aaa, "%s %s", selfservice ? "NEWU" : "CREU",  username);
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/*
 * Changes the user's password.  Returns 200 if changed, errors otherwise.
 */
int CtdlIPCChangePassword(CtdlIPC *ipc, const char *passwd, char *cret)
{
	register int ret;
	char *aaa;

	if (!passwd) return -2;
	if (!cret) return -2;

	aaa = (char *)malloc((size_t)(strlen(passwd) + 6));
	if (!aaa) return -1;

	sprintf(aaa, "SETP %s", passwd);
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/* LKRN */
/* Caller must free the march list */
/* which is 0 = LRMS, 1 = LKRN, 2 = LKRO, 3 = LKRA, 4 = LZRM */
/* floor is -1 for all, or floornum */
int CtdlIPCKnownRooms(CtdlIPC *ipc, int which, int floor, struct march **listing, char *cret)
{
	register int ret;
	struct march *march = NULL;
	static char *proto[] = {"LRMS", "LKRN", "LKRO", "LKRA", "LZRM" };
	char aaa[SIZ];
	char *bbb = NULL;
	size_t bbbsize;

	if (!listing) return -2;
	if (*listing) return -2;	/* Free the listing first */
	if (!cret) return -2;
	if (which < 0 || which > 4) return -2;
	if (floor < -1) return -2;	/* Can't validate upper bound, sorry */

	sprintf(aaa, "%s %d", proto[which], floor);
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, &bbb, &bbbsize, cret);
	if (ret / 100 == 1) {
		struct march *mptr;

		while (bbb && strlen(bbb)) {
			int a;

			extract_token(aaa, bbb, 0, '\n');
			a = strlen(aaa);
			memmove(bbb, bbb + a + 1, strlen(bbb) - a);
			mptr = (struct march *) malloc(sizeof (struct march));
			if (mptr) {
				mptr->next = NULL;
				extract(mptr->march_name, aaa, 0);
				mptr->march_floor = (char) extract_int(aaa, 2);
				mptr->march_order = (char) extract_int(aaa, 3);
				if (march == NULL)
					march = mptr;
				else {
					struct march *mptr2;

					mptr2 = march;
					while (mptr2->next != NULL)
						mptr2 = mptr2->next;
					mptr2->next = mptr;
				}
			}
		}
	}
	*listing = march;
	return ret;
}


/* GETU */
/* Caller must free the struct usersupp; caller may pass an existing one */
int CtdlIPCGetConfig(CtdlIPC *ipc, struct usersupp **uret, char *cret)
{
	register int ret;

	if (!cret) return -2;
	if (!uret) return -2;
	if (!*uret) *uret = (struct usersupp *)calloc(1, sizeof (struct usersupp));
	if (!*uret) return -1;

	ret = CtdlIPCGenericCommand(ipc, "GETU", NULL, 0, NULL, NULL, cret);
	if (ret / 100 == 2) {
		uret[0]->USscreenwidth = extract_int(cret, 0);
		uret[0]->USscreenheight = extract_int(cret, 1);
		uret[0]->flags = extract_int(cret, 2);
	}
	return ret;
}


/* SETU */
int CtdlIPCSetConfig(CtdlIPC *ipc, struct usersupp *uret, char *cret)
{
	char aaa[48];

	if (!uret) return -2;
	if (!cret) return -2;

	sprintf(aaa, "SETU %d|%d|%d",
			uret->USscreenwidth, uret->USscreenheight,
			uret->flags);
	return CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
}


/* GOTO */
int CtdlIPCGotoRoom(CtdlIPC *ipc, const char *room, const char *passwd,
		struct ctdlipcroom **rret, char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -2;
	if (!rret) return -2;
	if (!*rret) *rret = (struct ctdlipcroom *)calloc(1, sizeof (struct ctdlipcroom));
	if (!*rret) return -1;

	if (passwd) {
		aaa = (char *)malloc(strlen(room) + strlen(passwd) + 7);
		if (!aaa) {
			free(*rret);
			return -1;
		}
		sprintf(aaa, "GOTO %s|%s", room, passwd);
	} else {
		aaa = (char *)malloc(strlen(room) + 6);
		if (!aaa) {
			free(*rret);
			return -1;
		}
		sprintf(aaa, "GOTO %s", room);
	}
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
	if (ret / 100 == 2) {
		extract(rret[0]->RRname, cret, 0);
		rret[0]->RRunread = extract_long(cret, 1);
		rret[0]->RRtotal = extract_long(cret, 2);
		rret[0]->RRinfoupdated = extract_int(cret, 3);
		rret[0]->RRflags = extract_int(cret, 4);
		rret[0]->RRhighest = extract_long(cret, 5);
		rret[0]->RRlastread = extract_long(cret, 6);
		rret[0]->RRismailbox = extract_int(cret, 7);
		rret[0]->RRaide = extract_int(cret, 8);
		rret[0]->RRnewmail = extract_long(cret, 9);
		rret[0]->RRfloor = extract_int(cret, 10);
	} else {
		free(*rret);
	}
	return ret;
}


/* MSGS */
/* which is 0 = all, 1 = old, 2 = new, 3 = last, 4 = first, 5 = gt, 6 = lt */
/* whicharg is number of messages, applies to last, first, gt, lt */
int CtdlIPCGetMessages(CtdlIPC *ipc, int which, int whicharg, const char *template,
		long **mret, char *cret)
{
	register int ret;
	register long count = 0;
	static char *proto[] =
		{ "ALL", "OLD", "NEW", "LAST", "FIRST", "GT", "LT" };
	char aaa[33];
	char *bbb;
	size_t bbbsize;

	if (!cret) return -2;
	if (!mret) return -2;
	if (*mret) return -2;
	if (which < 0 || which > 6) return -2;

	if (which <= 2)
		sprintf(aaa, "MSGS %s||%d", proto[which],
				(template) ? 1 : 0);
	else
		sprintf(aaa, "MSGS %s|%d|%d", proto[which], whicharg,
				(template) ? 1 : 0);
	if (template) count = strlen(template);
	ret = CtdlIPCGenericCommand(ipc, aaa, template, count, &bbb, &bbbsize, cret);
	count = 0;
	while (strlen(bbb)) {
		int a;

		extract_token(aaa, bbb, 0, '\n');
		a = strlen(aaa);
		memmove(aaa, bbb + a + 1, strlen(bbb) - a - 1);
		*mret = (long *)realloc(mret,
					(size_t)((count + 1) * sizeof (long)));
		if (*mret)
			*mret[count++] = atol(aaa);
		*mret[count] = 0L;
	}
	return ret;
}


/* MSG0, MSG2 */
int CtdlIPCGetSingleMessage(CtdlIPC *ipc, long msgnum, int headers, int as_mime,
		struct ctdlipcmessage **mret, char *cret)
{
	register int ret;
	char aaa[SIZ];
	char *bbb = NULL;
	size_t bbbsize;
	int multipart_hunting = 0;
	char multipart_prefix[SIZ];

	if (!cret) return -1;
	if (!mret) return -1;
	if (!*mret) *mret = (struct ctdlipcmessage *)calloc(1, sizeof (struct ctdlipcmessage));
	if (!*mret) return -1;
	if (!msgnum) return -1;

	strcpy(mret[0]->content_type, "");
	sprintf(aaa, "MSG%d %ld|%d", as_mime, msgnum, headers);
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, &bbb, &bbbsize, cret);
	if (ret / 100 == 1) {
		if (as_mime != 2) {
			strcpy(mret[0]->mime_chosen, "1");	/* Default chosen-part is "1" */
			while (strlen(bbb) > 4 && bbb[4] == '=') {
				extract_token(aaa, bbb, 0, '\n');
				remove_token(bbb, 0, '\n');

				if (!strncasecmp(aaa, "nhdr=yes", 8))
					mret[0]->nhdr = 1;
				else if (!strncasecmp(aaa, "from=", 5))
					strcpy(mret[0]->author, &aaa[5]);
				else if (!strncasecmp(aaa, "type=", 5))
					mret[0]->type = atoi(&aaa[5]);
				else if (!strncasecmp(aaa, "msgn=", 5))
					strcpy(mret[0]->msgid, &aaa[5]);
				else if (!strncasecmp(aaa, "subj=", 5))
					strcpy(mret[0]->subject, &aaa[5]);
				else if (!strncasecmp(aaa, "rfca=", 5))
					strcpy(mret[0]->email, &aaa[5]);
				else if (!strncasecmp(aaa, "hnod=", 5))
					strcpy(mret[0]->hnod, &aaa[5]);
				else if (!strncasecmp(aaa, "room=", 5))
					strcpy(mret[0]->room, &aaa[5]);
				else if (!strncasecmp(aaa, "node=", 5))
					strcpy(mret[0]->node, &aaa[5]);
				else if (!strncasecmp(aaa, "rcpt=", 5))
					strcpy(mret[0]->recipient, &aaa[5]);
				else if (!strncasecmp(aaa, "time=", 5))
					mret[0]->time = atol(&aaa[5]);

				/* Multipart/alternative prefix & suffix strings help
				 * us to determine which part we want to download.
				 */
				else if (!strncasecmp(aaa, "pref=", 5)) {
					extract(multipart_prefix, &aaa[5], 1);
					if (!strcasecmp(multipart_prefix,
					   "multipart/alternative")) {
						++multipart_hunting;
					}
				}
				else if (!strncasecmp(aaa, "suff=", 5)) {
					extract(multipart_prefix, &aaa[5], 1);
					if (!strcasecmp(multipart_prefix,
					   "multipart/alternative")) {
						++multipart_hunting;
					}
				}

				else if (!strncasecmp(aaa, "part=", 5)) {
					struct parts *ptr, *chain;
	
					ptr = (struct parts *)calloc(1, sizeof (struct parts));
					if (ptr) {

						/* Fill the buffers for the caller */
						extract(ptr->name, &aaa[5], 0);
						extract(ptr->filename, &aaa[5], 1);
						extract(ptr->number, &aaa[5], 2);
						extract(ptr->disposition, &aaa[5], 3);
						extract(ptr->mimetype, &aaa[5], 4);
						ptr->length = extract_long(&aaa[5], 5);
						if (!mret[0]->attachments)
							mret[0]->attachments = ptr;
						else {
							chain = mret[0]->attachments;
							while (chain->next)
								chain = chain->next;
							chain->next = ptr;
						}

						/* Now handle multipart/alternative */
						if (multipart_hunting > 0) {
							if ( (!strcasecmp(ptr->mimetype,
							     "text/plain"))
							   || (!strcasecmp(ptr->mimetype,
							      "text/html")) ) {
								strcpy(mret[0]->mime_chosen,
									ptr->number);
							}
						}

					}
				}
			}
			/* Eliminate "text\n" */
			remove_token(bbb, 0, '\n');

			/* If doing a MIME thing, pull out the extra headers */
			if (as_mime == 4) {
				do {
					if (!strncasecmp(bbb, "Content-type: ", 14)) {
						extract_token(mret[0]->content_type, bbb, 0, '\n');
						strcpy(mret[0]->content_type,
							&mret[0]->content_type[14]);
						striplt(mret[0]->content_type);
					}
					remove_token(bbb, 0, '\n');
				} while ((bbb[0] != 0) && (bbb[0] != '\n'));
			}


		}
		if (strlen(bbb)) {
			/* Strip trailing whitespace */
			bbb = (char *)realloc(bbb, (size_t)(strlen(bbb) + 1));
		} else {
			bbb = (char *)realloc(bbb, 1);
			*bbb = '\0';
		}
		mret[0]->text = bbb;
	}
	return ret;
}


/* WHOK */
int CtdlIPCWhoKnowsRoom(CtdlIPC *ipc, char **listing, char *cret)
{
	register int ret;
	size_t bytes;

	if (!cret) return -2;
	if (!listing) return -2;
	if (*listing) return -2;

	ret = CtdlIPCGenericCommand(ipc, "WHOK", NULL, 0, listing, &bytes, cret);
	return ret;
}


/* INFO */
int CtdlIPCServerInfo(CtdlIPC *ipc, struct CtdlServInfo *ServInfo, char *cret)
{
	register int ret;
	size_t bytes;
	char *listing = NULL;
	char buf[SIZ];

	if (!cret) return -2;
	if (!ServInfo) return -2;

	ret = CtdlIPCGenericCommand(ipc, "INFO", NULL, 0, &listing, &bytes, cret);
	if (ret / 100 == 1) {
		int line = 0;

		while (*listing && strlen(listing)) {
			extract_token(buf, listing, 0, '\n');
			remove_token(listing, 0, '\n');
			switch (line++) {
			case 0:		ServInfo->serv_pid = atoi(buf);
					break;
			case 1:		strcpy(ServInfo->serv_nodename,buf);
					break;
			case 2:		strcpy(ServInfo->serv_humannode,buf);
					break;
			case 3:		strcpy(ServInfo->serv_fqdn,buf);
					break;
			case 4:		strcpy(ServInfo->serv_software,buf);
					break;
			case 5:		ServInfo->serv_rev_level = atoi(buf);
					break;
			case 6:		strcpy(ServInfo->serv_bbs_city,buf);
					break;
			case 7:		strcpy(ServInfo->serv_sysadm,buf);
					break;
			case 9:		strcpy(ServInfo->serv_moreprompt,buf);
					break;
			case 10:	ServInfo->serv_ok_floors = atoi(buf);
					break;
			case 11:	ServInfo->serv_paging_level = atoi(buf);
					break;
			case 13:	ServInfo->serv_supports_qnop = atoi(buf);
					break;
			}
		}

	}
	return ret;
}


/* RDIR */
int CtdlIPCReadDirectory(CtdlIPC *ipc, char **listing, char *cret)
{
	register int ret;
	size_t bytes;

	if (!cret) return -2;
	if (!listing) return -2;
	if (*listing) return -2;

	ret = CtdlIPCGenericCommand(ipc, "RDIR", NULL, 0, listing, &bytes, cret);
	return ret;
}


/*
 * Set last-read pointer in this room to msgnum, or 0 for HIGHEST.
 */
int CtdlIPCSetLastRead(CtdlIPC *ipc, long msgnum, char *cret)
{
	register int ret;
	char aaa[16];

	if (!cret) return -2;

	if (msgnum)
		sprintf(aaa, "SLRP %ld", msgnum);
	else
		sprintf(aaa, "SLRP HIGHEST");
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
	return ret;
}


/* INVT */
int CtdlIPCInviteUserToRoom(CtdlIPC *ipc, const char *username, char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -2;
	if (!username) return -2;

	aaa = (char *)malloc(strlen(username) + 6);
	if (!aaa) return -1;

	sprintf(aaa, "INVT %s", username);
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/* KICK */
int CtdlIPCKickoutUserFromRoom(CtdlIPC *ipc, const char *username, char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -1;
	if (!username) return -1;

	aaa = (char *)malloc(strlen(username) + 6);

	sprintf(aaa, "KICK %s", username);
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/* GETR */
int CtdlIPCGetRoomAttributes(CtdlIPC *ipc, struct quickroom **qret, char *cret)
{
	register int ret;

	if (!cret) return -2;
	if (!qret) return -2;
	if (!*qret) *qret = (struct quickroom *)calloc(1, sizeof (struct quickroom));
	if (!*qret) return -1;

	ret = CtdlIPCGenericCommand(ipc, "GETR", NULL, 0, NULL, NULL, cret);
	if (ret / 100 == 2) {
		extract(qret[0]->QRname, cret, 0);
		extract(qret[0]->QRpasswd, cret, 1);
		extract(qret[0]->QRdirname, cret, 2);
		qret[0]->QRflags = extract_int(cret, 3);
		qret[0]->QRfloor = extract_int(cret, 4);
		qret[0]->QRorder = extract_int(cret, 5);
	}
	return ret;
}


/* SETR */
/* set forget to kick all users out of room */
int CtdlIPCSetRoomAttributes(CtdlIPC *ipc, int forget, struct quickroom *qret, char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -2;
	if (!qret) return -2;

	aaa = (char *)malloc(strlen(qret->QRname) + strlen(qret->QRpasswd) +
			strlen(qret->QRdirname) + 52);
	if (!aaa) return -1;

	sprintf(aaa, "SETR %s|%s|%s|%d|%d|%d|%d",
			qret->QRname, qret->QRpasswd, qret->QRdirname,
			qret->QRflags, forget, qret->QRfloor, qret->QRorder);
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/* GETA */
int CtdlIPCGetRoomAide(CtdlIPC *ipc, char *cret)
{
	if (!cret) return -1;

	return CtdlIPCGenericCommand(ipc, "GETA", NULL, 0, NULL, NULL, cret);
}


/* SETA */
int CtdlIPCSetRoomAide(CtdlIPC *ipc, const char *username, char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -2;
	if (!username) return -2;

	aaa = (char *)malloc(strlen(username) + 6);
	if (!aaa) return -1;

	sprintf(aaa, "SETA %s", username);
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/* ENT0 */
int CtdlIPCPostMessage(CtdlIPC *ipc, int flag, const struct ctdlipcmessage *mr, char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -2;
	if (!mr) return -2;

	aaa = (char *)malloc(strlen(mr->recipient) + strlen(mr->author) + 40);
	if (!aaa) return -1;

	sprintf(aaa, "ENT0 %d|%s|%d|%d|%s", flag, mr->recipient, mr->anonymous,
			mr->type, mr->author);
	ret = CtdlIPCGenericCommand(ipc, aaa, mr->text, strlen(mr->text), NULL,
			NULL, cret);
	free(aaa);
	return ret;
}


/* RINF */
int CtdlIPCRoomInfo(CtdlIPC *ipc, char **iret, char *cret)
{
	size_t bytes;

	if (!cret) return -2;
	if (!iret) return -2;
	if (*iret) return -2;

	return CtdlIPCGenericCommand(ipc, "RINF", NULL, 0, iret, &bytes, cret);
}


/* DELE */
int CtdlIPCDeleteMessage(CtdlIPC *ipc, long msgnum, char *cret)
{
	char aaa[16];

	if (!cret) return -2;
	if (!msgnum) return -2;

	sprintf(aaa, "DELE %ld", msgnum);
	return CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
}


/* MOVE */
int CtdlIPCMoveMessage(CtdlIPC *ipc, int copy, long msgnum, const char *destroom, char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -2;
	if (!destroom) return -2;
	if (!msgnum) return -2;

	aaa = (char *)malloc(strlen(destroom) + 28);
	if (!aaa) return -1;

	sprintf(aaa, "MOVE %ld|%s|%d", msgnum, destroom, copy);
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/* KILL */
int CtdlIPCDeleteRoom(CtdlIPC *ipc, int for_real, char *cret)
{
	char aaa[16];

	if (!cret) return -2;

	sprintf(aaa, "KILL %d", for_real);
	return CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
}


/* CRE8 */
int CtdlIPCCreateRoom(CtdlIPC *ipc, int for_real, const char *roomname, int type,
		const char *password, int floor, char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -2;
	if (!roomname) return -2;

	if (password) {
		aaa = (char *)malloc(strlen(roomname) + strlen(password) + 40);
		if (!aaa) return -1;
		sprintf(aaa, "CRE8 %d|%s|%d|%s|%d", for_real, roomname, type,
				password, floor);
	} else {
		aaa = (char *)malloc(strlen(roomname) + 40);
		if (!aaa) return -1;
		sprintf(aaa, "CRE8 %d|%s|%d||%d", for_real, roomname, type,
				floor);
	}
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/* FORG */
int CtdlIPCForgetRoom(CtdlIPC *ipc, char *cret)
{
	if (!cret) return -2;

	return CtdlIPCGenericCommand(ipc, "FORG", NULL, 0, NULL, NULL, cret);
}


/* MESG */
int CtdlIPCSystemMessage(CtdlIPC *ipc, const char *message, char **mret, char *cret)
{
	register int ret;
	char *aaa;
	size_t bytes;

	if (!cret) return -2;
	if (!mret) return -2;
	if (*mret) return -2;
	if (!message) return -2;

	aaa = (char *)malloc(strlen(message) + 6);
	if (!aaa) return -1;

	sprintf(aaa, "MESG %s", message);
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, mret, &bytes, cret);
	free(aaa);
	return ret;
}


/* GNUR */
int CtdlIPCNextUnvalidatedUser(CtdlIPC *ipc, char *cret)
{
	if (!cret) return -2;

	return CtdlIPCGenericCommand(ipc, "GNUR", NULL, 0, NULL, NULL, cret);
}


/* GREG */
int CtdlIPCGetUserRegistration(CtdlIPC *ipc, const char *username, char **rret, char *cret)
{
	register int ret;
	char *aaa;
	size_t bytes;

	if (!cret) return -2;
	if (!rret) return -2;
	if (*rret) return -2;

	if (username)
		aaa = (char *)malloc(strlen(username) + 6);
	else
		aaa = (char *)malloc(12);
	if (!aaa) return -1;

	if (username)
		sprintf(aaa, "GREG %s", username);
	else
		sprintf(aaa, "GREG _SELF_");
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, rret, &bytes, cret);
	free(aaa);
	return ret;
}


/* VALI */
int CtdlIPCValidateUser(CtdlIPC *ipc, const char *username, int axlevel, char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -2;
	if (!username) return -2;
	if (axlevel < 0 || axlevel > 7) return -2;

	aaa = (char *)malloc(strlen(username) + 17);
	if (!aaa) return -1;

	sprintf(aaa, "VALI %s|%d", username, axlevel);
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/* EINF */
int CtdlIPCSetRoomInfo(CtdlIPC *ipc, int for_real, const char *info, char *cret)
{
	char aaa[16];

	if (!cret) return -1;
	if (!info) return -1;

	sprintf(aaa, "EINF %d", for_real);
	return CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
}


/* LIST */
int CtdlIPCUserListing(CtdlIPC *ipc, char **listing, char *cret)
{
	size_t bytes;

	if (!cret) return -1;
	if (!listing) return -1;
	if (*listing) return -1;

	return CtdlIPCGenericCommand(ipc, "LIST", NULL, 0, listing, &bytes, cret);
}


/* REGI */
int CtdlIPCSetRegistration(CtdlIPC *ipc, const char *info, char *cret)
{
	if (!cret) return -1;
	if (!info) return -1;

	return CtdlIPCGenericCommand(ipc, "REGI", info, strlen(info),
			NULL, NULL, cret);
}


/* CHEK */
int CtdlIPCMiscCheck(CtdlIPC *ipc, struct ctdlipcmisc *chek, char *cret)
{
	register int ret;

	if (!cret) return -1;
	if (!chek) return -1;

	ret = CtdlIPCGenericCommand(ipc, "CHEK", NULL, 0, NULL, NULL, cret);
	if (ret / 100 == 2) {
		chek->newmail = extract_long(cret, 0);
		chek->needregis = extract_int(cret, 1);
		chek->needvalid = extract_int(cret, 2);
	}
	return ret;
}


/* DELF */
int CtdlIPCDeleteFile(CtdlIPC *ipc, const char *filename, char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -2;
	if (!filename) return -2;
	
	aaa = (char *)malloc(strlen(filename) + 6);
	if (!aaa) return -1;

	sprintf(aaa, "DELF %s", filename);
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/* MOVF */
int CtdlIPCMoveFile(CtdlIPC *ipc, const char *filename, const char *destroom, char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -2;
	if (!filename) return -2;
	if (!destroom) return -2;

	aaa = (char *)malloc(strlen(filename) + strlen(destroom) + 7);
	if (!aaa) return -1;

	sprintf(aaa, "MOVF %s|%s", filename, destroom);
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/* NETF */
int CtdlIPCNetSendFile(CtdlIPC *ipc, const char *filename, const char *destnode, char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -2;
	if (!filename) return -2;
	if (!destnode) return -2;

	aaa = (char *)malloc(strlen(filename) + strlen(destnode) + 7);
	if (!aaa) return -1;

	sprintf(aaa, "NETF %s|%s", filename, destnode);
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/* RWHO */
int CtdlIPCOnlineUsers(CtdlIPC *ipc, char **listing, time_t *stamp, char *cret)
{
	register int ret;
	size_t bytes;

	if (!cret) return -1;
	if (!listing) return -1;
	if (*listing) return -1;

	*stamp = CtdlIPCServerTime(ipc, cret);
	if (!*stamp)
		*stamp = time(NULL);
	ret = CtdlIPCGenericCommand(ipc, "RWHO", NULL, 0, listing, &bytes, cret);
	return ret;
}


/* OPEN */
int CtdlIPCFileDownload(CtdlIPC *ipc, const char *filename, void **buf,
		void (*progress_gauge_callback)(long, long), char *cret)
{
	register int ret;
	size_t bytes;
	time_t last_mod;
	char mimetype[SIZ];
	char *aaa;

	if (!cret) return -2;
	if (!filename) return -2;
	if (!buf) return -2;
	if (*buf) return -2;
	if (ipc->downloading) return -2;

	aaa = (char *)malloc(strlen(filename) + 6);
	if (!aaa) return -1;

	sprintf(aaa, "OPEN %s", filename);
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	if (ret / 100 == 2) {
		ipc->downloading = 1;
		bytes = extract_long(cret, 0);
		last_mod = extract_int(cret, 1);
		extract(mimetype, cret, 2);
		ret = CtdlIPCReadDownload(ipc, buf, bytes, progress_gauge_callback, cret);
/*		ret = CtdlIPCHighSpeedReadDownload(ipc, buf, bytes, progress_gauge_callback, cret); */
		ret = CtdlIPCEndDownload(ipc, cret);
		if (ret / 100 == 2)
			sprintf(cret, "%d|%ld|%s|%s", bytes, last_mod,
					filename, mimetype);
	}
	return ret;
}


/* OPNA */
int CtdlIPCAttachmentDownload(CtdlIPC *ipc, long msgnum, const char *part, void **buf,
		void (*progress_gauge_callback)(long, long), char *cret)
{
	register int ret;
	size_t bytes;
	time_t last_mod;
	char filename[SIZ];
	char mimetype[SIZ];
	char *aaa;

	if (!cret) return -2;
	if (!buf) return -2;
	if (*buf) return -2;
	if (!part) return -2;
	if (!msgnum) return -2;
	if (ipc->downloading) return -2;

	aaa = (char *)malloc(strlen(part) + 17);
	if (!aaa) return -1;

	sprintf(aaa, "OPNA %ld|%s", msgnum, part);
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	if (ret / 100 == 2) {
		ipc->downloading = 1;
		bytes = extract_long(cret, 0);
		last_mod = extract_int(cret, 1);
		extract(mimetype, cret, 2);
		ret = CtdlIPCHighSpeedReadDownload(ipc, buf, bytes, progress_gauge_callback, cret);
		ret = CtdlIPCEndDownload(ipc, cret);
		if (ret / 100 == 2)
			sprintf(cret, "%d|%ld|%s|%s", bytes, last_mod,
					filename, mimetype);
	}
	return ret;
}


/* OIMG */
int CtdlIPCImageDownload(CtdlIPC *ipc, const char *filename, void **buf,
		void (*progress_gauge_callback)(long, long), char *cret)
{
	register int ret;
	size_t bytes;
	time_t last_mod;
	char mimetype[SIZ];
	char *aaa;

	if (!cret) return -1;
	if (!buf) return -1;
	if (*buf) return -1;
	if (!filename) return -1;
	if (ipc->downloading) return -1;

	aaa = (char *)malloc(strlen(filename) + 6);
	if (!aaa) return -1;

	sprintf(aaa, "OIMG %s", filename);
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	if (ret / 100 == 2) {
		ipc->downloading = 1;
		bytes = extract_long(cret, 0);
		last_mod = extract_int(cret, 1);
		extract(mimetype, cret, 2);
		ret = CtdlIPCReadDownload(ipc, buf, bytes, progress_gauge_callback, cret);
		ret = CtdlIPCEndDownload(ipc, cret);
		if (ret / 100 == 2)
			sprintf(cret, "%d|%ld|%s|%s", bytes, last_mod,
					filename, mimetype);
	}
	return ret;
}


/* UOPN */
int CtdlIPCFileUpload(CtdlIPC *ipc, const char *filename, const char *comment, void *buf,
		size_t bytes, char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -1;
	if (!filename) return -1;
	if (!comment) return -1;
	if (ipc->uploading) return -1;

	aaa = (char *)malloc(strlen(filename) + strlen(comment) + 7);
	if (!aaa) return -1;

	sprintf(aaa, "UOPN %s|%s", filename, comment);
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	if (ret / 100 == 2)
		ipc->uploading = 1;
	ret = CtdlIPCWriteUpload(ipc, buf, bytes, cret);
	ret = CtdlIPCEndUpload(ipc, cret);
	return ret;
}


/* UIMG */
int CtdlIPCImageUpload(CtdlIPC *ipc, int for_real, const char *filename, size_t bytes,
		char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -1;
	if (!filename) return -1;
	if (ipc->uploading) return -1;

	aaa = (char *)malloc(strlen(filename) + 17);
	if (!aaa) return -1;

	sprintf(aaa, "UIMG %d|%s", for_real, filename);
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	if (ret / 100 == 2)
		ipc->uploading = 1;
	return ret;
}


/* QUSR */
int CtdlIPCQueryUsername(CtdlIPC *ipc, const char *username, char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -2;
	if (!username) return -2;

	aaa = (char *)malloc(strlen(username) + 6);
	if (!aaa) return -1;

	sprintf(aaa, "QUSR %s", username);
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/* LFLR */
int CtdlIPCFloorListing(CtdlIPC *ipc, char **listing, char *cret)
{
	size_t bytes;

	if (!cret) return -2;
	if (!listing) return -2;
	if (*listing) return -2;

	return CtdlIPCGenericCommand(ipc, "LFLR", NULL, 0, listing, &bytes, cret);
}


/* CFLR */
int CtdlIPCCreateFloor(CtdlIPC *ipc, int for_real, const char *name, char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -2;
	if (!name) return -2;

	aaa = (char *)malloc(strlen(name) + 17);
	if (!aaa) return -1;

	sprintf(aaa, "CFLR %s|%d", name, for_real);
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/* KFLR */
int CtdlIPCDeleteFloor(CtdlIPC *ipc, int for_real, int floornum, char *cret)
{
	char aaa[27];

	if (!cret) return -1;
	if (floornum < 0) return -1;

	sprintf(aaa, "KFLR %d|%d", floornum, for_real);
	return CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
}


/* EFLR */
int CtdlIPCEditFloor(CtdlIPC *ipc, int floornum, const char *floorname, char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -2;
	if (!floorname) return -2;
	if (floornum < 0) return -2;

	aaa = (char *)malloc(strlen(floorname) + 17);
	if (!aaa) return -1;

	sprintf(aaa, "EFLR %d|%s", floornum, floorname);
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/* IDEN */
int CtdlIPCIdentifySoftware(CtdlIPC *ipc, int developerid, int clientid, int revision,
		const char *software_name, const char *hostname, char *cret)
{
	register int ret;
	char *aaa;

	if (developerid < 0) return -2;
	if (clientid < 0) return -2;
	if (revision < 0) return -2;
	if (!software_name) return -2;
	if (!hostname) return -2;

	aaa = (char *)malloc(strlen(software_name) + strlen(hostname) + 29);
	if (!aaa) return -1;

	sprintf(aaa, "IDEN %d|%d|%d|%s|%s", developerid, clientid,
			revision, software_name, hostname);
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/* SEXP */
int CtdlIPCSendInstantMessage(CtdlIPC *ipc, const char *username, const char *text,
		char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -2;
	if (!username) return -2;

	aaa = (char *)malloc(strlen(username) + 8);
	if (!aaa) return -1;

	if (text) {
		sprintf(aaa, "SEXP %s|-", username);
		ret = CtdlIPCGenericCommand(ipc, aaa, text, strlen(text),
				NULL, NULL, cret);
	} else {
		sprintf(aaa, "SEXP %s||", username);
		ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
	}
	free(aaa);
	return ret;
}


/* GEXP */
int CtdlIPCGetInstantMessage(CtdlIPC *ipc, char **listing, char *cret)
{
	size_t bytes;

	if (!cret) return -2;
	if (!listing) return -2;
	if (*listing) return -2;

	return CtdlIPCGenericCommand(ipc, "GEXP", NULL, 0, listing, &bytes, cret);
}


/* DEXP */
/* mode is 0 = enable, 1 = disable, 2 = status */
int CtdlIPCEnableInstantMessageReceipt(CtdlIPC *ipc, int mode, char *cret)
{
	char aaa[16];

	if (!cret) return -2;

	sprintf(aaa, "DEXP %d", mode);
	return CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
}


/* EBIO */
int CtdlIPCSetBio(CtdlIPC *ipc, char *bio, char *cret)
{
	if (!cret) return -2;
	if (!bio) return -2;

	return CtdlIPCGenericCommand(ipc, "EBIO", bio, strlen(bio),
			NULL, NULL, cret);
}


/* RBIO */
int CtdlIPCGetBio(CtdlIPC *ipc, const char *username, char **listing, char *cret)
{
	register int ret;
	size_t bytes;
	char *aaa;

	if (!cret) return -2;
	if (!username) return -2;
	if (!listing) return -2;
	if (*listing) return -2;

	aaa = (char *)malloc(strlen(username) + 6);
	if (!aaa) return -1;

	sprintf(aaa, "RBIO %s", username);
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, listing, &bytes, cret);
	free(aaa);
	return ret;
}


/* LBIO */
int CtdlIPCListUsersWithBios(CtdlIPC *ipc, char **listing, char *cret)
{
	size_t bytes;

	if (!cret) return -2;
	if (!listing) return -2;
	if (*listing) return -2;

	return CtdlIPCGenericCommand(ipc, "LBIO", NULL, 0, listing, &bytes, cret);
}


/* STEL */
int CtdlIPCStealthMode(CtdlIPC *ipc, int mode, char *cret)
{
	char aaa[16];

	if (!cret) return -1;

	sprintf(aaa, "STEL %d", mode ? 1 : 0);
	return CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
}


/* TERM */
int CtdlIPCTerminateSession(CtdlIPC *ipc, int sid, char *cret)
{
	char aaa[16];

	if (!cret) return -1;

	sprintf(aaa, "TERM %d", sid);
	return CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
}


/* DOWN */
int CtdlIPCTerminateServerNow(CtdlIPC *ipc, char *cret)
{
	if (!cret) return -1;

	return CtdlIPCGenericCommand(ipc, "DOWN", NULL, 0, NULL, NULL, cret);
}


/* SCDN */
int CtdlIPCTerminateServerScheduled(CtdlIPC *ipc, int mode, char *cret)
{
	char aaa[16];

	if (!cret) return -1;

	sprintf(aaa, "SCDN %d", mode ? 1 : 0);
	return CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
}


/* EMSG */
int CtdlIPCEnterSystemMessage(CtdlIPC *ipc, const char *filename, const char *text,
		char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -2;
	if (!text) return -2;
	if (!filename) return -2;

	aaa = (char *)malloc(strlen(filename) + 6);
	if (!aaa) return -1;

	sprintf(aaa, "EMSG %s", filename);
	ret = CtdlIPCGenericCommand(ipc, aaa, text, strlen(text), NULL, NULL, cret);
	free(aaa);
	return ret;
}


/* HCHG */
int CtdlIPCChangeHostname(CtdlIPC *ipc, const char *hostname, char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -2;
	if (!hostname) return -2;

	aaa = (char *)malloc(strlen(hostname) + 6);
	if (!aaa) return -1;

	sprintf(aaa, "HCHG %s", hostname);
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/* RCHG */
int CtdlIPCChangeRoomname(CtdlIPC *ipc, const char *roomname, char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -2;
	if (!roomname) return -2;

	aaa = (char *)malloc(strlen(roomname) + 6);
	if (!aaa) return -1;

	sprintf(aaa, "RCHG %s", roomname);
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/* UCHG */
int CtdlIPCChangeUsername(CtdlIPC *ipc, const char *username, char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -2;
	if (!username) return -2;

	aaa = (char *)malloc(strlen(username) + 6);
	if (!aaa) return -1;

	sprintf(aaa, "UCHG %s", username);
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/* TIME */
/* This function returns the actual server time reported, or 0 if error */
time_t CtdlIPCServerTime(CtdlIPC *ipc, char *cret)
{
	register time_t tret;
	register int ret;

	ret = CtdlIPCGenericCommand(ipc, "TIME", NULL, 0, NULL, NULL, cret);
	if (ret / 100 == 2) {
		tret = extract_long(cret, 0);
	} else {
		tret = 0L;
	}
	return tret;
}


/* AGUP */
int CtdlIPCAideGetUserParameters(CtdlIPC *ipc, const char *who,
				 struct usersupp **uret, char *cret)
{
	register int ret;
	char aaa[SIZ];

	if (!cret) return -2;
	if (!uret) return -2;
	if (!*uret) *uret = (struct usersupp *)calloc(1, sizeof(struct usersupp));
	if (!*uret) return -1;

	sprintf(aaa, "AGUP %s", who);
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);

	if (ret / 100 == 2) {
		extract(uret[0]->fullname, cret, 0);
		extract(uret[0]->password, cret, 1);
		uret[0]->flags = extract_int(cret, 2);
		uret[0]->timescalled = extract_long(cret, 3);
		uret[0]->posted = extract_long(cret, 4);
		uret[0]->axlevel = extract_int(cret, 5);
		uret[0]->usernum = extract_long(cret, 6);
		uret[0]->lastcall = extract_long(cret, 7);
		uret[0]->USuserpurge = extract_int(cret, 8);
	}
	return ret;
}


/* ASUP */
int CtdlIPCAideSetUserParameters(CtdlIPC *ipc, const struct usersupp *uret, char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -2;
	if (!uret) return -2;

	aaa = (char *)malloc(strlen(uret->fullname) + strlen(uret->password) + 84);
	if (!aaa) return -1;

	sprintf(aaa, "ASUP %s|%s|%d|%ld|%ld|%d|%ld|%ld|%d",
			uret->fullname, uret->password, uret->flags,
			uret->timescalled, uret->posted, uret->axlevel,
			uret->usernum, uret->lastcall, uret->USuserpurge);
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/* GPEX */
/* which is 0 = room, 1 = floor, 2 = site */
int CtdlIPCGetMessageExpirationPolicy(CtdlIPC *ipc, int which, char *cret)
{
	static char *proto[] = {"room", "floor", "site"};
	char aaa[11];

	if (!cret) return -2;
	if (which < 0 || which > 2) return -2;
	
	sprintf(aaa, "GPEX %s", proto[which]);
	return CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
}


/* SPEX */
/* which is 0 = room, 1 = floor, 2 = site */
/* policy is 0 = inherit, 1 = no purge, 2 = by count, 3 = by age (days) */
int CtdlIPCSetMessageExpirationPolicy(CtdlIPC *ipc, int which, int policy, int value,
		char *cret)
{
	char aaa[38];

	if (!cret) return -2;
	if (which < 0 || which > 2) return -2;
	if (policy < 0 || policy > 3) return -2;
	if (policy >= 2 && value < 1) return -2;

	sprintf(aaa, "SPEX %d|%d|%d", which, policy, value);
	return CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
}


/* CONF GET */
int CtdlGetSystemConfig(CtdlIPC *ipc, char **listing, char *cret)
{
	size_t bytes;

	if (!cret) return -2;
	if (!listing) return -2;
	if (*listing) return -2;

	return CtdlIPCGenericCommand(ipc, "CONF GET", NULL, 0,
			listing, &bytes, cret);
}


/* CONF SET */
int CtdlSetSystemConfig(CtdlIPC *ipc, const char *listing, char *cret)
{
	if (!cret) return -2;
	if (!listing) return -2;

	return CtdlIPCGenericCommand(ipc, "CONF SET", listing, strlen(listing),
			NULL, NULL, cret);
}


/* CONF GETSYS */
int CtdlGetSystemConfigByType(CtdlIPC *ipc, const char *mimetype,
	       	char **listing, char *cret)
{
	char *aaa;
	size_t bytes;

	if (!cret) return -2;
	if (!mimetype) return -2;
	if (!listing) return -2;
	if (*listing) return -2;

	aaa = malloc(strlen(mimetype) + 13);
	if (!aaa) return -1;
	sprintf(aaa, "CONF GETSYS|%s", mimetype);
	return CtdlIPCGenericCommand(ipc, aaa, NULL, 0,
			listing, &bytes, cret);
}


/* CONF PUTSYS */
int CtdlSetSystemConfigByType(CtdlIPC *ipc, const char *mimetype,
	       const char *listing, char *cret)
{
	char *aaa;

	if (!cret) return -2;
	if (!mimetype) return -2;
	if (!listing) return -2;

	aaa = malloc(strlen(mimetype) + 13);
	if (!aaa) return -1;
	sprintf(aaa, "CONF PUTSYS|%s", mimetype);
	return CtdlIPCGenericCommand(ipc, aaa, listing, strlen(listing),
			NULL, NULL, cret);
}

/* MMOD */
int CtdlIPCModerateMessage(CtdlIPC *ipc, long msgnum, int level, char *cret)
{
	char aaa[27];

	if (!cret) return -2;
	if (!msgnum) return -2;

	sprintf(aaa, "MMOD %ld|%d", msgnum, level);
	return CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
}


/* REQT */
int CtdlIPCRequestClientLogout(CtdlIPC *ipc, int session, char *cret)
{
	char aaa[16];

	if (!cret) return -2;
	if (session < 0) return -2;

	sprintf(aaa, "REQT %d", session);
	return CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
}


/* SEEN */
int CtdlIPCSetMessageSeen(CtdlIPC *ipc, long msgnum, int seen, char *cret)
{
	char aaa[27];

	if (!cret) return -2;
	if (msgnum < 0) return -2;

	sprintf(aaa, "SEEN %ld|%d", msgnum, seen ? 1 : 0);
	return CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
}


/* STLS */
int CtdlIPCStartEncryption(CtdlIPC *ipc, char *cret)
{
	return CtdlIPCGenericCommand(ipc, "STLS", NULL, 0, NULL, NULL, cret);
}


/* QDIR */
int CtdlIPCDirectoryLookup(CtdlIPC *ipc, const char *address, char *cret)
{
	char *aaa;

	if (!address) return -2;
	if (!cret) return -2;

	aaa = (char *)malloc(strlen(address) + 6);
	if (!aaa) return -1;

	sprintf(aaa, "QDIR %s", address);
	return CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
}


/* IPGM */
int CtdlIPCInternalProgram(CtdlIPC *ipc, int secret, char *cret)
{
	char aaa[30];

	if (!cret) return -2;
	sprintf(aaa, "IPGM %d", secret);
	return CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
}


/*
 * Not implemented:
 * 
 * CHAT
 * ETLS
 * EXPI
 * GTLS
 * IGAB
 * MSG3
 * MSG4
 * NDOP
 * NETP
 * NUOP
 * SMTP
 */


/* ************************************************************************** */
/*             Stuff below this line is not for public consumption            */
/* ************************************************************************** */


inline void CtdlIPC_lock(CtdlIPC *ipc)
{
#ifdef THREADED_CLIENT
	pthread_mutex_lock(&(ipc->mutex));
#endif
}


inline void CtdlIPC_unlock(CtdlIPC *ipc)
{
#ifdef THREADED_CLIENT
	pthread_mutex_unlock(&(ipc->mutex));
#endif
}


/* Read a listing from the server up to 000.  Append to dest if it exists */
char *CtdlIPCReadListing(CtdlIPC *ipc, char *dest)
{
	size_t length = 0;
	size_t linelength;
	char *ret;
	char aaa[SIZ];

	ret = dest;
	if (ret != NULL) {
		length = strlen(ret);
	}
	else {
		ret = strdup("");
		length = 0;
	}

	while (CtdlIPC_getline(ipc, aaa), strcmp(aaa, "000")) {
		linelength = strlen(aaa);
		ret = (char *)realloc(ret, (size_t)(length + linelength + 2));
		if (ret) {
			strcpy(&ret[length], aaa);
			length += linelength;
			strcpy(&ret[length++], "\n");
		}
	}

	return(ret);
}


/* Send a listing to the server; generate the ending 000. */
int CtdlIPCSendListing(CtdlIPC *ipc, const char *listing)
{
	char *text;

	text = (char *)malloc(strlen(listing) + 6);
	if (text) {
		strcpy(text, listing);
		while (text[strlen(text) - 1] == '\n')
			text[strlen(text) - 1] = '\0';
		strcat(text, "\n000");
		CtdlIPC_putline(ipc, text);
		free(text);
		text = NULL;
	} else {
		/* Malloc failed but we are committed to send */
		/* This may result in extra blanks at the bottom */
		CtdlIPC_putline(ipc, text);
		CtdlIPC_putline(ipc, "000");
	}
	return 0;
}


/* Partial read of file from server */
size_t CtdlIPCPartialRead(CtdlIPC *ipc, void **buf, size_t offset, size_t bytes, char *cret)
{
	register size_t len = 0;
	char aaa[SIZ];

	if (!buf) return -1;
	if (!cret) return -1;
	if (bytes < 1) return -1;
	if (offset < 0) return -1;

	CtdlIPC_lock(ipc);
	sprintf(aaa, "READ %d|%d", offset, bytes);
	CtdlIPC_putline(ipc, aaa);
	CtdlIPC_getline(ipc, aaa);
	if (aaa[0] != '6')
		strcpy(cret, &aaa[4]);
	else {
		len = extract_long(&aaa[4], 0);
		*buf = (void *)realloc(*buf, (size_t)(offset + len));
		if (*buf) {
			/* I know what I'm doing */
			serv_read(ipc, (*buf + offset), len);
		} else {
			/* We have to read regardless */
			serv_read(ipc, aaa, len);
			len = -1;
		}
	}
	CtdlIPC_unlock(ipc);
	return len;
}


/* CLOS */
int CtdlIPCEndDownload(CtdlIPC *ipc, char *cret)
{
	register int ret;

	if (!cret) return -2;
	if (!ipc->downloading) return -2;

	ret = CtdlIPCGenericCommand(ipc, "CLOS", NULL, 0, NULL, NULL, cret);
	if (ret / 100 == 2)
		ipc->downloading = 0;
	return ret;
}


/* MSGP */
int CtdlIPCSpecifyPreferredFormats(CtdlIPC *ipc, char *cret, char *formats) {
	register int ret;
	char cmd[SIZ];
	
	snprintf(cmd, sizeof cmd, "MSGP %s", formats);
	ret = CtdlIPCGenericCommand(ipc, cmd, NULL, 0, NULL, NULL, cret);
	return ret;
}



/* READ */
int CtdlIPCReadDownload(CtdlIPC *ipc, void **buf, size_t bytes,
	       void (*progress_gauge_callback)(long, long), char *cret)
{
	register size_t len;

	if (!cret) return -1;
	if (!buf) return -1;
	if (*buf) return -1;
	if (!ipc->downloading) return -1;

	len = 0;
	if (progress_gauge_callback)
		progress_gauge_callback(len, bytes);
	while (len < bytes) {
		register size_t block;

		block = CtdlIPCPartialRead(ipc, buf, len, 4096, cret);
		if (block == -1) {
			free(*buf);
			return 0;
		}
		len += block;
		if (progress_gauge_callback)
			progress_gauge_callback(len, bytes);
	}
	return len;
}


/* READ - pipelined */
int CtdlIPCHighSpeedReadDownload(CtdlIPC *ipc, void **buf, size_t bytes,
	       void (*progress_gauge_callback)(long, long), char *cret)
{
	register size_t len;
	register int calls;	/* How many calls in the pipeline */
	register int i;		/* iterator */
	char aaa[4096];

	if (!cret) return -1;
	if (!buf) return -1;
	if (*buf) return -1;
	if (!ipc->downloading) return -1;

	*buf = (void *)realloc(*buf, bytes);
	if (!*buf) return -1;

	len = 0;
	CtdlIPC_lock(ipc);
	if (progress_gauge_callback)
		progress_gauge_callback(len, bytes);

	/* How many calls will be in the pipeline? */
	calls = bytes / 4096;
	if (bytes % 4096) calls++;

	/* Send all requests at once */
	for (i = 0; i < calls; i++) {
		sprintf(aaa, "READ %d|4096", i * 4096);
		CtdlIPC_putline(ipc, aaa);
	}

	/* Receive all responses at once */
	for (i = 0; i < calls; i++) {
		CtdlIPC_getline(ipc, aaa);
		if (aaa[0] != '6')
			strcpy(cret, &aaa[4]);
		else {
			len = extract_long(&aaa[4], 0);
			/* I know what I'm doing */
			serv_read(ipc, ((*buf) + (i * 4096)), len);
		}
		if (progress_gauge_callback)
			progress_gauge_callback(i * 4096 + len, bytes);
	}
	CtdlIPC_unlock(ipc);
	return len;
}


/* UCLS */
int CtdlIPCEndUpload(CtdlIPC *ipc, char *cret)
{
	register int ret;

	if (!cret) return -1;
	if (!ipc->uploading) return -1;

	ret = CtdlIPCGenericCommand(ipc, "UCLS", NULL, 0, NULL, NULL, cret);
	if (ret / 100 == 2)
		ipc->uploading = 0;
	return ret;
}


/* WRIT */
int CtdlIPCWriteUpload(CtdlIPC *ipc, void *buf, size_t bytes, char *cret)
{
	register int ret = -1;
	register size_t offset;
	char aaa[SIZ];

	if (!cret) return -1;
	if (!buf) return -1;
	if (bytes < 1) return -1;

	offset = 0;
	while (offset < bytes) {
		sprintf(aaa, "WRIT %d", bytes - offset);
		CtdlIPC_putline(ipc, aaa);
		CtdlIPC_getline(ipc, aaa);
		strcpy(cret, &aaa[4]);
		ret = atoi(aaa);
		if (aaa[0] == '7') {
			register size_t to_write;

			to_write = extract_long(&aaa[4], 0);
			serv_write(ipc, buf + offset, to_write);
			offset += to_write;
		} else {
			break;
		}
	}
	return ret;
}


/*
 * Generic command method.  This method should handle any server command
 * except for CHAT.  It takes the following arguments:
 *
 * ipc			The server to speak with
 * command		Preformatted command to send to server
 * to_send		A text or binary file to send to server
 * 			(only sent if server requests it)
 * bytes_to_send	The number of bytes in to_send (required if
 * 			sending binary, optional if sending listing)
 * to_receive		Pointer to a NULL pointer, if the server
 * 			sends text or binary we will allocate memory
 * 			for the file and stuff it here
 * bytes_to_receive	If a file is received, we will store its
 * 			byte count here
 * proto_response	The protocol response.  Caller must provide
 * 			this buffer and ensure that it is at least
 * 			128 bytes in length.
 *
 * This function returns a number equal to the protocol response number,
 * -1 if an internal error occurred, -2 if caller provided bad values,
 * or 0 - the protocol response number if bad values were found during
 * the protocol exchange.
 * It stores the protocol response string (minus the number) in 
 * protocol_response as described above.  Some commands send additional
 * data in this string.
 */
int CtdlIPCGenericCommand(CtdlIPC *ipc,
		const char *command, const char *to_send,
		size_t bytes_to_send, char **to_receive, 
		size_t *bytes_to_receive, char *proto_response)
{
	char buf[SIZ];
	register int ret;
	int watch_ssl = 0;

	if (!command) return -2;
	if (!proto_response) return -2;

	if (ipc->ssl) watch_ssl = 1;

	CtdlIPC_lock(ipc);
	CtdlIPC_putline(ipc, command);
	while (1) {
		CtdlIPC_getline(ipc, proto_response);
		if (proto_response[3] == '*')
			express_msgs = 1;
		ret = atoi(proto_response);
		strcpy(proto_response, &proto_response[4]);
		switch (ret / 100) {
		default:			/* Unknown, punt */
		case 2:				/* OK */
		case 3:				/* MORE_DATA */
		case 5:				/* ERROR */
			/* Don't need to do anything */
			break;
		case 1:				/* LISTING_FOLLOWS */
			if (to_receive && !*to_receive && bytes_to_receive) {
				*to_receive = CtdlIPCReadListing(ipc, NULL);
			} else { /* Drain */
				while (CtdlIPC_getline(ipc, buf), strcmp(buf, "000")) ;
				ret = -ret;
			}
			break;
		case 4:				/* SEND_LISTING */
			if (to_send) {
				CtdlIPCSendListing(ipc, to_send);
			} else {
				/* No listing given, fake it */
				CtdlIPC_putline(ipc, "000");
				ret = -ret;
			}
			break;
		case 6:				/* BINARY_FOLLOWS */
			if (to_receive && !*to_receive && bytes_to_receive) {
				*bytes_to_receive =
					extract_long(proto_response, 0);
				*to_receive = (char *)
					malloc((size_t)*bytes_to_receive);
				if (!*to_receive) {
					ret = -1;
				} else {
					serv_read(ipc, *to_receive,
							*bytes_to_receive);
				}
			} else {
				/* Drain */
				size_t drain;

				drain = extract_long(proto_response, 0);
				while (drain > SIZ) {
					serv_read(ipc, buf, SIZ);
					drain -= SIZ;
				}
				serv_read(ipc, buf, drain);
				ret = -ret;
			}
			break;
		case 7:				/* SEND_BINARY */
			if (to_send && bytes_to_send) {
				serv_write(ipc, to_send, bytes_to_send);
			} else if (bytes_to_send) {
				/* Fake it, send nulls */
				size_t fake;

				fake = bytes_to_send;
				memset(buf, '\0', SIZ);
				while (fake > SIZ) {
					serv_write(ipc, buf, SIZ);
					fake -= SIZ;
				}
				serv_write(ipc, buf, fake);
				ret = -ret;
			} /* else who knows?  DANGER WILL ROBINSON */
			break;
		case 8:				/* START_CHAT_MODE */
			if (!strncasecmp(command, "CHAT", 4)) {
				/* Don't call chatmode with generic! */
				CtdlIPC_putline(ipc, "/quit");
				ret = -ret;
			} else {
				/* In this mode we send then receive listing */
				if (to_send) {
					CtdlIPCSendListing(ipc, to_send);
				} else {
					/* No listing given, fake it */
					CtdlIPC_putline(ipc, "000");
					ret = -ret;
				}
				if (to_receive && !*to_receive
						&& bytes_to_receive) {
					*to_receive = CtdlIPCReadListing(ipc, NULL);
				} else { /* Drain */
					while (CtdlIPC_getline(ipc, buf),
							strcmp(buf, "000")) ;
					ret = -ret;
				}
			}
			break;
		case 9:				/* ASYNC_MSG */
			/* CtdlIPCDoAsync(ret, proto_response); */
			free(CtdlIPCReadListing(ipc, NULL));	/* STUB FIXME */
			break;
		}
		if (ret / 100 != 9)
			break;
	}
	CtdlIPC_unlock(ipc);
	return ret;
}
