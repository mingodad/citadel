/* $Id$ */

#include "sysdep.h"
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif
#ifdef HAVE_OPENSSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Quick and dirty hack; we don't want to use malloc() in C++ */
#ifdef __cplusplus
#define ialloc(t)	new t()
#define ifree(o)	delete o
#else
#define ialloc(t)	malloc(sizeof(t))
#define ifree(o)	free(o);
#endif

/* This class is responsible for the server connection */
typedef struct _CtdlIPC {
#if defined(HAVE_OPENSSL)
	/* NULL if not encrypted, non-NULL otherwise */
	SSL *ssl;
#endif
#if defined(HAVE_PTHREAD_H)
	/* Fast mutex, call CtdlIPC_lock() or CtdlIPC_unlock() to use */
	pthread_mutex_t mutex;
#endif
	/* -1 if not connected, >= 0 otherwise */
	int sock;
	/* 1 if server is local, 0 otherwise or if not connected */
	int isLocal;
	/* 1 if a download is open on the server, 0 otherwise */
	int downloading;
	/* 1 if an upload is open on the server, 0 otherwise */
	int uploading;
	/* Time the last command was sent to the server */
	time_t last_command_sent;
} CtdlIPC;

/* C constructor */
CtdlIPC* CtdlIPC_new(int argc, char **argv, char *hostbuf, char *portbuf);
/* C destructor */
void CtdlIPC_delete(CtdlIPC* ipc);
/* Convenience destructor; also nulls out caller's pointer */
void CtdlIPC_delete_ptr(CtdlIPC** pipc);
/* Read a line from server, discarding newline */
void CtdlIPC_getline(CtdlIPC* ipc, char *buf);
/* Write a line to server, adding newline */
void CtdlIPC_putline(CtdlIPC* ipc, const char *buf);

struct ctdlipcroom {
	char RRname[ROOMNAMELEN];	/* Name of room */
	long RRunread;			/* Number of unread messages */
	long RRtotal;			/* Total number of messages in room */
	char RRinfoupdated;		/* Nonzero if info was updated */
	unsigned RRflags;		/* Various flags (see LKRN) */
	long RRhighest;			/* Highest message number in room */
	long RRlastread;		/* Highest message user has read */
	char RRismailbox;		/* Is this room a mailbox room? */
	char RRaide;			/* User can do aide commands in room */
	long RRnewmail;			/* Number of new mail messages */
	char RRfloor;			/* Which floor this room is on */
};


struct parts {
	struct parts *next;
	char number[16];		/* part number */
	char name[PATH_MAX];		/* Name */
	char filename[PATH_MAX];	/* Suggested filename */
	char mimetype[SIZ];		/* MIME type */
	char disposition[SIZ];		/* Content disposition */
	unsigned long length;		/* Content length */
};


struct ctdlipcmessage {
	char msgid[SIZ];		/* Original message ID */
	char path[SIZ];			/* Return path to sender */
	char zaps[SIZ];			/* Message ID that this supersedes */
	char subject[SIZ];		/* Message subject */
	char email[SIZ];		/* Email address of sender */
	char author[USERNAME_SIZE];	/* Sender of message */
	char recipient[USERNAME_SIZE];	/* Recipient of message */
	char room[ROOMNAMELEN];		/* Originating room */
	char node[16];			/* Short nodename of origin system */
	char hnod[21];			/* Humannode of origin system */
	struct parts *attachments;	/* Available attachments */
	char *text;			/* Message text */
	int type;			/* Message type */
	time_t time;			/* Time message was posted */
	char nhdr;			/* Suppress message header? */
	char anonymous;			/* An anonymous message */
	char mime_chosen[SIZ];		/* Chosen MIME part to output */
	char content_type[SIZ];		/* How would you like that? */
};


struct ctdlipcmisc {
	long newmail;			/* Number of new Mail messages */
	char needregis;			/* Nonzero if user needs to register */
	char needvalid;			/* Nonzero if users need validation */
};

