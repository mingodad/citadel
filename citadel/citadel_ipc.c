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
#include "client_crypto.h"
#include "tools.h"

#ifdef THREADED_CLIENT
pthread_mutex_t rwlock;
#endif
char express_msgs = 0;

static volatile int download_in_progress = 0;	/* download file open */
static volatile int upload_in_progress = 0;	/* upload file open */
/* static volatile int serv_sock;	/* Socket on which we talk to server */


/*
 * Does nothing.  The server should always return 200.
 */
int CtdlIPCNoop(void)
{
	char aaa[128];

	return CtdlIPCGenericCommand("NOOP", NULL, 0, NULL, NULL, aaa);
}


/*
 * Does nothing interesting.  The server should always return 200
 * along with your string.
 */
int CtdlIPCEcho(const char *arg, char *cret)
{
	register int ret;
	char *aaa;
	
	if (!arg) return -2;
	if (!cret) return -2;

	aaa = (char *)malloc(strlen(arg) + 6);
	if (!aaa) return -1;

	sprintf(aaa, "ECHO %s", arg);
	ret = CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/*
 * Asks the server to close the connecction.
 * Should always return 200.
 */
int CtdlIPCQuit(void)
{
	register int ret;
	char aaa[128];

	netio_lock();
	serv_puts("QUIT");
	serv_gets(aaa);
	ret = atoi(aaa);
	netio_unlock();
	return ret;
}


/*
 * Asks the server to logout.  Should always return 200, even if no user
 * was logged in.  The user will not be logged in after this!
 */
int CtdlIPCLogout(void)
{
	register int ret;
	char aaa[128];

	netio_lock();
	serv_puts("LOUT");
	serv_gets(aaa);
	ret = atoi(aaa);
	netio_unlock();
	return ret;
}


/*
 * First stage of authentication - pass the username.  Returns 300 if the
 * username is able to log in, with the username correctly spelled in cret.
 * Returns various 500 error codes if the user doesn't exist, etc.
 */
int CtdlIPCTryLogin(const char *username, char *cret)
{
	register int ret;
	char *aaa;

	if (!username) return -2;
	if (!cret) return -2;

	aaa = (char *)malloc(strlen(username) + 6);
	if (!aaa) return -1;

	sprintf(aaa, "USER %s", username);
	ret = CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/*
 * Second stage of authentication - provide password.  The server returns
 * 200 and several arguments in cret relating to the user's account.
 */
int CtdlIPCTryPassword(const char *passwd, char *cret)
{
	register int ret;
	char *aaa;

	if (!passwd) return -2;
	if (!cret) return -2;

	aaa = (char *)malloc(strlen(passwd) + 6);
	if (!aaa) return -1;

	sprintf(aaa, "PASS %s", passwd);
	ret = CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
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
int CtdlIPCCreateUser(const char *username, int selfservice, char *cret)
{
	register int ret;
	char *aaa;

	if (!username) return -2;
	if (!cret) return -2;

	aaa = (char *)malloc(strlen(username) + 6);
	if (!aaa) return -1;

	sprintf(aaa, "%s %s", selfservice ? "NEWU" : "CREU",  username);
	ret = CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/*
 * Changes the user's password.  Returns 200 if changed, errors otherwise.
 */
int CtdlIPCChangePassword(const char *passwd, char *cret)
{
	register int ret;
	char *aaa;

	if (!passwd) return -2;
	if (!cret) return -2;

	aaa = (char *)malloc(strlen(passwd) + 6);
	if (!aaa) return -1;

	sprintf(aaa, "SETP %s", passwd);
	ret = CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/* LKRN */
/* Caller must free the march list */
/* which is 0 = LRMS, 1 = LKRN, 2 = LKRO, 3 = LKRA, 4 = LZRM */
/* floor is -1 for all, or floornum */
int CtdlIPCKnownRooms(int which, int floor, struct march **listing, char *cret)
{
	register int ret;
	struct march *march = NULL;
	static char *proto[] = {"LRMS", "LKRN", "LKRO", "LKRA", "LZRM" };
	char aaa[256];
	char *bbb = NULL;
	size_t bbbsize;

	if (!listing) return -2;
	if (*listing) return -2;	/* Free the listing first */
	if (!cret) return -2;
	if (which < 0 || which > 4) return -2;
	if (floor < -1) return -2;	/* Can't validate upper bound, sorry */

	sprintf(aaa, "%s %d", proto[which], floor);
	ret = CtdlIPCGenericCommand(aaa, NULL, 0, &bbb, &bbbsize, cret);
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
int CtdlIPCGetConfig(struct usersupp **uret, char *cret)
{
	register int ret;

	if (!cret) return -2;
	if (!uret) return -2;
	if (!*uret) *uret = (struct usersupp *)calloc(1, sizeof (struct usersupp));
	if (!*uret) return -1;

	ret = CtdlIPCGenericCommand("GETU", NULL, 0, NULL, NULL, cret);
	if (ret / 100 == 2) {
		uret[0]->USscreenwidth = extract_int(cret, 0);
		uret[0]->USscreenheight = extract_int(cret, 1);
		uret[0]->flags = extract_int(cret, 2);
	}
	return ret;
}


/* SETU */
int CtdlIPCSetConfig(struct usersupp *uret, char *cret)
{
	char aaa[48];

	if (!uret) return -2;
	if (!cret) return -2;

	sprintf(aaa, "SETU %d|%d|%d",
			uret->USscreenwidth, uret->USscreenheight,
			uret->flags);
	return CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
}


/* GOTO */
int CtdlIPCGotoRoom(const char *room, const char *passwd,
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
	ret = CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
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
int CtdlIPCGetMessages(int which, int whicharg, const char *template,
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
	ret = CtdlIPCGenericCommand(aaa, template, count, &bbb, &bbbsize, cret);
	count = 0;
	while (strlen(bbb)) {
		int a;

		extract_token(aaa, bbb, 0, '\n');
		a = strlen(aaa);
		memmove(aaa, bbb + a + 1, strlen(bbb) - a - 1);
		*mret = (long *)realloc(mret, (count + 1) * sizeof (long));
		if (*mret)
			*mret[count++] = atol(aaa);
		*mret[count] = 0L;
	}
	return ret;
}


/* MSG0, MSG2 */
int CtdlIPCGetSingleMessage(long msgnum, int headers, int as_mime,
		struct ctdlipcmessage **mret, char *cret)
{
	register int ret;
	char aaa[SIZ];
	char *bbb = NULL;
	size_t bbbsize;

	if (!cret) return -1;
	if (!mret) return -1;
	if (!*mret) *mret = (struct ctdlipcmessage *)calloc(1, sizeof (struct ctdlipcmessage));
	if (!*mret) return -1;
	if (!msgnum) return -1;

	sprintf(aaa, "MSG%c %ld|%d", as_mime ? '2' : '0', msgnum, headers);
	ret = CtdlIPCGenericCommand(aaa, NULL, 0, &bbb, &bbbsize, cret);
	if (ret / 100 == 1) {
		if (!as_mime) {
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
				else if (!strncasecmp(aaa, "part=", 5)) {
					struct parts *ptr, *chain;
	
					ptr = (struct parts *)calloc(1, sizeof (struct parts));
					if (ptr) {
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
					}
				}
			}
			/* Eliminate "text\n" */
			remove_token(bbb, 0, '\n');
		}
		if (strlen(bbb)) {
			/* Strip trailing whitespace */
			bbb = (char *)realloc(bbb, strlen(bbb) + 1);
			mret[0]->text = bbb;
		} else {
			free(bbb);
		}
	}
	return ret;
}


/* WHOK */
int CtdlIPCWhoKnowsRoom(char **listing, char *cret)
{
	register int ret;
	size_t bytes;

	if (!cret) return -2;
	if (!listing) return -2;
	if (*listing) return -2;

	ret = CtdlIPCGenericCommand("WHOK", NULL, 0, listing, &bytes, cret);
	return ret;
}


/* INFO */
int CtdlIPCServerInfo(struct CtdlServInfo *ServInfo, char *cret)
{
	register int ret;
	size_t bytes;
	char *listing = NULL;
	char buf[SIZ];

	if (!cret) return -2;
	if (!ServInfo) return -2;

	ret = CtdlIPCGenericCommand("INFO", NULL, 0, &listing, &bytes, cret);
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
int CtdlIPCReadDirectory(char **listing, char *cret)
{
	register int ret;
	size_t bytes;

	if (!cret) return -2;
	if (!listing) return -2;
	if (*listing) return -2;

	ret = CtdlIPCGenericCommand("RDIR", NULL, 0, listing, &bytes, cret);
	return ret;
}


/*
 * Set last-read pointer in this room to msgnum, or 0 for HIGHEST.
 */
int CtdlIPCSetLastRead(long msgnum, char *cret)
{
	register int ret;
	char aaa[16];

	if (!cret) return -2;

	if (msgnum)
		sprintf(aaa, "SLRP %ld", msgnum);
	else
		sprintf(aaa, "SLRP HIGHEST");
	ret = CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
	return ret;
}


/* INVT */
int CtdlIPCInviteUserToRoom(const char *username, char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -2;
	if (!username) return -2;

	aaa = (char *)malloc(strlen(username) + 6);
	if (!aaa) return -1;

	sprintf(aaa, "INVT %s", username);
	ret = CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/* KICK */
int CtdlIPCKickoutUserFromRoom(const char *username, char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -1;
	if (!username) return -1;

	aaa = (char *)malloc(strlen(username) + 6);

	sprintf(aaa, "KICK %s", username);
	ret = CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/* GETR */
int CtdlIPCGetRoomAttributes(struct quickroom **qret, char *cret)
{
	register int ret;

	if (!cret) return -2;
	if (!qret) return -2;
	if (!*qret) *qret = (struct quickroom *)calloc(1, sizeof (struct quickroom));
	if (!*qret) return -1;

	ret = CtdlIPCGenericCommand("GETR", NULL, 0, NULL, NULL, cret);
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
int CtdlIPCSetRoomAttributes(int forget, struct quickroom *qret, char *cret)
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
	ret = CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/* GETA */
int CtdlIPCGetRoomAide(char *cret)
{
	if (!cret) return -1;

	return CtdlIPCGenericCommand("GETA", NULL, 0, NULL, NULL, cret);
}


/* SETA */
int CtdlIPCSetRoomAide(const char *username, char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -2;
	if (!username) return -2;

	aaa = (char *)malloc(strlen(username) + 6);
	if (!aaa) return -1;

	sprintf(aaa, "SETA %s", username);
	ret = CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/* ENT0 */
int CtdlIPCPostMessage(int flag, const struct ctdlipcmessage *mr, char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -2;
	if (!mr) return -2;

	aaa = (char *)malloc(strlen(mr->recipient) + strlen(mr->author) + 40);
	if (!aaa) return -1;

	sprintf(aaa, "ENT0 %d|%s|%d|%d|%s", flag, mr->recipient, mr->anonymous,
			mr->type, mr->author);
	ret = CtdlIPCGenericCommand(aaa, mr->text, strlen(mr->text), NULL,
			NULL, cret);
	free(aaa);
	return ret;
}


/* RINF */
int CtdlIPCRoomInfo(char **iret, char *cret)
{
	size_t bytes;

	if (!cret) return -2;
	if (!iret) return -2;
	if (*iret) return -2;

	return CtdlIPCGenericCommand("RINF", NULL, 0, iret, &bytes, cret);
}


/* DELE */
int CtdlIPCDeleteMessage(long msgnum, char *cret)
{
	char aaa[16];

	if (!cret) return -2;
	if (!msgnum) return -2;

	sprintf(aaa, "DELE %ld", msgnum);
	return CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
}


/* MOVE */
int CtdlIPCMoveMessage(int copy, long msgnum, const char *destroom, char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -2;
	if (!destroom) return -2;
	if (!msgnum) return -2;

	aaa = (char *)malloc(strlen(destroom) + 28);
	if (!aaa) return -1;

	sprintf(aaa, "MOVE %ld|%s|%d", msgnum, destroom, copy);
	ret = CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/* KILL */
int CtdlIPCDeleteRoom(int for_real, char *cret)
{
	char aaa[16];

	if (!cret) return -2;

	sprintf(aaa, "KILL %d", for_real);
	return CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
}


/* CRE8 */
int CtdlIPCCreateRoom(int for_real, const char *roomname, int type,
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
	ret = CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/* FORG */
int CtdlIPCForgetRoom(char *cret)
{
	if (!cret) return -2;

	return CtdlIPCGenericCommand("FORG", NULL, 0, NULL, NULL, cret);
}


/* MESG */
int CtdlIPCSystemMessage(const char *message, char **mret, char *cret)
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
	ret = CtdlIPCGenericCommand(aaa, NULL, 0, mret, &bytes, cret);
	free(aaa);
	return ret;
}


/* GNUR */
int CtdlIPCNextUnvalidatedUser(char *cret)
{
	if (!cret) return -2;

	return CtdlIPCGenericCommand("GNUR", NULL, 0, NULL, NULL, cret);
}


/* GREG */
int CtdlIPCGetUserRegistration(const char *username, char **rret, char *cret)
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
	ret = CtdlIPCGenericCommand(aaa, NULL, 0, rret, &bytes, cret);
	free(aaa);
	return ret;
}


/* VALI */
int CtdlIPCValidateUser(const char *username, int axlevel, char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -2;
	if (!username) return -2;
	if (axlevel < 0 || axlevel > 7) return -2;

	aaa = (char *)malloc(strlen(username) + 17);
	if (!aaa) return -1;

	sprintf(aaa, "VALI %s|%d", username, axlevel);
	ret = CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/* EINF */
int CtdlIPCSetRoomInfo(int for_real, const char *info, char *cret)
{
	char aaa[16];

	if (!cret) return -1;
	if (!info) return -1;

	sprintf(aaa, "EINF %d", for_real);
	return CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
}


/* LIST */
int CtdlIPCUserListing(char **listing, char *cret)
{
	size_t bytes;

	if (!cret) return -1;
	if (!listing) return -1;
	if (*listing) return -1;

	return CtdlIPCGenericCommand("LIST", NULL, 0, listing, &bytes, cret);
}


/* REGI */
int CtdlIPCSetRegistration(const char *info, char *cret)
{
	if (!cret) return -1;
	if (!info) return -1;

	return CtdlIPCGenericCommand("REGI", info, strlen(info),
			NULL, NULL, cret);
}


/* CHEK */
int CtdlIPCMiscCheck(struct ctdlipcmisc *chek, char *cret)
{
	register int ret;

	if (!cret) return -1;
	if (!chek) return -1;

	ret = CtdlIPCGenericCommand("CHEK", NULL, 0, NULL, NULL, cret);
	if (ret / 100 == 2) {
		chek->newmail = extract_long(cret, 0);
		chek->needregis = extract_int(cret, 1);
		chek->needvalid = extract_int(cret, 2);
	}
	return ret;
}


/* DELF */
int CtdlIPCDeleteFile(const char *filename, char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -2;
	if (!filename) return -2;
	
	aaa = (char *)malloc(strlen(filename) + 6);
	if (!aaa) return -1;

	sprintf(aaa, "DELF %s", filename);
	ret = CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/* MOVF */
int CtdlIPCMoveFile(const char *filename, const char *destroom, char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -2;
	if (!filename) return -2;
	if (!destroom) return -2;

	aaa = (char *)malloc(strlen(filename) + strlen(destroom) + 7);
	if (!aaa) return -1;

	sprintf(aaa, "MOVF %s|%s", filename, destroom);
	ret = CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/* NETF */
int CtdlIPCNetSendFile(const char *filename, const char *destnode, char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -2;
	if (!filename) return -2;
	if (!destnode) return -2;

	aaa = (char *)malloc(strlen(filename) + strlen(destnode) + 7);
	if (!aaa) return -1;

	sprintf(aaa, "NETF %s|%s", filename, destnode);
	ret = CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/* RWHO */
int CtdlIPCOnlineUsers(char **listing, time_t *stamp, char *cret)
{
	register int ret;
	size_t bytes;

	if (!cret) return -1;
	if (!listing) return -1;
	if (*listing) return -1;

	*stamp = CtdlIPCServerTime(cret);
	if (!*stamp)
		*stamp = time(NULL);
	ret = CtdlIPCGenericCommand("RWHO", NULL, 0, listing, &bytes, cret);
	return ret;
}


/* OPEN */
int CtdlIPCFileDownload(const char *filename, void **buf, char *cret)
{
	register int ret;
	size_t bytes;
	time_t last_mod;
	char mimetype[256];
	char *aaa;

	if (!cret) return -2;
	if (!filename) return -2;
	if (!buf) return -2;
	if (*buf) return -2;
	if (download_in_progress) return -2;

	aaa = (char *)malloc(strlen(filename) + 6);
	if (!aaa) return -1;

	sprintf(aaa, "OPEN %s", filename);
	ret = CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	/* FIXME: Possible race condition */
	if (ret / 100 == 2) {
		download_in_progress = 1;
		bytes = extract_long(cret, 0);
		last_mod = extract_int(cret, 1);
		extract(mimetype, cret, 2);
		ret = CtdlIPCReadDownload(buf, bytes, cret);
		ret = CtdlIPCEndDownload(cret);
		if (ret / 100 == 2)
			sprintf(cret, "%d|%ld|%s|%s", bytes, last_mod,
					filename, mimetype);
	}
	return ret;
}


/* OPNA */
int CtdlIPCAttachmentDownload(long msgnum, const char *part, void **buf,
		char *cret)
{
	register int ret;
	size_t bytes;
	time_t last_mod;
	char filename[256];
	char mimetype[256];
	char *aaa;

	if (!cret) return -2;
	if (!buf) return -2;
	if (*buf) return -2;
	if (!part) return -2;
	if (!msgnum) return -2;
	if (download_in_progress) return -2;

	aaa = (char *)malloc(strlen(part) + 17);
	if (!aaa) return -1;

	sprintf(aaa, "OPNA %ld|%s", msgnum, part);
	ret = CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	/* FIXME: Possible race condition */
	if (ret / 100 == 2) {
		download_in_progress = 1;
		bytes = extract_long(cret, 0);
		last_mod = extract_int(cret, 1);
		extract(mimetype, cret, 2);
		ret = CtdlIPCReadDownload(buf, bytes, cret);
		ret = CtdlIPCEndDownload(cret);
		if (ret / 100 == 2)
			sprintf(cret, "%d|%ld|%s|%s", bytes, last_mod,
					filename, mimetype);
	}
	return ret;
}


/* OIMG */
int CtdlIPCImageDownload(const char *filename, void **buf, char *cret)
{
	register int ret;
	size_t bytes;
	time_t last_mod;
	char mimetype[256];
	char *aaa;

	if (!cret) return -1;
	if (!buf) return -1;
	if (*buf) return -1;
	if (!filename) return -1;
	if (download_in_progress) return -1;

	aaa = (char *)malloc(strlen(filename) + 6);
	if (!aaa) return -1;

	sprintf(aaa, "OIMG %s", filename);
	ret = CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	/* FIXME: Possible race condition */
	if (ret / 100 == 2) {
		download_in_progress = 1;
		bytes = extract_long(cret, 0);
		last_mod = extract_int(cret, 1);
		extract(mimetype, cret, 2);
		ret = CtdlIPCReadDownload(buf, bytes, cret);
		ret = CtdlIPCEndDownload(cret);
		if (ret / 100 == 2)
			sprintf(cret, "%d|%ld|%s|%s", bytes, last_mod,
					filename, mimetype);
	}
	return ret;
}


/* UOPN */
int CtdlIPCFileUpload(const char *filename, const char *comment, void *buf,
		size_t bytes, char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -1;
	if (!filename) return -1;
	if (!comment) return -1;
	if (upload_in_progress) return -1;

	aaa = (char *)malloc(strlen(filename) + strlen(comment) + 7);
	if (!aaa) return -1;

	sprintf(aaa, "UOPN %s|%s", filename, comment);
	ret = CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	/* FIXME: Possible race condition */
	if (ret / 100 == 2)
		upload_in_progress = 1;
	ret = CtdlIPCWriteUpload(buf, bytes, cret);
	ret = CtdlIPCEndUpload(cret);
	return ret;
}


/* UIMG */
int CtdlIPCImageUpload(int for_real, const char *filename, size_t bytes,
		char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -1;
	if (!filename) return -1;
	if (upload_in_progress) return -1;

	aaa = (char *)malloc(strlen(filename) + 17);
	if (!aaa) return -1;

	sprintf(aaa, "UIMG %d|%s", for_real, filename);
	ret = CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	/* FIXME: Possible race condition */
	if (ret / 100 == 2)
		upload_in_progress = 1;
	return ret;
}


/* QUSR */
int CtdlIPCQueryUsername(const char *username, char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -2;
	if (!username) return -2;

	aaa = (char *)malloc(strlen(username) + 6);
	if (!aaa) return -1;

	sprintf(aaa, "QUSR %s", username);
	ret = CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/* LFLR */
int CtdlIPCFloorListing(char **listing, char *cret)
{
	size_t bytes;

	if (!cret) return -2;
	if (!listing) return -2;
	if (*listing) return -2;

	return CtdlIPCGenericCommand("LFLR", NULL, 0, listing, &bytes, cret);
}


/* CFLR */
int CtdlIPCCreateFloor(int for_real, const char *name, char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -2;
	if (!name) return -2;

	aaa = (char *)malloc(strlen(name) + 17);
	if (!aaa) return -1;

	sprintf(aaa, "CFLR %s|%d", name, for_real);
	ret = CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/* KFLR */
int CtdlIPCDeleteFloor(int for_real, int floornum, char *cret)
{
	char aaa[27];

	if (!cret) return -1;
	if (floornum < 0) return -1;

	sprintf(aaa, "KFLR %d|%d", floornum, for_real);
	return CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
}


/* EFLR */
int CtdlIPCEditFloor(int floornum, const char *floorname, char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -2;
	if (!floorname) return -2;
	if (floornum < 0) return -2;

	aaa = (char *)malloc(strlen(floorname) + 17);
	if (!aaa) return -1;

	sprintf(aaa, "EFLR %d|%s", floornum, floorname);
	ret = CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/* IDEN */
int CtdlIPCIdentifySoftware(int developerid, int clientid, int revision,
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
	ret = CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/* SEXP */
int CtdlIPCSendInstantMessage(const char *username, const char *text,
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
		ret = CtdlIPCGenericCommand(aaa, text, strlen(text),
				NULL, NULL, cret);
	} else {
		sprintf(aaa, "SEXP %s||", username);
		ret = CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
	}
	free(aaa);
	return ret;
}


/* GEXP */
int CtdlIPCGetInstantMessage(char **listing, char *cret)
{
	size_t bytes;

	if (!cret) return -2;
	if (!listing) return -2;
	if (*listing) return -2;

	return CtdlIPCGenericCommand("GEXP", NULL, 0, listing, &bytes, cret);
}


/* DEXP */
/* mode is 0 = enable, 1 = disable, 2 = status */
int CtdlIPCEnableInstantMessageReceipt(int mode, char *cret)
{
	char aaa[16];

	if (!cret) return -2;

	sprintf(aaa, "DEXP %d", mode);
	return CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
}


/* EBIO */
int CtdlIPCSetBio(char *bio, char *cret)
{
	if (!cret) return -2;
	if (!bio) return -2;

	return CtdlIPCGenericCommand("EBIO", bio, strlen(bio),
			NULL, NULL, cret);
}


/* RBIO */
int CtdlIPCGetBio(const char *username, char **listing, char *cret)
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
	ret = CtdlIPCGenericCommand(aaa, NULL, 0, listing, &bytes, cret);
	free(aaa);
	return ret;
}


/* LBIO */
int CtdlIPCListUsersWithBios(char **listing, char *cret)
{
	size_t bytes;

	if (!cret) return -2;
	if (!listing) return -2;
	if (*listing) return -2;

	return CtdlIPCGenericCommand("LBIO", NULL, 0, listing, &bytes, cret);
}


/* STEL */
int CtdlIPCStealthMode(int mode, char *cret)
{
	char aaa[16];

	if (!cret) return -1;

	sprintf(aaa, "STEL %d", mode ? 1 : 0);
	return CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
}


/* TERM */
int CtdlIPCTerminateSession(int sid, char *cret)
{
	char aaa[16];

	if (!cret) return -1;

	sprintf(aaa, "TERM %d", sid);
	return CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
}


/* DOWN */
int CtdlIPCTerminateServerNow(char *cret)
{
	if (!cret) return -1;

	return CtdlIPCGenericCommand("DOWN", NULL, 0, NULL, NULL, cret);
}


/* SCDN */
int CtdlIPCTerminateServerScheduled(int mode, char *cret)
{
	char aaa[16];

	if (!cret) return -1;

	sprintf(aaa, "SCDN %d", mode ? 1 : 0);
	return CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
}


/* EMSG */
int CtdlIPCEnterSystemMessage(const char *filename, const char *text,
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
	ret = CtdlIPCGenericCommand(aaa, text, strlen(text), NULL, NULL, cret);
	free(aaa);
	return ret;
}


/* HCHG */
int CtdlIPCChangeHostname(const char *hostname, char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -2;
	if (!hostname) return -2;

	aaa = (char *)malloc(strlen(hostname) + 6);
	if (!aaa) return -1;

	sprintf(aaa, "HCHG %s", hostname);
	ret = CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/* RCHG */
int CtdlIPCChangeRoomname(const char *roomname, char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -2;
	if (!roomname) return -2;

	aaa = (char *)malloc(strlen(roomname) + 6);
	if (!aaa) return -1;

	sprintf(aaa, "RCHG %s", roomname);
	ret = CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/* UCHG */
int CtdlIPCChangeUsername(const char *username, char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -2;
	if (!username) return -2;

	aaa = (char *)malloc(strlen(username) + 6);
	if (!aaa) return -1;

	sprintf(aaa, "UCHG %s", username);
	ret = CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/* TIME */
/* This function returns the actual server time reported, or 0 if error */
time_t CtdlIPCServerTime(char *cret)
{
	register time_t tret;
	register int ret;

	ret = CtdlIPCGenericCommand("TIME", NULL, 0, NULL, NULL, cret);
	if (ret / 100 == 2) {
		tret = extract_long(cret, 0);
	} else {
		tret = 0L;
	}
	return tret;
}


/* AGUP */
int CtdlIPCAideGetUserParameters(const char *who,
				 struct usersupp **uret, char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -2;
	if (!uret) return -2;
	if (!*uret) *uret = (struct usersupp *)calloc(1, sizeof(struct usersupp));
	if (!*uret) return -1;

	aaa = (char *)malloc(strlen(uret[0]->fullname) + 6);
	if (!aaa) return -1;

	sprintf(aaa, "AGUP %s", who);
	ret = CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
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
	free(aaa);
	return ret;
}


/* ASUP */
int CtdlIPCAideSetUserParameters(const struct usersupp *uret, char *cret)
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
	ret = CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	return ret;
}


/* GPEX */
/* which is 0 = room, 1 = floor, 2 = site */
int CtdlIPCGetMessageExpirationPolicy(int which, char *cret)
{
	static char *proto[] = {"room", "floor", "site"};
	char aaa[11];

	if (!cret) return -2;
	if (which < 0 || which > 2) return -2;
	
	sprintf(aaa, "GPEX %s", proto[which]);
	return CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
}


/* SPEX */
/* which is 0 = room, 1 = floor, 2 = site */
/* policy is 0 = inherit, 1 = no purge, 2 = by count, 3 = by age (days) */
int CtdlIPCSetMessageExpirationPolicy(int which, int policy, int value,
		char *cret)
{
	char aaa[38];

	if (!cret) return -2;
	if (which < 0 || which > 2) return -2;
	if (policy < 0 || policy > 3) return -2;
	if (policy >= 2 && value < 1) return -2;

	sprintf(aaa, "SPEX %d|%d|%d", which, policy, value);
	return CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
}


/* CONF GET */
int CtdlGetSystemConfig(char **listing, char *cret)
{
	size_t bytes;

	if (!cret) return -2;
	if (!listing) return -2;
	if (*listing) return -2;

	return CtdlIPCGenericCommand("CONF GET", NULL, 0,
			listing, &bytes, cret);
}


/* CONF SET */
int CtdlSetSystemConfig(const char *listing, char *cret)
{
	if (!cret) return -2;
	if (!listing) return -2;

	return CtdlIPCGenericCommand("CONF SET", listing, strlen(listing),
			NULL, NULL, cret);
}


/* MMOD */
int CtdlIPCModerateMessage(long msgnum, int level, char *cret)
{
	char aaa[27];

	if (!cret) return -2;
	if (!msgnum) return -2;

	sprintf(aaa, "MMOD %ld|%d", msgnum, level);
	return CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
}


/* REQT */
int CtdlIPCRequestClientLogout(int session, char *cret)
{
	char aaa[16];

	if (!cret) return -2;
	if (session < 0) return -2;

	sprintf(aaa, "REQT %d", session);
	return CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
}


/* SEEN */
int CtdlIPCSetMessageSeen(long msgnum, int seen, char *cret)
{
	char aaa[27];

	if (!cret) return -2;
	if (msgnum < 0) return -2;

	sprintf(aaa, "SEEN %ld|%d", msgnum, seen ? 1 : 0);
	return CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
}


/* STLS */
int CtdlIPCStartEncryption(char *cret)
{
	return CtdlIPCGenericCommand("STLS", NULL, 0, NULL, NULL, cret);
}


/* QDIR */
int CtdlIPCDirectoryLookup(const char *address, char *cret)
{
	char *aaa;

	if (!address) return -2;
	if (!cret) return -2;

	aaa = (char *)malloc(strlen(address) + 6);
	if (!aaa) return -1;

	sprintf(aaa, "QDIR %s", address);
	return CtdlIPCGenericCommand(aaa, NULL, 0, NULL, NULL, cret);
}


/*
 * Not implemented:
 * 
 * CHAT
 * ETLS
 * EXPI
 * GTLS
 * IGAB
 * IPGM
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


inline void netio_lock(void)
{
#ifdef THREADED_CLIENT
	pthread_mutex_lock(&rwlock);
#endif
}


inline void netio_unlock(void)
{
#ifdef THREADED_CLIENT
	pthread_mutex_unlock(&rwlock);
#endif
}


/* Read a listing from the server up to 000.  Append to dest if it exists */
char *CtdlIPCReadListing(char *dest)
{
	long length = 0;
	char *ret;
	char aaa[256];

	ret = dest;
	if (ret) length = strlen(ret);
	while (serv_gets(aaa), strcmp(aaa, "000")) {
		ret = (char *)realloc(ret, length + strlen(aaa) + 2);
		if (ret) {
			strcpy(&ret[length], aaa);
			length += strlen(aaa);
			strcpy(&ret[length++], "\n");
		}
	}
	return ret;
}


/* Send a listing to the server; generate the ending 000. */
int CtdlIPCSendListing(const char *listing)
{
	char *text;

	text = (char *)malloc(strlen(listing) + 6);
	if (text) {
		strcpy(text, listing);
		while (text[strlen(text) - 1] == '\n')
			text[strlen(text) - 1] = '\0';
		strcat(text, "\n000");
		serv_puts(text);
		free(text);
		text = NULL;
	} else {
		/* Malloc failed but we are committed to send */
		/* This may result in extra blanks at the bottom */
		serv_puts(text);
		serv_puts("000");
	}
	return 0;
}


/* Partial read of file from server */
size_t CtdlIPCPartialRead(void **buf, size_t offset, size_t bytes, char *cret)
{
	register size_t len = 0;
	char aaa[256];

	if (!buf) return -1;
	if (!cret) return -1;
	if (bytes < 1) return -1;
	if (offset < 0) return -1;

	netio_lock();
	sprintf(aaa, "READ %d|%d", offset, bytes);
	serv_puts(aaa);
	serv_gets(aaa);
	if (aaa[0] != '6')
		strcpy(cret, &aaa[4]);
	else {
		len = extract_long(&aaa[4], 0);
		*buf = (void *)realloc(*buf, offset + len);
		if (*buf) {
			/* I know what I'm doing */
			serv_read((char *)&buf[offset], len);
		} else {
			/* We have to read regardless */
			serv_read(aaa, len);
			len = -1;
		}
	}
	netio_unlock();
	return len;
}


/* CLOS */
int CtdlIPCEndDownload(char *cret)
{
	register int ret;

	if (!cret) return -2;
	if (!download_in_progress) return -2;

	ret = CtdlIPCGenericCommand("CLOS", NULL, 0, NULL, NULL, cret);
	if (ret / 100 == 2)
		download_in_progress = 0;
	return ret;
}


/* READ */
int CtdlIPCReadDownload(void **buf, size_t bytes, char *cret)
{
	register size_t len;

	if (!cret) return -1;
	if (!buf) return -1;
	if (*buf) return -1;
	if (!download_in_progress) return -1;

	len = 0;
	while (len < bytes) {
		len = CtdlIPCPartialRead(buf, len, 4096, cret);
		if (len == -1) {
			free(*buf);
			return 0;
		}
	}
	return len;
}


/* UCLS */
int CtdlIPCEndUpload(char *cret)
{
	register int ret;

	if (!cret) return -1;
	if (!upload_in_progress) return -1;

	ret = CtdlIPCGenericCommand("UCLS", NULL, 0, NULL, NULL, cret);
	if (ret / 100 == 2)
		upload_in_progress = 0;
	return ret;
}


/* WRIT */
int CtdlIPCWriteUpload(void *buf, size_t bytes, char *cret)
{
	register int ret = -1;
	register size_t offset;
	char aaa[256];

	if (!cret) return -1;
	if (!buf) return -1;
	if (bytes < 1) return -1;

	offset = 0;
	while (offset < bytes) {
		sprintf(aaa, "WRIT %d", bytes - offset);
		serv_puts(aaa);
		serv_gets(aaa);
		strcpy(cret, &aaa[4]);
		ret = atoi(aaa);
		if (aaa[0] == '7') {
			register size_t to_write;

			to_write = extract_long(&aaa[4], 0);
			serv_write(buf + offset, to_write);
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
int CtdlIPCGenericCommand(const char *command, const char *to_send,
		size_t bytes_to_send, char **to_receive, 
		size_t *bytes_to_receive, char *proto_response)
{
	char buf[SIZ];
	register int ret;

	if (!command) return -2;
	if (!proto_response) return -2;

	netio_lock();
	serv_puts((char *)command);
	while (1) {
		serv_gets(proto_response);
		if (proto_response[3] == '*')
			express_msgs = 1;
		ret = atoi(proto_response);
		memmove(proto_response, &proto_response[4],
				strlen(proto_response) - 3);
		switch (ret / 100) {
		default:			/* Unknown, punt */
		case 2:				/* OK */
		case 3:				/* MORE_DATA */
		case 5:				/* ERROR */
			/* Don't need to do anything */
			break;
		case 1:				/* LISTING_FOLLOWS */
			if (to_receive && !*to_receive && bytes_to_receive) {
				*to_receive = CtdlIPCReadListing(NULL);
			} else { /* Drain */
				while (serv_gets(buf), strcmp(buf, "000")) ;
				ret = -ret;
			}
			break;
		case 4:				/* SEND_LISTING */
			if (to_send) {
				CtdlIPCSendListing(to_send);
			} else {
				/* No listing given, fake it */
				serv_puts("000");
				ret = -ret;
			}
			break;
		case 6:				/* BINARY_FOLLOWS */
			if (to_receive && !*to_receive && bytes_to_receive) {
				*bytes_to_receive =
					extract_long(proto_response, 0);
				*to_receive = (char *)malloc(*bytes_to_receive);
				if (!*to_receive) {
					ret = -1;
				} else {
					serv_read(*to_receive,
							*bytes_to_receive);
				}
			} else {
				/* Drain */
				size_t drain;

				drain = extract_long(proto_response, 0);
				while (drain > SIZ) {
					serv_read(buf, SIZ);
					drain -= SIZ;
				}
				serv_read(buf, drain);
				ret = -ret;
			}
			break;
		case 7:				/* SEND_BINARY */
			if (to_send && bytes_to_send) {
				serv_write((char *)to_send, bytes_to_send);
			} else if (bytes_to_send) {
				/* Fake it, send nulls */
				size_t fake;

				fake = bytes_to_send;
				memset(buf, '\0', SIZ);
				while (fake > SIZ) {
					serv_write(buf, SIZ);
					fake -= SIZ;
				}
				serv_write(buf, fake);
				ret = -ret;
			} /* else who knows?  DANGER WILL ROBINSON */
			break;
		case 8:				/* START_CHAT_MODE */
			if (!strncasecmp(command, "CHAT", 4)) {
				/* Don't call chatmode with generic! */
				serv_puts("/quit");
				ret = -ret;
			} else {
				/* In this mode we send then receive listing */
				if (to_send) {
					CtdlIPCSendListing(to_send);
				} else {
					/* No listing given, fake it */
					serv_puts("000");
					ret = -ret;
				}
				if (to_receive && !*to_receive
						&& bytes_to_receive) {
					*to_receive = CtdlIPCReadListing(NULL);
				} else { /* Drain */
					while (serv_gets(buf),
							strcmp(buf, "000")) ;
					ret = -ret;
				}
			}
			break;
		case 9:				/* ASYNC_MSG */
			/* CtdlIPCDoAsync(ret, proto_response); */
			free(CtdlIPCReadListing(NULL));	/* STUB FIXME */
			break;
		}
		if (ret / 100 != 9)
			break;
	}
	netio_unlock();
	return ret;
}
