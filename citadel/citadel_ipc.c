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
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <stdlib.h>
#include <ctype.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/un.h>
#include <errno.h>
#ifdef THREADED_CLIENT
#include <pthread.h>
#endif
#include "citadel.h"
#include "citadel_ipc.h"
#include "citadel_decls.h"
#include "tools.h"
#include "citadel_dirs.h"
#ifdef THREADED_CLIENT
pthread_mutex_t rwlock;
#endif

#ifdef HAVE_OPENSSL
static SSL_CTX *ssl_ctx;
char arg_encrypt;
char rc_encrypt;
#ifdef THREADED_CLIENT
pthread_mutex_t **Critters;			/* Things that need locking */
#endif /* THREADED_CLIENT */

#endif /* HAVE_OPENSSL */

#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff
#endif

static void (*status_hook)(char *s) = NULL;

void setCryptoStatusHook(void (*hook)(char *s)) {
	status_hook = hook;
}

void CtdlIPC_SetNetworkStatusCallback(CtdlIPC *ipc, void (*hook)(int state)) {
	ipc->network_status_cb = hook;
}


char instant_msgs = 0;


static void serv_read(CtdlIPC *ipc, char *buf, unsigned int bytes);
static void serv_write(CtdlIPC *ipc, const char *buf, unsigned int nbytes);
#ifdef HAVE_OPENSSL
static void serv_read_ssl(CtdlIPC *ipc, char *buf, unsigned int bytes);
static void serv_write_ssl(CtdlIPC *ipc, const char *buf, unsigned int nbytes);
static void endtls(SSL *ssl);
#ifdef THREADED_CLIENT
static unsigned long id_callback(void);
#endif /* THREADED_CLIENT */
#endif /* HAVE_OPENSSL */
static void CtdlIPC_getline(CtdlIPC* ipc, char *buf);
static void CtdlIPC_putline(CtdlIPC *ipc, const char *buf);


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
	register int ret = 221;		/* Default to successful quit */
	char aaa[SIZ]; 

	CtdlIPC_lock(ipc);
	if (ipc->sock > -1) {
		CtdlIPC_putline(ipc, "QUIT");
		CtdlIPC_getline(ipc, aaa);
		ret = atoi(aaa);
	}
#ifdef HAVE_OPENSSL
	if (ipc->ssl)
		SSL_shutdown(ipc->ssl);
	ipc->ssl = NULL;
#endif
	if (ipc->sock)
		shutdown(ipc->sock, 2);	/* Close connection; we're dead */
	ipc->sock = -1;
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
	char aaa[SIZ];

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
 * Second stage of authentication - provide password.  The server returns
 * 200 and several arguments in cret relating to the user's account.
 */