/* Shared Diffie-Hellman parameters */
#define DH_P		"1A74527AEE4EE2568E85D4FB2E65E18C9394B9C80C42507D7A6A0DBE9A9A54B05A9A96800C34C7AA5297095B69C88901EEFD127F969DCA26A54C0E0B5C5473EBAEB00957D2633ECAE3835775425DE66C0DE6D024DBB17445E06E6B0C78415E589B8814F08531D02FD43778451E7685541079CFFB79EF0D26EFEEBBB69D1E80383"
#define DH_G		"2"
#define DH_L		1024
#define CIT_CIPHERS	"ALL:RC4+RSA:+SSLv2:@STRENGTH"	/* see ciphers(1) */

int CtdlIPCNoop(CtdlIPC *ipc);
int CtdlIPCEcho(CtdlIPC *ipc, const char *arg, char *cret);
int CtdlIPCQuit(CtdlIPC *ipc);
int CtdlIPCLogout(CtdlIPC *ipc);
int CtdlIPCTryLogin(CtdlIPC *ipc, const char *username, char *cret);
int CtdlIPCTryPassword(CtdlIPC *ipc, const char *passwd, char *cret);
int CtdlIPCCreateUser(CtdlIPC *ipc, const char *username, int selfservice, char *cret);
int CtdlIPCChangePassword(CtdlIPC *ipc, const char *passwd, char *cret);
int CtdlIPCKnownRooms(CtdlIPC *ipc, int which, int floor, struct march **listing, char *cret);
int CtdlIPCGetConfig(CtdlIPC *ipc, struct usersupp **uret, char *cret);
int CtdlIPCSetConfig(CtdlIPC *ipc, struct usersupp *uret, char *cret);
int CtdlIPCGotoRoom(CtdlIPC *ipc, const char *room, const char *passwd,
		struct ctdlipcroom **rret, char *cret);
int CtdlIPCGetMessages(CtdlIPC *ipc, int which, int whicharg, const char *template,
		long **mret, char *cret);
int CtdlIPCGetSingleMessage(CtdlIPC *ipc, long msgnum, int headers, int as_mime,
		struct ctdlipcmessage **mret, char *cret);
int CtdlIPCWhoKnowsRoom(CtdlIPC *ipc, char **listing, char *cret);
int CtdlIPCServerInfo(CtdlIPC *ipc, struct CtdlServInfo *ServInfo, char *cret);
int CtdlIPCReadDirectory(CtdlIPC *ipc, char **listing, char *cret);
int CtdlIPCSetLastRead(CtdlIPC *ipc, long msgnum, char *cret);
int CtdlIPCInviteUserToRoom(CtdlIPC *ipc, const char *username, char *cret);
int CtdlIPCKickoutUserFromRoom(CtdlIPC *ipc, const char *username, char *cret);
int CtdlIPCGetRoomAttributes(CtdlIPC *ipc, struct quickroom **qret, char *cret);
int CtdlIPCSetRoomAttributes(CtdlIPC *ipc, int forget, struct quickroom *qret, char *cret);
int CtdlIPCGetRoomAide(CtdlIPC *ipc, char *cret);
int CtdlIPCSetRoomAide(CtdlIPC *ipc, const char *username, char *cret);
int CtdlIPCPostMessage(CtdlIPC *ipc, int flag, const struct ctdlipcmessage *mr, char *cret);
int CtdlIPCRoomInfo(CtdlIPC *ipc, char **iret, char *cret);
int CtdlIPCDeleteMessage(CtdlIPC *ipc, long msgnum, char *cret);
int CtdlIPCMoveMessage(CtdlIPC *ipc, int copy, long msgnum, const char *destroom, char *cret);
int CtdlIPCDeleteRoom(CtdlIPC *ipc, int for_real, char *cret);
int CtdlIPCCreateRoom(CtdlIPC *ipc, int for_real, const char *roomname, int type,
		const char *password, int floor, char *cret);
int CtdlIPCForgetRoom(CtdlIPC *ipc, char *cret);
int CtdlIPCSystemMessage(CtdlIPC *ipc, const char *message, char **mret, char *cret);
int CtdlIPCNextUnvalidatedUser(CtdlIPC *ipc, char *cret);
int CtdlIPCGetUserRegistration(CtdlIPC *ipc, const char *username, char **rret, char *cret);
int CtdlIPCValidateUser(CtdlIPC *ipc, const char *username, int axlevel, char *cret);
int CtdlIPCSetRoomInfo(CtdlIPC *ipc, int for_real, const char *info, char *cret);
int CtdlIPCUserListing(CtdlIPC *ipc, char **list, char *cret);
int CtdlIPCSetRegistration(CtdlIPC *ipc, const char *info, char *cret);
int CtdlIPCMiscCheck(CtdlIPC *ipc, struct ctdlipcmisc *chek, char *cret);
int CtdlIPCDeleteFile(CtdlIPC *ipc, const char *filename, char *cret);
int CtdlIPCMoveFile(CtdlIPC *ipc, const char *filename, const char *destroom, char *cret);
int CtdlIPCNetSendFile(CtdlIPC *ipc, const char *filename, const char *destnode, char *cret);
int CtdlIPCOnlineUsers(CtdlIPC *ipc, char **listing, time_t *stamp, char *cret);
int CtdlIPCFileDownload(CtdlIPC *ipc, const char *filename, void **buf,
		void (*progress_gauge_callback)(long, long), char *cret);
int CtdlIPCAttachmentDownload(CtdlIPC *ipc, long msgnum, const char *part, void **buf,
		void (*progress_gauge_callback)(long, long), char *cret);
int CtdlIPCImageDownload(CtdlIPC *ipc, const char *filename, void **buf,
		void (*progress_gauge_callback)(long, long), char *cret);
int CtdlIPCFileUpload(CtdlIPC *ipc, const char *save_as, const char *comment,
		const char *path, void (*progress_gauge_callback)(long, long),
		char *cret);
int CtdlIPCImageUpload(CtdlIPC *ipc, int for_real, const char *path,
		const char *save_as,
		void (*progress_gauge_callback)(long, long), char *cret);
int CtdlIPCQueryUsername(CtdlIPC *ipc, const char *username, char *cret);
int CtdlIPCFloorListing(CtdlIPC *ipc, char **listing, char *cret);
int CtdlIPCCreateFloor(CtdlIPC *ipc, int for_real, const char *name, char *cret);
int CtdlIPCDeleteFloor(CtdlIPC *ipc, int for_real, int floornum, char *cret);
int CtdlIPCEditFloor(CtdlIPC *ipc, int floornum, const char *floorname, char *cret);
int CtdlIPCIdentifySoftware(CtdlIPC *ipc, int developerid, int clientid, int revision,
		const char *software_name, const char *hostname, char *cret);
int CtdlIPCSendInstantMessage(CtdlIPC *ipc, const char *username, const char *text,
		char *cret);
int CtdlIPCGetInstantMessage(CtdlIPC *ipc, char **listing, char *cret);
int CtdlIPCEnableInstantMessageReceipt(CtdlIPC *ipc, int mode, char *cret);
int CtdlIPCSetBio(CtdlIPC *ipc, char *bio, char *cret);
int CtdlIPCGetBio(CtdlIPC *ipc, const char *username, char **listing, char *cret);
int CtdlIPCListUsersWithBios(CtdlIPC *ipc, char **listing, char *cret);
int CtdlIPCStealthMode(CtdlIPC *ipc, int mode, char *cret);
int CtdlIPCTerminateSession(CtdlIPC *ipc, int sid, char *cret);
int CtdlIPCTerminateServerNow(CtdlIPC *ipc, char *cret);
int CtdlIPCTerminateServerScheduled(CtdlIPC *ipc, int mode, char *cret);
int CtdlIPCEnterSystemMessage(CtdlIPC *ipc, const char *filename, const char *text,
		char *cret);