int CtdlIPCTryApopPassword(CtdlIPC *ipc, const char *response, char *cret)
{
	register int ret;
	char *aaa;

	if (!response) return -2;
	if (!cret) return -2;

	aaa = (char *)malloc((size_t)(strlen(response) + 6));
	if (!aaa) return -1;

	sprintf(aaa, "PAS2 %s", response);
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
/* Room types are defined in enum RoomList; keep these in sync! */
/* floor is -1 for all, or floornum */
int CtdlIPCKnownRooms(CtdlIPC *ipc, enum RoomList which, int floor, struct march **listing, char *cret)
{
	register int ret;
	struct march *march = NULL;
	static char *proto[] =
		{"LKRA", "LKRN", "LKRO", "LZRM", "LRMS", "LPRM" };
	char aaa[SIZ];
	char *bbb = NULL;
	size_t bbb_len;

	if (!listing) return -2;
	if (*listing) return -2;	/* Free the listing first */
	if (!cret) return -2;
	/* if (which < 0 || which > 4) return -2; */
	if (floor < -1) return -2;	/* Can't validate upper bound, sorry */

	sprintf(aaa, "%s %d", proto[which], floor);
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, &bbb, &bbb_len, cret);
	if (ret / 100 == 1) {
		struct march *mptr;

		while (bbb && strlen(bbb)) {
			int a;

			extract_token(aaa, bbb, 0, '\n', sizeof aaa);
			a = strlen(aaa);
			memmove(bbb, bbb + a + 1, strlen(bbb) - a);
			mptr = (struct march *) malloc(sizeof (struct march));
			if (mptr) {
				mptr->next = NULL;
				extract_token(mptr->march_name, aaa, 0, '|', sizeof mptr->march_name);
				mptr->march_flags = (unsigned int) extract_int(aaa, 1);
				mptr->march_floor = (char) extract_int(aaa, 2);
				mptr->march_order = (char) extract_int(aaa, 3);
				mptr->march_flags2 = (unsigned int) extract_int(aaa, 4);
				mptr->march_access = (char) extract_int(aaa, 5);
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
	if (bbb) free(bbb);
	return ret;
}


/* GETU */
/* Caller must free the struct ctdluser; caller may pass an existing one */
int CtdlIPCGetConfig(CtdlIPC *ipc, struct ctdluser **uret, char *cret)
{
	register int ret;

	if (!cret) return -2;
	if (!uret) return -2;
	if (!*uret) *uret = (struct ctdluser *)calloc(1, sizeof (struct ctdluser));
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
int CtdlIPCSetConfig(CtdlIPC *ipc, struct ctdluser *uret, char *cret)
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
		extract_token(rret[0]->RRname, cret, 0, '|', sizeof rret[0]->RRname);
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
		rret[0]->RRflags2 = extract_int(cret, 14);
	} else {
		free(*rret);
		*rret = NULL;
	}
	free(aaa);
	return ret;
}


/* MSGS */
/* which is 0 = all, 1 = old, 2 = new, 3 = last, 4 = first, 5 = gt, 6 = lt */
/* whicharg is number of messages, applies to last, first, gt, lt */
int CtdlIPCGetMessages(CtdlIPC *ipc, enum MessageList which, int whicharg,
		const char *mtemplate, unsigned long **mret, char *cret)
{
	register int ret;
	register unsigned long count = 0;
	static char *proto[] =
		{ "ALL", "OLD", "NEW", "LAST", "FIRST", "GT", "LT" };
	char aaa[33];
	char *bbb = NULL;
	size_t bbb_len;

	if (!cret) return -2;
	if (!mret) return -2;
	if (*mret) return -2;
	if (which < 0 || which > 6) return -2;

	if (which <= 2)
		sprintf(aaa, "MSGS %s||%d", proto[which],
				(mtemplate) ? 1 : 0);
	else
		sprintf(aaa, "MSGS %s|%d|%d", proto[which], whicharg,
				(mtemplate) ? 1 : 0);
	if (mtemplate) count = strlen(mtemplate);
	ret = CtdlIPCGenericCommand(ipc, aaa, mtemplate, count, &bbb, &bbb_len, cret);
	if (ret / 100 != 1)
		return ret;
	count = 0;
	*mret = (unsigned long *)calloc(1, sizeof(unsigned long));
	if (!*mret)
		return -1;
	while (bbb && strlen(bbb)) {
		extract_token(aaa, bbb, 0, '\n', sizeof aaa);
		remove_token(bbb, 0, '\n');
		*mret = (unsigned long *)realloc(*mret, (size_t)((count + 2) *
					sizeof (unsigned long)));
		if (*mret) {
			(*mret)[count++] = atol(aaa);
			(*mret)[count] = 0L;
		} else {
			break;
		}
	}
	if (bbb) free(bbb);
	return ret;
}


/* MSG0, MSG2 */
int CtdlIPCGetSingleMessage(CtdlIPC *ipc, long msgnum, int headers, int as_mime,
		struct ctdlipcmessage **mret, char *cret)
{
	register int ret;
	char aaa[SIZ];
	char *bbb = NULL;
	size_t bbb_len;
	int multipart_hunting = 0;
	char multipart_prefix[128];

	if (!cret) return -1;
	if (!mret) return -1;
	if (!*mret) *mret = (struct ctdlipcmessage *)calloc(1, sizeof (struct ctdlipcmessage));
	if (!*mret) return -1;
	if (!msgnum) return -1;

	strcpy(mret[0]->content_type, "");
	sprintf(aaa, "MSG%d %ld|%d", as_mime, msgnum, headers);
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, &bbb, &bbb_len, cret);
	if (ret / 100 == 1) {
		if (as_mime != 2) {
			strcpy(mret[0]->mime_chosen, "1");	/* Default chosen-part is "1" */
			while (strlen(bbb) > 4 && bbb[4] == '=') {
				extract_token(aaa, bbb, 0, '\n', sizeof aaa);
				remove_token(bbb, 0, '\n');

				if (!strncasecmp(aaa, "nhdr=yes", 8))
					mret[0]->nhdr = 1;
				else if (!strncasecmp(aaa, "from=", 5))
					safestrncpy(mret[0]->author, &aaa[5], SIZ);
				else if (!strncasecmp(aaa, "type=", 5))
					mret[0]->type = atoi(&aaa[5]);
				else if (!strncasecmp(aaa, "msgn=", 5))
					safestrncpy(mret[0]->msgid, &aaa[5], SIZ);
				else if (!strncasecmp(aaa, "subj=", 5))
					safestrncpy(mret[0]->subject, &aaa[5], SIZ);
				else if (!strncasecmp(aaa, "rfca=", 5))
					safestrncpy(mret[0]->email, &aaa[5], SIZ);
				else if (!strncasecmp(aaa, "hnod=", 5))
					safestrncpy(mret[0]->hnod, &aaa[5], SIZ);
				else if (!strncasecmp(aaa, "room=", 5))
					safestrncpy(mret[0]->room, &aaa[5], SIZ);
				else if (!strncasecmp(aaa, "node=", 5))
					safestrncpy(mret[0]->node, &aaa[5], SIZ);
				else if (!strncasecmp(aaa, "rcpt=", 5))
					safestrncpy(mret[0]->recipient, &aaa[5], SIZ);
				else if (!strncasecmp(aaa, "time=", 5))
					mret[0]->time = atol(&aaa[5]);

				/* Multipart/alternative prefix & suffix strings help
				 * us to determine which part we want to download.
				 */
				else if (!strncasecmp(aaa, "pref=", 5)) {
					extract_token(multipart_prefix, &aaa[5], 1, '|', sizeof multipart_prefix);
					if (!strcasecmp(multipart_prefix,
					   "multipart/alternative")) {
						++multipart_hunting;
					}
				}
				else if (!strncasecmp(aaa, "suff=", 5)) {
					extract_token(multipart_prefix, &aaa[5], 1, '|', sizeof multipart_prefix);
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
						extract_token(ptr->name, &aaa[5], 0, '|', sizeof ptr->name);
						extract_token(ptr->filename, &aaa[5], 1, '|', sizeof ptr->filename);
						extract_token(ptr->number, &aaa[5], 2, '|', sizeof ptr->number);
						extract_token(ptr->disposition, &aaa[5], 3, '|', sizeof ptr->disposition);
						extract_token(ptr->mimetype, &aaa[5], 4, '|', sizeof ptr->mimetype);
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
					if (!strncasecmp(bbb, "Content-type:", 13)) {
						extract_token(mret[0]->content_type, bbb, 0, '\n', sizeof mret[0]->content_type);
						strcpy(mret[0]->content_type, &mret[0]->content_type[13]);
						striplt(mret[0]->content_type);

						/* strip out ";charset=" portion.  FIXME do something with
						 * the charset (like... convert it) instead of just throwing
						 * it away
						 */
						if (strstr(mret[0]->content_type, ";") != NULL) {
							strcpy(strstr(mret[0]->content_type, ";"), "");
						}

					}
					if (!strncasecmp(bbb, "X-Citadel-MSG4-Partnum:", 23)) {
						extract_token(mret[0]->mime_chosen, bbb, 0, '\n', sizeof mret[0]->mime_chosen);
						strcpy(mret[0]->mime_chosen, &mret[0]->mime_chosen[23]);
						striplt(mret[0]->mime_chosen);
					}
					remove_token(bbb, 0, '\n');
				} while ((bbb[0] != 0) && (bbb[0] != '\n'));
				remove_token(bbb, 0, '\n');
			}


		}
		if (strlen(bbb)) {
			/* FIXME: Strip trailing whitespace */
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
int CtdlIPCServerInfo(CtdlIPC *ipc, char *cret)
{
	register int ret;
	size_t bytes;
	char *listing = NULL;
	char buf[SIZ];

	if (!cret) return -2;

	ret = CtdlIPCGenericCommand(ipc, "INFO", NULL, 0, &listing, &bytes, cret);
	if (ret / 100 == 1) {
		int line = 0;

		while (*listing && strlen(listing)) {
			extract_token(buf, listing, 0, '\n', sizeof buf);
			remove_token(listing, 0, '\n');
			switch (line++) {
			case 0:		ipc->ServInfo.pid = atoi(buf);
					break;
			case 1:		strcpy(ipc->ServInfo.nodename,buf);
					break;
			case 2:		strcpy(ipc->ServInfo.humannode,buf);
					break;
			case 3:		strcpy(ipc->ServInfo.fqdn,buf);
					break;
			case 4:		strcpy(ipc->ServInfo.software,buf);
					break;
			case 5:		ipc->ServInfo.rev_level = atoi(buf);
					break;
			case 6:		strcpy(ipc->ServInfo.site_location,buf);
					break;
			case 7:		strcpy(ipc->ServInfo.sysadm,buf);
					break;
			case 9:		strcpy(ipc->ServInfo.moreprompt,buf);
					break;
			case 10:	ipc->ServInfo.ok_floors = atoi(buf);
					break;
			case 11:	ipc->ServInfo.paging_level = atoi(buf);
					break;
			case 13:	ipc->ServInfo.supports_qnop = atoi(buf);
					break;
			case 14:	ipc->ServInfo.supports_ldap = atoi(buf);
					break;
			case 15:	ipc->ServInfo.newuser_disabled = atoi(buf);
					break;
			}
		}

	}
	if (listing) free(listing);
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

	if (msgnum) {
		sprintf(aaa, "SLRP %ld", msgnum);
	}
	else {
		sprintf(aaa, "SLRP HIGHEST");
	}
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
int CtdlIPCGetRoomAttributes(CtdlIPC *ipc, struct ctdlroom **qret, char *cret)
{
	register int ret;

	if (!cret) return -2;
	if (!qret) return -2;
	if (!*qret) *qret = (struct ctdlroom *)calloc(1, sizeof (struct ctdlroom));
	if (!*qret) return -1;

	ret = CtdlIPCGenericCommand(ipc, "GETR", NULL, 0, NULL, NULL, cret);
	if (ret / 100 == 2) {
		extract_token(qret[0]->QRname, cret, 0, '|', sizeof qret[0]->QRname);
		extract_token(qret[0]->QRpasswd, cret, 1, '|', sizeof qret[0]->QRpasswd);
		extract_token(qret[0]->QRdirname, cret, 2, '|', sizeof qret[0]->QRdirname);
		qret[0]->QRflags = extract_int(cret, 3);
		qret[0]->QRfloor = extract_int(cret, 4);
		qret[0]->QRorder = extract_int(cret, 5);
		qret[0]->QRdefaultview = extract_int(cret, 6);
		qret[0]->QRflags2 = extract_int(cret, 7);
	}
	return ret;
}


/* SETR */
/* set forget to kick all users out of room */
int CtdlIPCSetRoomAttributes(CtdlIPC *ipc, int forget, struct ctdlroom *qret, char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -2;
	if (!qret) return -2;

	aaa = (char *)malloc(strlen(qret->QRname) + strlen(qret->QRpasswd) +
			strlen(qret->QRdirname) + 64);
	if (!aaa) return -1;

	sprintf(aaa, "SETR %s|%s|%s|%d|%d|%d|%d|%d|%d",
			qret->QRname, qret->QRpasswd, qret->QRdirname,
			qret->QRflags, forget, qret->QRfloor, qret->QRorder,
			qret->QRdefaultview, qret->QRflags2);
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
int CtdlIPCPostMessage(CtdlIPC *ipc, int flag, int *subject_required,  const struct ctdlipcmessage *mr, char *cret)
{
	register int ret;
	char cmd[SIZ];

	if (!cret) return -2;
	if (!mr) return -2;

	snprintf(cmd, sizeof cmd,
			"ENT0 %d|%s|%d|%d|%s|%s", flag, mr->recipient,
			mr->anonymous, mr->type, mr->subject, mr->author);
	ret = CtdlIPCGenericCommand(ipc, cmd, mr->text, strlen(mr->text), NULL,
			NULL, cret);
	if ((flag == 0) && (subject_required != NULL)) {
		/* Is the server strongly recommending that the user enter a message subject? */
		if ((cret[3] != '\0') && (cret[4] != '\0')) {
			*subject_required = extract_int(&cret[4], 1);
		}

		
	}
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
int CtdlIPCUserListing(CtdlIPC *ipc, char *searchstring, char **listing, char *cret)
{
	size_t bytes;
	char *cmd;
	int ret;

	if (!cret) return -1;
	if (!listing) return -1;
	if (*listing) return -1;
	if (!searchstring) return -1;

	cmd = malloc(strlen(searchstring) + 10);
	sprintf(cmd, "LIST %s", searchstring);

	ret = CtdlIPCGenericCommand(ipc, cmd, NULL, 0, listing, &bytes, cret);
	free(cmd);
	return(ret);
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
		size_t resume,
		void (*progress_gauge_callback)
			(CtdlIPC*, unsigned long, unsigned long),
		char *cret)
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
		extract_token(mimetype, cret, 2, '|', sizeof mimetype);

		ret = CtdlIPCReadDownload(ipc, buf, bytes, resume,
					progress_gauge_callback, cret);
		/*
		ret = CtdlIPCHighSpeedReadDownload(ipc, buf, bytes, resume,
					progress_gauge_callback, cret);
		*/

		ret = CtdlIPCEndDownload(ipc, cret);
		if (ret / 100 == 2)
			sprintf(cret, "%d|%ld|%s|%s", (int)bytes, last_mod,
					filename, mimetype);
	}
	return ret;
}


/* OPNA */
int CtdlIPCAttachmentDownload(CtdlIPC *ipc, long msgnum, const char *part,
		void **buf,
		void (*progress_gauge_callback)
			(CtdlIPC*, unsigned long, unsigned long),
		char *cret)
{
	register int ret;
	size_t bytes;
	time_t last_mod;
	char filename[SIZ];
	char mimetype[SIZ];
	char aaa[SIZ];

	if (!cret) return -2;
	if (!buf) return -2;
	if (*buf) return -2;
	if (!part) return -2;
	if (!msgnum) return -2;
	if (ipc->downloading) return -2;

	sprintf(aaa, "OPNA %ld|%s", msgnum, part);
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
	if (ret / 100 == 2) {
		ipc->downloading = 1;
		bytes = extract_long(cret, 0);
		last_mod = extract_int(cret, 1);
		extract_token(filename, cret, 2, '|', sizeof filename);
		extract_token(mimetype, cret, 3, '|', sizeof mimetype);
		/* ret = CtdlIPCReadDownload(ipc, buf, bytes, 0, progress_gauge_callback, cret); */
		ret = CtdlIPCHighSpeedReadDownload(ipc, buf, bytes, 0, progress_gauge_callback, cret);
		ret = CtdlIPCEndDownload(ipc, cret);
		if (ret / 100 == 2)
			sprintf(cret, "%d|%ld|%s|%s", (int)bytes, last_mod,
					filename, mimetype);
	}
	return ret;
}


/* OIMG */
int CtdlIPCImageDownload(CtdlIPC *ipc, const char *filename, void **buf,
		void (*progress_gauge_callback)
			(CtdlIPC*, unsigned long, unsigned long),
		char *cret)
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
		extract_token(mimetype, cret, 2, '|', sizeof mimetype);
/*		ret = CtdlIPCReadDownload(ipc, buf, bytes, 0, progress_gauge_callback, cret); */
		ret = CtdlIPCHighSpeedReadDownload(ipc, buf, bytes, 0, progress_gauge_callback, cret);
		ret = CtdlIPCEndDownload(ipc, cret);
		if (ret / 100 == 2)
			sprintf(cret, "%d|%ld|%s|%s", (int)bytes, last_mod,
					filename, mimetype);
	}
	return ret;
}


/* UOPN */
int CtdlIPCFileUpload(CtdlIPC *ipc, const char *save_as, const char *comment,
		const char *path,
		void (*progress_gauge_callback)
			(CtdlIPC*, unsigned long, unsigned long),
		char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -1;
	if (!save_as) return -1;
	if (!comment) return -1;
	if (!path) return -1;
	if (!*path) return -1;
	if (ipc->uploading) return -1;

	aaa = (char *)malloc(strlen(save_as) + strlen(comment) + 7);
	if (!aaa) return -1;

	sprintf(aaa, "UOPN %s|%s", save_as, comment);
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	if (ret / 100 == 2) {
		ipc->uploading = 1;
		ret = CtdlIPCWriteUpload(ipc, path, progress_gauge_callback, cret);
		ret = CtdlIPCEndUpload(ipc, (ret == -2 ? 1 : 0), cret);
		ipc->uploading = 0;
	}
	return ret;
}


/* UIMG */
int CtdlIPCImageUpload(CtdlIPC *ipc, int for_real, const char *path,
		const char *save_as,
		void (*progress_gauge_callback)
			(CtdlIPC*, unsigned long, unsigned long),
		char *cret)
{
	register int ret;
	char *aaa;

	if (!cret) return -1;
	if (!save_as) return -1;
	if (!path && for_real) return -1;
	if (!*path && for_real) return -1;
	if (ipc->uploading) return -1;

	aaa = (char *)malloc(strlen(save_as) + 17);
	if (!aaa) return -1;

	sprintf(aaa, "UIMG %d|%s", for_real, save_as);
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
	free(aaa);
	if (ret / 100 == 2 && for_real) {
		ipc->uploading = 1;
		ret = CtdlIPCWriteUpload(ipc, path, progress_gauge_callback, cret);
		ret = CtdlIPCEndUpload(ipc, (ret == -2 ? 1 : 0), cret);
		ipc->uploading = 0;
	}
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
	char aaa[SIZ];

	if (!cret) return -2;
	if (!name) return -2;

	sprintf(aaa, "CFLR %s|%d", name, for_real);
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
	return ret;
}


/* KFLR */
int CtdlIPCDeleteFloor(CtdlIPC *ipc, int for_real, int floornum, char *cret)
{
	char aaa[SIZ];

	if (!cret) return -1;
	if (floornum < 0) return -1;

	sprintf(aaa, "KFLR %d|%d", floornum, for_real);
	return CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
}


/* EFLR */
int CtdlIPCEditFloor(CtdlIPC *ipc, int floornum, const char *floorname, char *cret)
{
	register int ret;
	char aaa[SIZ];

	if (!cret) return -2;
	if (!floorname) return -2;
	if (floornum < 0) return -2;

	sprintf(aaa, "EFLR %d|%s", floornum, floorname);
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
	return ret;
}


/*
 * IDEN 
 *
 * You only need to fill out hostname, the defaults will be used if any of the
 * other fields are not set properly.
 */
int CtdlIPCIdentifySoftware(CtdlIPC *ipc, int developerid, int clientid,
		int revision, const char *software_name, const char *hostname,
		char *cret)
{
	register int ret;
	char *aaa;

	if (developerid < 0 || clientid < 0 || revision < 0 ||
	    !software_name) {
		developerid = 8;
		clientid = 0;
		revision = REV_LEVEL - 600;
		software_name = "Citadel (libcitadel)";
	}
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
				 struct ctdluser **uret, char *cret)
{
	register int ret;
	char aaa[SIZ];

	if (!cret) return -2;
	if (!uret) return -2;
	if (!*uret) *uret = (struct ctdluser *)calloc(1, sizeof(struct ctdluser));
	if (!*uret) return -1;

	sprintf(aaa, "AGUP %s", who);
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);

	if (ret / 100 == 2) {
		extract_token(uret[0]->fullname, cret, 0, '|', sizeof uret[0]->fullname);
		extract_token(uret[0]->password, cret, 1, '|', sizeof uret[0]->password);
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
int CtdlIPCAideSetUserParameters(CtdlIPC *ipc, const struct ctdluser *uret, char *cret)
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
/* which is 0 = room, 1 = floor, 2 = site, 3 = default for mailboxes */
/* caller must free the struct ExpirePolicy */
int CtdlIPCGetMessageExpirationPolicy(CtdlIPC *ipc, int which,
		struct ExpirePolicy **policy, char *cret)
{
	static char *proto[] = {"room", "floor", "site", "mailboxes" };
	char cmd[256];
	register int ret;

	if (!cret) return -2;
	if (!policy) return -2;
	if (!*policy) *policy = (struct ExpirePolicy *)calloc(1, sizeof(struct ExpirePolicy));
	if (!*policy) return -1;
	if (which < 0 || which > 3) return -2;
	
	sprintf(cmd, "GPEX %s", proto[which]);
	ret = CtdlIPCGenericCommand(ipc, cmd, NULL, 0, NULL, NULL, cret);
	if (ret / 100 == 2) {
		policy[0]->expire_mode = extract_int(cret, 0);
		policy[0]->expire_value = extract_int(cret, 1);
	}
	return ret;
}


/* SPEX */
/* which is 0 = room, 1 = floor, 2 = site, 3 = default for mailboxes */
/* policy is 0 = inherit, 1 = no purge, 2 = by count, 3 = by age (days) */
int CtdlIPCSetMessageExpirationPolicy(CtdlIPC *ipc, int which,
		struct ExpirePolicy *policy, char *cret)
{
	char aaa[38];
	char *whichvals[] = { "room", "floor", "site", "mailboxes" };

	if (!cret) return -2;
	if (which < 0 || which > 3) return -2;
	if (!policy) return -2;
	if (policy->expire_mode < 0 || policy->expire_mode > 3) return -2;
	if (policy->expire_mode >= 2 && policy->expire_value < 1) return -2;

	sprintf(aaa, "SPEX %s|%d|%d", whichvals[which],
			policy->expire_mode, policy->expire_value);
	return CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
}


/* CONF GET */
int CtdlIPCGetSystemConfig(CtdlIPC *ipc, char **listing, char *cret)
{
	size_t bytes;

	if (!cret) return -2;
	if (!listing) return -2;
	if (*listing) return -2;

	return CtdlIPCGenericCommand(ipc, "CONF GET", NULL, 0,
			listing, &bytes, cret);
}


/* CONF SET */
int CtdlIPCSetSystemConfig(CtdlIPC *ipc, const char *listing, char *cret)
{
	if (!cret) return -2;
	if (!listing) return -2;

	return CtdlIPCGenericCommand(ipc, "CONF SET", listing, strlen(listing),
			NULL, NULL, cret);
}


/* CONF GETSYS */
int CtdlIPCGetSystemConfigByType(CtdlIPC *ipc, const char *mimetype,
	       	char **listing, char *cret)
{
	register int ret;
	char *aaa;
	size_t bytes;

	if (!cret) return -2;
	if (!mimetype) return -2;
	if (!listing) return -2;
	if (*listing) return -2;

	aaa = malloc(strlen(mimetype) + 13);
	if (!aaa) return -1;
	sprintf(aaa, "CONF GETSYS|%s", mimetype);
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0,
			listing, &bytes, cret);
    free(aaa);
    return ret;
}


/* CONF PUTSYS */
int CtdlIPCSetSystemConfigByType(CtdlIPC *ipc, const char *mimetype,
	       const char *listing, char *cret)
{
    register int ret;
	char *aaa;

	if (!cret) return -2;
	if (!mimetype) return -2;
	if (!listing) return -2;

	aaa = malloc(strlen(mimetype) + 13);
	if (!aaa) return -1;
	sprintf(aaa, "CONF PUTSYS|%s", mimetype);
	ret = CtdlIPCGenericCommand(ipc, aaa, listing, strlen(listing),
			NULL, NULL, cret);
    free(aaa);
    return ret;
}


/* GNET */
int CtdlIPCGetRoomNetworkConfig(CtdlIPC *ipc, char **listing, char *cret)
{
	size_t bytes;

	if (!cret) return -2;
	if (!listing) return -2;
	if (*listing) return -2;

	return CtdlIPCGenericCommand(ipc, "GNET", NULL, 0,
			listing, &bytes, cret);
}


/* SNET */
int CtdlIPCSetRoomNetworkConfig(CtdlIPC *ipc, const char *listing, char *cret)
{
	if (!cret) return -2;
	if (!listing) return -2;

	return CtdlIPCGenericCommand(ipc, "SNET", listing, strlen(listing),
			NULL, NULL, cret);
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
	int a;
	int r;
	char buf[SIZ];

#ifdef HAVE_OPENSSL
	SSL *temp_ssl;

	/* New SSL object */
	temp_ssl = SSL_new(ssl_ctx);
	if (!temp_ssl) {
		error_printf("SSL_new failed: %s\n",
				ERR_reason_error_string(ERR_get_error()));
		return -2;
	}
	/* Pointless flag waving */
#if SSLEAY_VERSION_NUMBER >= 0x0922
	SSL_set_session_id_context(temp_ssl, "Citadel SID", 14);
#endif

	if (!access(EGD_POOL, F_OK))
		RAND_egd(EGD_POOL);

	if (!RAND_status()) {
		error_printf("PRNG not properly seeded\n");
		return -2;
	}

	/* Associate network connection with SSL object */
	if (SSL_set_fd(temp_ssl, ipc->sock) < 1) {
		error_printf("SSL_set_fd failed: %s\n",
				ERR_reason_error_string(ERR_get_error()));
		return -2;
	}

	if (status_hook != NULL)
		status_hook("Requesting encryption...\r");

	/* Ready to start SSL/TLS */
	/* Old code
	CtdlIPC_putline(ipc, "STLS");
	CtdlIPC_getline(ipc, buf);
	if (buf[0] != '2') {
		error_printf("Server can't start TLS: %s\n", buf);
		return 0;
	}
	*/
	r = CtdlIPCGenericCommand(ipc,
				  "STLS", NULL, 0, NULL, NULL, cret);
	if (r / 100 != 2) {
		error_printf("Server can't start TLS: %s\n", buf);
		endtls(temp_ssl);
		return r;
	}

	/* Do SSL/TLS handshake */
	if ((a = SSL_connect(temp_ssl)) < 1) {
		error_printf("SSL_connect failed: %s\n",
				ERR_reason_error_string(ERR_get_error()));
		endtls(temp_ssl);
		return -2;
	}
	ipc->ssl = temp_ssl;

	BIO_set_close(ipc->ssl->rbio, BIO_NOCLOSE);
	{
		int bits, alg_bits;

		bits = SSL_CIPHER_get_bits(SSL_get_current_cipher(ipc->ssl), &alg_bits);
		error_printf("Encrypting with %s cipher %s (%d of %d bits)\n",
				SSL_CIPHER_get_version(SSL_get_current_cipher(ipc->ssl)),
				SSL_CIPHER_get_name(SSL_get_current_cipher(ipc->ssl)),
				bits, alg_bits);
	}
	return r;
#else
	return 0;
#endif /* HAVE_OPENSSL */
}


#ifdef HAVE_OPENSSL
static void endtls(SSL *ssl)
{
	if (ssl) {
		SSL_shutdown(ssl);
		SSL_free(ssl);
	}
}
#endif


/* QDIR */
int CtdlIPCDirectoryLookup(CtdlIPC *ipc, const char *address, char *cret)
{
    register int ret;
	char *aaa;

	if (!address) return -2;
	if (!cret) return -2;

	aaa = (char *)malloc(strlen(address) + 6);
	if (!aaa) return -1;

	sprintf(aaa, "QDIR %s", address);
	ret = CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
    free(aaa);
    return ret;
}


/* IPGM */
int CtdlIPCInternalProgram(CtdlIPC *ipc, int secret, char *cret)
{
	char aaa[30];

	if (!cret) return -2;
	sprintf(aaa, "IPGM %d", secret);
	return CtdlIPCGenericCommand(ipc, aaa, NULL, 0, NULL, NULL, cret);
}


/* FSCK */
int CtdlIPCMessageBaseCheck(CtdlIPC *ipc, char **mret, char *cret)
{
	size_t size = 0;

	if (!cret) return -2;
	if (!mret) return -2;
	if (*mret) return -2;

	return CtdlIPCGenericCommand(ipc, "FSCK", NULL, 0, mret, &size, cret);
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


INLINE void CtdlIPC_lock(CtdlIPC *ipc)
{
	if (ipc->network_status_cb) ipc->network_status_cb(1);
#ifdef THREADED_CLIENT
	pthread_mutex_lock(&(ipc->mutex));
#endif
}


INLINE void CtdlIPC_unlock(CtdlIPC *ipc)
{
#ifdef THREADED_CLIENT
	pthread_mutex_unlock(&(ipc->mutex));
#endif
	if (ipc->network_status_cb) ipc->network_status_cb(0);
}


/* Read a listing from the server up to 000.  Append to dest if it exists */
char *CtdlIPCReadListing(CtdlIPC *ipc, char *dest)
{
	size_t length = 0;
	size_t linelength;
	char *ret = NULL;
	char aaa[SIZ];

	ret = dest;
	if (ret != NULL) {
		length = strlen(ret);
	} else {
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

	if (!buf) return 0;
	if (!cret) return 0;
	if (bytes < 1) return 0;

	CtdlIPC_lock(ipc);
	sprintf(aaa, "READ %d|%d", (int)offset, (int)bytes);
	CtdlIPC_putline(ipc, aaa);
	CtdlIPC_getline(ipc, aaa);
	if (aaa[0] != '6')
		strcpy(cret, &aaa[4]);
	else {
		len = extract_long(&aaa[4], 0);
		*buf = (void *)realloc(*buf, (size_t)(offset + len));
		if (*buf) {
			/* I know what I'm doing */
			serv_read(ipc, ((char *)(*buf) + offset), len);
		} else {
			/* We have to read regardless */
			serv_read(ipc, aaa, len);
			len = 0;
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
int CtdlIPCReadDownload(CtdlIPC *ipc, void **buf, size_t bytes, size_t resume,
		void (*progress_gauge_callback)
			(CtdlIPC*, unsigned long, unsigned long),
	       char *cret)
{
	register size_t len;

	if (!cret) return -1;
	if (!buf) return -1;
	if (*buf) return -1;
	if (!ipc->downloading) return -1;

	len = resume;
	if (progress_gauge_callback)
		progress_gauge_callback(ipc, len, bytes);
	while (len < bytes) {
		register size_t block;

		block = CtdlIPCPartialRead(ipc, buf, len, 4096, cret);
		if (block == 0) {
			free(*buf);
			return 0;
		}
		len += block;
		if (progress_gauge_callback)
			progress_gauge_callback(ipc, len, bytes);
	}
	return len;
}

/* READ - pipelined */
int CtdlIPCHighSpeedReadDownload(CtdlIPC *ipc, void **buf, size_t bytes,
	       size_t resume,
		void (*progress_gauge_callback)
			(CtdlIPC*, unsigned long, unsigned long),
	       char *cret)
{
	register size_t len;
	register int calls;	/* How many calls in the pipeline */
	register int i;		/* iterator */
	char aaa[4096];

	if (!cret) return -1;
	if (!buf) return -1;
	if (*buf) return -1;
	if (!ipc->downloading) return -1;

	*buf = (void *)realloc(*buf, bytes - resume);
	if (!*buf) return -1;

	len = 0;
	CtdlIPC_lock(ipc);
	if (progress_gauge_callback)
		progress_gauge_callback(ipc, len, bytes);

	/* How many calls will be in the pipeline? */
	calls = (bytes - resume) / 4096;
	if ((bytes - resume) % 4096) calls++;

	/* Send all requests at once */
	for (i = 0; i < calls; i++) {
		sprintf(aaa, "READ %d|4096", (int)(i * 4096 + resume) );
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
			serv_read(ipc, ((char *)(*buf) + (i * 4096)), len);
		}
		if (progress_gauge_callback)
			progress_gauge_callback(ipc, i * 4096 + len, bytes);
	}
	CtdlIPC_unlock(ipc);
	return len;
}


/* UCLS */
int CtdlIPCEndUpload(CtdlIPC *ipc, int discard, char *cret)
{
	register int ret;
	char cmd[8];

	if (!cret) return -1;
	if (!ipc->uploading) return -1;

	sprintf(cmd, "UCLS %d", discard ? 0 : 1);
	ret = CtdlIPCGenericCommand(ipc, cmd, NULL, 0, NULL, NULL, cret);
	ipc->uploading = 0;
	return ret;
}


/* WRIT */
int CtdlIPCWriteUpload(CtdlIPC *ipc, const char *path,
		void (*progress_gauge_callback)
			(CtdlIPC*, unsigned long, unsigned long),
		char *cret)
{
	register int ret = -1;
	register size_t offset = 0;
	size_t bytes;
	char aaa[SIZ];
	char buf[4096];
	FILE *fd;
	int ferr;

	if (!cret) return -1;
	if (!path) return -1;
	if (!*path) return -1;

	fd = fopen(path, "r");
	if (!fd) return -2;

	fseek(fd, 0L, SEEK_END);
	bytes = ftell(fd);
	rewind(fd);

	if (progress_gauge_callback)
		progress_gauge_callback(ipc, 0, bytes);

	while (offset < bytes) {
		register size_t to_write;

		/* Read some data in */
		to_write = fread(buf, 1, 4096, fd);
		if (!to_write) {
			if (feof(fd) || ferror(fd)) break;
		}
		sprintf(aaa, "WRIT %d", (int)to_write);
		CtdlIPC_putline(ipc, aaa);
		CtdlIPC_getline(ipc, aaa);
		strcpy(cret, &aaa[4]);
		ret = atoi(aaa);
		if (aaa[0] == '7') {
			to_write = extract_long(&aaa[4], 0);
			
			serv_write(ipc, buf, to_write);
			offset += to_write;
			if (progress_gauge_callback)
				progress_gauge_callback(ipc, offset, bytes);
			/* Detect short reads and back up if needed */
			/* offset will never be negative anyway */
			fseek(fd, (signed)offset, SEEK_SET);
		} else {
			break;
		}
	}
	if (progress_gauge_callback)
		progress_gauge_callback(ipc, 1, 1);
	ferr = ferror(fd);
	fclose(fd);
	return (!ferr ? ret : -2);
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

#ifdef HAVE_OPENSSL
	if (ipc->ssl) watch_ssl = 1;
#endif

	CtdlIPC_lock(ipc);
	CtdlIPC_putline(ipc, command);
	while (1) {
		CtdlIPC_getline(ipc, proto_response);
		if (proto_response[3] == '*')
			instant_msgs = 1;
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


static int connectsock(char *host, char *service, char *protocol, int defaultPort)
{
	struct hostent *phe;
	struct servent *pse;
	struct protoent *ppe;
	struct sockaddr_in sin;
	int s, type;

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;

	pse = getservbyname(service, protocol);
	if (pse != NULL) {
		sin.sin_port = pse->s_port;
	}
	else if (atoi(service) > 0) {
		sin.sin_port = htons(atoi(service));
	}
	else {
		sin.sin_port = htons(defaultPort);
	}
	phe = gethostbyname(host);
	if (phe) {
		memcpy(&sin.sin_addr, phe->h_addr, phe->h_length);
	} else if ((sin.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE) {
		return -1;
	}
	if ((ppe = getprotobyname(protocol)) == 0) {
		return -1;
	}
	if (!strcmp(protocol, "udp")) {
		type = SOCK_DGRAM;
	} else {
		type = SOCK_STREAM;
	}

	s = socket(PF_INET, type, ppe->p_proto);
	if (s < 0) {
		return -1;
	}

	if (connect(s, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
		close(s);
		return -1;
	}

	return (s);
}

static int uds_connectsock(int *isLocal, char *sockpath)
{
	struct sockaddr_un addr;
	int s;

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	safestrncpy(addr.sun_path, sockpath, sizeof addr.sun_path);

	s = socket(AF_UNIX, SOCK_STREAM, 0);
	if (s < 0) {
		return -1;
	}

	if (connect(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		close(s);
		return -1;
	}

	*isLocal = 1;
	return s;
}


/*
 * input binary data from socket
 */
static void serv_read(CtdlIPC *ipc, char *buf, unsigned int bytes)
{
	unsigned int len, rlen;

#if defined(HAVE_OPENSSL)
	if (ipc->ssl) {
		serv_read_ssl(ipc, buf, bytes);
		return;
	}
#endif
	len = 0;
	while (len < bytes) {
		rlen = read(ipc->sock, &buf[len], bytes - len);
		if (rlen < 1) {
			connection_died(ipc, 0);
			return;
		}
		len += rlen;
	}
}


/*
 * send binary to server
 */
void serv_write(CtdlIPC *ipc, const char *buf, unsigned int nbytes)
{
	unsigned int bytes_written = 0;
	int retval;

#if defined(HAVE_OPENSSL)
	if (ipc->ssl) {
		serv_write_ssl(ipc, buf, nbytes);
		return;
	}
#endif
	while (bytes_written < nbytes) {
		retval = write(ipc->sock, &buf[bytes_written],
			       nbytes - bytes_written);
		if (retval < 1) {
			connection_died(ipc, 0);
			return;
		}
		bytes_written += retval;
	}
}


#ifdef HAVE_OPENSSL
/*
 * input binary data from encrypted connection
 */
static void serv_read_ssl(CtdlIPC* ipc, char *buf, unsigned int bytes)
{
	int len, rlen;
	char junk[1];

	len = 0;
	while (len < bytes) {
		if (SSL_want_read(ipc->ssl)) {
			if ((SSL_write(ipc->ssl, junk, 0)) < 1) {
				error_printf("SSL_write in serv_read:\n");
				ERR_print_errors_fp(stderr);
			}
		}
		rlen = SSL_read(ipc->ssl, &buf[len], bytes - len);
		if (rlen < 1) {
			long errval;

			errval = SSL_get_error(ipc->ssl, rlen);
			if (errval == SSL_ERROR_WANT_READ ||
					errval == SSL_ERROR_WANT_WRITE) {
				sleep(1);
				continue;
			}
/***
 Not sure why we'd want to handle these error codes any differently,
 but this definitely isn't the way to handle them.  Someone must have
 naively assumed that we could fall back to unencrypted communications,
 but all it does is just recursively blow the stack.
			if (errval == SSL_ERROR_ZERO_RETURN ||
					errval == SSL_ERROR_SSL) {
				serv_read(ipc, &buf[len], bytes - len);
				return;
			}
 ***/
			error_printf("SSL_read in serv_read: %s\n",
					ERR_reason_error_string(ERR_peek_error()));
			connection_died(ipc, 1);
			return;
		}
		len += rlen;
	}
}


/*
 * send binary to server encrypted
 */
static void serv_write_ssl(CtdlIPC *ipc, const char *buf, unsigned int nbytes)
{
	unsigned int bytes_written = 0;
	int retval;
	char junk[1];

	while (bytes_written < nbytes) {
		if (SSL_want_write(ipc->ssl)) {
			if ((SSL_read(ipc->ssl, junk, 0)) < 1) {
				error_printf("SSL_read in serv_write:\n");
				ERR_print_errors_fp(stderr);
			}
		}
		retval = SSL_write(ipc->ssl, &buf[bytes_written],
				nbytes - bytes_written);
		if (retval < 1) {
			long errval;

			errval = SSL_get_error(ipc->ssl, retval);
			if (errval == SSL_ERROR_WANT_READ ||
					errval == SSL_ERROR_WANT_WRITE) {
				sleep(1);
				continue;
			}
			if (errval == SSL_ERROR_ZERO_RETURN ||
					errval == SSL_ERROR_SSL) {
				serv_write(ipc, &buf[bytes_written],
						nbytes - bytes_written);
				return;
			}
			error_printf("SSL_write in serv_write: %s\n",
					ERR_reason_error_string(ERR_peek_error()));
			connection_died(ipc, 1);
			return;
		}
		bytes_written += retval;
	}
}


#ifdef THREADED_CLIENT
static void ssl_lock(int mode, int n, const char *file, int line)
{
	if (mode & CRYPTO_LOCK)
		pthread_mutex_lock(Critters[n]);
	else
		pthread_mutex_unlock(Critters[n]);
}
#endif /* THREADED_CLIENT */


static void CtdlIPC_init_OpenSSL(void)
{
	int a;
	SSL_METHOD *ssl_method;
	DH *dh;
	
	/* already done init */
	if (ssl_ctx) {
		return;
	}

	/* Get started */
	a = 0;
	ssl_ctx = NULL;
	dh = NULL;
	SSL_load_error_strings();
	SSLeay_add_ssl_algorithms();

	/* Set up the SSL context in which we will oeprate */
	ssl_method = SSLv23_client_method();
	ssl_ctx = SSL_CTX_new(ssl_method);
	if (!ssl_ctx) {
		error_printf("SSL_CTX_new failed: %s\n",
				ERR_reason_error_string(ERR_get_error()));
		return;
	}
	/* Any reasonable cipher we can get */
	if (!(SSL_CTX_set_cipher_list(ssl_ctx, CIT_CIPHERS))) {
		error_printf("No ciphers available for encryption\n");
		return;
	}
	SSL_CTX_set_session_cache_mode(ssl_ctx, SSL_SESS_CACHE_BOTH);
	
	/* Load DH parameters into the context */
	dh = DH_new();
	if (!dh) {
		error_printf("Can't allocate a DH object: %s\n",
				ERR_reason_error_string(ERR_get_error()));
		return;
	}
	if (!(BN_hex2bn(&(dh->p), DH_P))) {
		error_printf("Can't assign DH_P: %s\n",
				ERR_reason_error_string(ERR_get_error()));
		DH_free(dh);
		return;
	}
	if (!(BN_hex2bn(&(dh->g), DH_G))) {
		error_printf("Can't assign DH_G: %s\n",
				ERR_reason_error_string(ERR_get_error()));
		DH_free(dh);
		return;
	}
	dh->length = DH_L;
	SSL_CTX_set_tmp_dh(ssl_ctx, dh);
	DH_free(dh);

#ifdef THREADED_CLIENT
	/* OpenSSL requires callbacks for threaded clients */
	CRYPTO_set_locking_callback(ssl_lock);
	CRYPTO_set_id_callback(id_callback);

	/* OpenSSL requires us to do semaphores for threaded clients */
	Critters = malloc(CRYPTO_num_locks() * sizeof (pthread_mutex_t *));
	if (!Critters) {
		perror("malloc failed");
		exit(1);
	} else {
		for (a = 0; a < CRYPTO_num_locks(); a++) {
			Critters[a] = malloc(sizeof (pthread_mutex_t));
			if (!Critters[a]) {
				perror("malloc failed");
				exit(1);
			}
			pthread_mutex_init(Critters[a], NULL);
		}
	}
#endif /* THREADED_CLIENT */       
}



#ifdef THREADED_CLIENT
static unsigned long id_callback(void) {
	return (unsigned long)pthread_self();
}
#endif /* THREADED_CLIENT */
#endif /* HAVE_OPENSSL */


int
ReadNetworkChunk(CtdlIPC* ipc)
{
	fd_set read_fd;
	int tries;
	int ret = 0;
	int err = 0;
	struct timeval tv;
	size_t n;

	tv.tv_sec = 1;
	tv.tv_usec = 1000;
	tries = 0;
	n = 0;
	while (1)
	{
		errno=0;
		FD_ZERO(&read_fd);
		FD_SET(ipc->sock, &read_fd);
		ret = select(ipc->sock+1, &read_fd, NULL, NULL,  &tv);
		
//		fprintf(stderr, "\nselect failed: %d %d %s\n", ret,  err, strerror(err));
		
		if (ret > 0) {
			
			*(ipc->BufPtr) = '\0';
			n = read(ipc->sock, ipc->BufPtr, ipc->BufSize  - (ipc->BufPtr - ipc->Buf) - 1);
			if (n > 0) {
				ipc->BufPtr[n]='\0';
				ipc->BufUsed += n;
				return n;
			}
			else 
				return n;
		}
		else if (ret < 0) {
			if (!(errno == EINTR || errno == EAGAIN))
				error_printf( "\nselect failed: %d %s\n", err, strerror(err));
			return -1;
		}/*
		else {
			tries ++;
			if (tries >= 10)
			n = read(ipc->sock, ipc->BufPtr, ipc->BufSize  - (ipc->BufPtr - ipc->Buf) - 1);
			if (n > 0) {
				ipc->BufPtr[n]='\0';
				ipc->BufUsed += n;
				return n;
			}
			else {
				connection_died(ipc, 0);
				return -1;
			}
			}*/
	}
}

/*
 * input string from socket - implemented in terms of serv_read()
 */
static void CtdlIPC_getline(CtdlIPC* ipc, char *buf)
{
	int i, ntries;
	char *aptr, *bptr, *aeptr, *beptr;

//	error_printf("---\n");

	beptr = buf + SIZ;
#if defined(HAVE_OPENSSL)
 	if (ipc->ssl) {
		
		/* Read one character at a time. */
 		for (i = 0;; i++) {
 			serv_read(ipc, &buf[i], 1);
 			if (buf[i] == '\n' || i == (SIZ-1))
 				break;
 		}
		
		/* If we got a long line, discard characters until the newline. */
 		if (i == (SIZ-1))
 			while (buf[i] != '\n')
 				serv_read(ipc, &buf[i], 1);
		
		/* Strip the trailing newline (and carriage return, if present) */
 		if (i>=0 && buf[i] == 10) buf[i--] = 0;
 		if (i>=0 && buf[i] == 13) buf[i--] = 0;
 	}
 	else
#endif
	{
		if (ipc->Buf == NULL)
		{
			ipc->BufSize = SIZ;
			ipc->Buf = (char*) malloc(ipc->BufSize + 10);
			*(ipc->Buf) = '\0';
			ipc->BufPtr = ipc->Buf;
		}

		ntries = 0;
//		while ((ipc->BufUsed == 0)||(ntries++ > 10))
		if (ipc->BufUsed == 0)
			ReadNetworkChunk(ipc);

////		if (ipc->BufUsed != 0) while (1)
		bptr = buf;

		while (1)
		{
			aptr = ipc->BufPtr;
			aeptr = ipc->Buf + ipc->BufSize;
			while ((aptr < aeptr) && 
			       (bptr < beptr) &&
			       (*aptr != '\0') && 
			       (*aptr != '\n'))
				*(bptr++) = *(aptr++);
			if ((*aptr == '\n') && (aptr < aeptr))
			{
				/* Terminate it right, remove the line breaks */
				while ((aptr < aeptr) && ((*aptr == '\n') || (*aptr == '\r')))
					aptr ++;
				while ((aptr < aeptr ) && (*(aptr + 1) == '\0') )
					aptr ++;
				*(bptr++) = '\0';
//				fprintf(stderr, "parsing %d %d %d - %d %d %d %s\n", ipc->BufPtr - ipc->Buf, aptr - ipc->BufPtr, ipc->BufUsed , *aptr, *(aptr-1), *(aptr+1), buf);
				if ((bptr > buf + 1) && (*(bptr-1) == '\r'))
					*(--bptr) = '\0';
				
				/* is there more in the buffer we need to read later? */
				if (ipc->Buf + ipc->BufUsed > aptr)
				{
					ipc->BufPtr = aptr;
				}
				else
				{
					ipc->BufUsed = 0;
					ipc->BufPtr = ipc->Buf;
				}
//				error_printf("----bla6\n");
				return;
				
			}/* should we move our read stuf to the bufferstart so we have more space at the end? */
			else if ((ipc->BufPtr != ipc->Buf) && 
				 (ipc->BufUsed > (ipc->BufSize  - (ipc->BufSize / 4))))
			{
				size_t NewBufSize = ipc->BufSize * 2;
				int delta = (ipc->BufPtr - ipc->Buf);
				char *NewBuf;

				/* if the line would end after our buffer, we should use a bigger buffer. */
				NewBuf = (char *)malloc (NewBufSize + 10);
				memcpy (NewBuf, ipc->BufPtr, ipc->BufUsed - delta);
				free(ipc->Buf);
				ipc->Buf = ipc->BufPtr = NewBuf;
				ipc->BufUsed -= delta;
				ipc->BufSize = NewBufSize;
			}
			if (ReadNetworkChunk(ipc) <0)
			{
//				error_printf("----bla\n");
				return;
			}
		}
///		error_printf("----bl45761%s\nipc->BufUsed");
	}
//	error_printf("----bla1\n");
}

void CtdlIPC_chat_recv(CtdlIPC* ipc, char* buf)
{
	CtdlIPC_getline(ipc, buf);
}

/*
 * send line to server - implemented in terms of serv_write()
 */
static void CtdlIPC_putline(CtdlIPC *ipc, const char *buf)
{
	char *cmd = NULL;
	int len;

	len = strlen(buf);
	cmd = malloc(len + 2);
	if (!cmd) {
		/* This requires no extra memory */
		serv_write(ipc, buf, len);
		serv_write(ipc, "\n", 1);
	} else {
		/* This is network-optimized */
		strncpy(cmd, buf, len);
		strcpy(cmd + len, "\n");
		serv_write(ipc, cmd, len + 1);
		free(cmd);
	}

	ipc->last_command_sent = time(NULL);
}

void CtdlIPC_chat_send(CtdlIPC* ipc, const char* buf)
{
	CtdlIPC_putline(ipc, buf);
}


/*
 * attach to server
 */
CtdlIPC* CtdlIPC_new(int argc, char **argv, char *hostbuf, char *portbuf)
{
	int a;
	char cithost[SIZ];
	char citport[SIZ];
	char sockpath[SIZ];
	CtdlIPC* ipc;

	ipc = ialloc(CtdlIPC);
	if (!ipc) {
		return 0;
	}
#if defined(HAVE_OPENSSL)
	ipc->ssl = NULL;
	CtdlIPC_init_OpenSSL();
#endif
#if defined(HAVE_PTHREAD_H)
	pthread_mutex_init(&(ipc->mutex), NULL); /* Default fast mutex */
#endif
	ipc->sock = -1;			/* Not connected */
	ipc->isLocal = 0;		/* Not local, of course! */
	ipc->downloading = 0;
	ipc->uploading = 0;
	ipc->last_command_sent = 0L;
	ipc->network_status_cb = NULL;
	ipc->Buf = NULL;
	ipc->BufUsed = 0;
	ipc->BufPtr = NULL;

	strcpy(cithost, DEFAULT_HOST);	/* default host */
	strcpy(citport, DEFAULT_PORT);	/* default port */

	/* Allow caller to supply our values (Windows) */
	if (hostbuf && strlen(hostbuf) > 0)
		strcpy(cithost, hostbuf);
	if (portbuf && strlen(portbuf) > 0)
		strcpy(citport, portbuf);

	/* Read host/port from command line if present */
	for (a = 0; a < argc; ++a) {
		if (a == 0) {
			/* do nothing */
		} else if (a == 1) {
			strcpy(cithost, argv[a]);
		} else if (a == 2) {
			strcpy(citport, argv[a]);
		} else {
			error_printf("%s: usage: ",argv[0]);
			error_printf("%s [host] [port] ",argv[0]);
			ifree(ipc);
			errno = EINVAL;
			return 0;
   		}
	}

	if ((!strcmp(cithost, "localhost"))
	   || (!strcmp(cithost, "127.0.0.1"))) {
		ipc->isLocal = 1;
	}

	/* If we're using a unix domain socket we can do a bunch of stuff */
	if (!strcmp(cithost, UDS)) {
		if (!strcasecmp(citport, DEFAULT_PORT)) {
			snprintf(sockpath, sizeof sockpath, file_citadel_socket);
		}
		else {
			snprintf(sockpath, sizeof sockpath, "%s/%s", citport, "citadel.socket");
		}
		ipc->sock = uds_connectsock(&(ipc->isLocal), sockpath);
		if (ipc->sock == -1) {
			ifree(ipc);
			return 0;
		}
		if (hostbuf != NULL) strcpy(hostbuf, cithost);
		if (portbuf != NULL) strcpy(portbuf, sockpath);
		return ipc;
	}

	ipc->sock = connectsock(cithost, citport, "tcp", 504);
	if (ipc->sock == -1) {
		ifree(ipc);
		return 0;
	}
	if (hostbuf != NULL) strcpy(hostbuf, cithost);
	if (portbuf != NULL) strcpy(portbuf, citport);
	return ipc;
}


/*
 * Disconnect and delete the IPC class (destructor)
 */
void CtdlIPC_delete(CtdlIPC* ipc)
{
#ifdef HAVE_OPENSSL
	if (ipc->ssl) {
		SSL_shutdown(ipc->ssl);
		SSL_free(ipc->ssl);
		ipc->ssl = NULL;
	}
#endif
	if (ipc->sock > -1) {
		shutdown(ipc->sock, 2);	/* Close it up */
		ipc->sock = -1;
	}
	if (ipc->Buf != NULL)
		free (ipc->Buf);
	ipc->Buf = NULL;
	ipc->BufPtr = NULL;
	ifree(ipc);
}


/*
 * Disconnect and delete the IPC class (destructor)
 * Also NULLs out the pointer
 */
void CtdlIPC_delete_ptr(CtdlIPC** pipc)
{
	CtdlIPC_delete(*pipc);
	*pipc = NULL;
}


/*
 * return the file descriptor of the server socket so we can select() on it.
 *
 * FIXME: This is only used in chat mode; eliminate it when chat mode gets
 * rewritten...
 */
int CtdlIPC_getsockfd(CtdlIPC* ipc)
{
	return ipc->sock;
}


/*
 * return one character
 *
 * FIXME: This is only used in chat mode; eliminate it when chat mode gets
 * rewritten...
 */
char CtdlIPC_get(CtdlIPC* ipc)
{
	char buf[2];
	char ch;

	serv_read(ipc, buf, 1);
	ch = (int) buf[0];

	return (ch);
}