int CtdlIPCChangeHostname(CtdlIPC *ipc, const char *hostname, char *cret);
int CtdlIPCChangeRoomname(CtdlIPC *ipc, const char *roomname, char *cret);
int CtdlIPCChangeUsername(CtdlIPC *ipc, const char *username, char *cret);
time_t CtdlIPCServerTime(CtdlIPC *ipc, char *crert);
int CtdlIPCAideGetUserParameters(CtdlIPC *ipc, const char *who,
				 struct usersupp **uret, char *cret);
int CtdlIPCAideSetUserParameters(CtdlIPC *ipc, const struct usersupp *uret, char *cret);
int CtdlIPCGetMessageExpirationPolicy(CtdlIPC *ipc, int which, char *cret);
int CtdlIPCSetMessageExpirationPolicy(CtdlIPC *ipc, int which, int policy, int value,
		char *cret);
int CtdlGetSystemConfig(CtdlIPC *ipc, char **listing, char *cret);
int CtdlSetSystemConfig(CtdlIPC *ipc, const char *listing, char *cret);
int CtdlGetSystemConfigByType(CtdlIPC *ipc, const char *mimetype,
	       	char **listing, char *cret);
int CtdlSetSystemConfigByType(CtdlIPC *ipc, const char *mimetype,
	       const char *listing, char *cret);
int CtdlIPCModerateMessage(CtdlIPC *ipc, long msgnum, int level, char *cret);
int CtdlIPCRequestClientLogout(CtdlIPC *ipc, int session, char *cret);
int CtdlIPCSetMessageSeen(CtdlIPC *ipc, long msgnum, int seen, char *cret);
int CtdlIPCStartEncryption(CtdlIPC *ipc, char *cret);
int CtdlIPCDirectoryLookup(CtdlIPC *ipc, const char *address, char *cret);
int CtdlIPCSpecifyPreferredFormats(CtdlIPC *ipc, char *cret, char *formats);
int CtdlIPCInternalProgram(CtdlIPC *ipc, int secret, char *cret);

/* ************************************************************************** */
/*             Stuff below this line is not for public consumption            */
/* ************************************************************************** */

inline void CtdlIPC_lock(CtdlIPC *ipc);
inline void CtdlIPC_unlock(CtdlIPC *ipc);
char *CtdlIPCReadListing(CtdlIPC *ipc, char *dest);
int CtdlIPCSendListing(CtdlIPC *ipc, const char *listing);
size_t CtdlIPCPartialRead(CtdlIPC *ipc, void **buf, size_t offset, size_t bytes, char *cret);
int CtdlIPCEndUpload(CtdlIPC *ipc, int discard, char *cret);
int CtdlIPCWriteUpload(CtdlIPC *ipc, const char *path,
		void (*progress_gauge_callback)(long, long), char *cret);
int CtdlIPCEndDownload(CtdlIPC *ipc, char *cret);
int CtdlIPCReadDownload(CtdlIPC *ipc, void **buf, size_t bytes,
	       void (*progress_gauge_callback)(long, long), char *cret);
int CtdlIPCHighSpeedReadDownload(CtdlIPC *ipc, void **buf, size_t bytes,
	       void (*progress_gauge_callback)(long, long), char *cret);
int CtdlIPCGenericCommand(CtdlIPC *ipc, const char *command, const char *to_send,
		size_t bytes_to_send, char **to_receive,
		size_t *bytes_to_receive, char *proto_response);

/* Internals */
int starttls(CtdlIPC *ipc);
void setCryptoStatusHook(void (*hook)(char *s));
/* This is all Ford's doing.  FIXME: figure out what it's doing */
extern int (*error_printf)(char *s, ...);
void setIPCDeathHook(void (*hook)(void));
void setIPCErrorPrintf(int (*func)(char *s, ...));

#ifdef __cplusplus
}
#endif
