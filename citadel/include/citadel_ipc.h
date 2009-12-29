/* $Id$ */

#define	UDS			"_UDS_"
#ifdef __CYGWIN__
#define DEFAULT_HOST		"localhost"
#else
#define DEFAULT_HOST		UDS
#endif
#define DEFAULT_PORT		"citadel"

#include "sysdep.h"
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif
#ifdef HAVE_OPENSSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#endif

#include "server.h"

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

struct CtdlServInfo {
	int pid;
	char nodename[32];
	char humannode[64];
	char fqdn[64];
	char software[64];
	int rev_level;
	char site_location[64];
	char sysadm[64];
	char moreprompt[256];
	int ok_floors;
	int paging_level;
	int supports_qnop;
	int supports_ldap;
	int newuser_disabled;
	char default_cal_zone[256];
	double load_avg;
	double worker_avg;
	int thread_count;
	int has_sieve;
	int fulltext_enabled;
	char svn_revision[256];
};

/* This class is responsible for the server connection */
typedef struct _CtdlIPC {
	/* The server info for this connection */
	struct CtdlServInfo ServInfo;

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
	/* Our buffer for linebuffered read. */
	char *Buf;
	size_t BufSize;
	size_t BufUsed;
	char *BufPtr;
	/* Callback for update on whether the IPC is locked */
	void (*network_status_cb)(int state);
} CtdlIPC;

/* C constructor */
CtdlIPC* CtdlIPC_new(int argc, char **argv, char *hostbuf, char *portbuf);
/* C destructor */
void CtdlIPC_delete(CtdlIPC* ipc);
/* Convenience destructor; also nulls out caller's pointer */
void CtdlIPC_delete_ptr(CtdlIPC** pipc);
/* Read a line from server, discarding newline, for chat, will go away */
void CtdlIPC_chat_recv(CtdlIPC* ipc, char *buf);
/* Write a line to server, adding newline, for chat, will go away */
void CtdlIPC_chat_send(CtdlIPC* ipc, const char *buf);

struct ctdlipcroom {
	char RRname[ROOMNAMELEN];	/* Name of room */
	long RRunread;			/* Number of unread messages */
	long RRtotal;			/* Total number of messages in room */
	char RRinfoupdated;		/* Nonzero if info was updated */
	unsigned RRflags;		/* Various flags (see LKRN) */
	unsigned RRflags2;		/* Various flags (see LKRN) */
	long RRhighest;			/* Highest message number in room */
	long RRlastread;		/* Highest message user has read */
	char RRismailbox;		/* Is this room a mailbox room? */
	char RRaide;			/* User can do aide commands in room */
	long RRnewmail;			/* Number of new mail messages */
	char RRfloor;			/* Which floor this room is on */
	char RRcurrentview;		/* The user's current view for this room */
	char RRdefaultview;		/* The default view for this room */
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
	char author[SIZ];		/* Sender of message */
	char recipient[SIZ];		/* Recipient of message */
	char room[SIZ];			/* Originating room */
	char node[SIZ];			/* Short nodename of origin system */
	char hnod[SIZ];			/* Humannode of origin system */
	struct parts *attachments;	/* Available attachments */
	char *text;			/* Message text */
	int type;			/* Message type */
	time_t time;			/* Time message was posted */
	char nhdr;			/* Suppress message header? */
	char anonymous;			/* An anonymous message */
	char mime_chosen[SIZ];		/* Chosen MIME part to output */
	char content_type[SIZ];		/* How would you like that? */
	char references[SIZ];		/* Thread references */
};


struct ctdlipcfile {
	char remote_name[PATH_MAX];	/* Filename on server */
	char local_name[PATH_MAX];	/* Filename on client */
	char description[80];		/* Description on server */
	FILE *local_fd;			/* Open file on client */
	size_t size;			/* Size of file in octets */
	unsigned int upload:1;		/* uploading? 0 if downloading */
	unsigned int complete:1;	/* Transfer has finished? */
};


struct ctdlipcmisc {
	long newmail;			/* Number of new Mail messages */
	char needregis;			/* Nonzero if user needs to register */
	char needvalid;			/* Nonzero if users need validation */
};

enum RoomList {
	SubscribedRooms,
	SubscribedRoomsWithNewMessages,
	SubscribedRoomsWithNoNewMessages,
	UnsubscribedRooms,
	AllAccessibleRooms,
	AllPublicRooms
};
#define AllFloors -1
enum MessageList {
	AllMessages,
	OldMessages,
	NewMessages,
	LastMessages,
	FirstMessages,
	MessagesGreaterThan,
	MessagesLessThan
};
enum MessageDirection {
	ReadReverse = -1,
	ReadForward = 1
};

/* Shared Diffie-Hellman parameters */
#define DH_P		"F6E33BD70D475906ABCFB368DA2D1E5611D57DFDAC6A10CD78F406D6952519C74E21FFDCC5A780B9359722AACC8036E4CD24D5F5165EAC9EF226DBD9BBCF678F8DDEE86386F1BC20E291A9854A513A2CA326B411DC92E38F2ED2FEB6A3B792F13DB6550371FDBAC5ECA373BE5050CA4905431CA86088737D52B36C8D13CE9CB4EEF4C910285035E8329DD07551A80B87775676DD1067395CCEE9040C9B8BF998C528B3772B4C590A2CF18C5E58929BFCB538A62638B7437A9C68572D15287E97692B0B1EC5444D9DAB6EB062D20B79CA005EC5035065567AFD1FEF9B251D74747C6065D8C8B6B0862D1EE03F3A244C429EADE0CCC5C3A4196F5CBF5AA01A9026EFB20AA90E462BD64620278F271905EB604F38E6CFAE412EAA6C468E3B58170909BC18662FE2053224F30BE4FDB93BF9FBF969D91A5427A0665AC7BD1C43701B991094C92F7A935063055617142164F02973EB4ED86DD74D2BBAB3CD3B28F7BBD8D9F925B0FE92F7F7D0568D783F9ECE7AF96FB5AF274B586924B64639733A73ACA8F2BD1E970DF51ADDD983F7F6361A2B0DC4F086DE26D8656EC8813DE4B74D6D57BC1E690AC2FF1682B7E16938565A41D1DC64C75ADB81DA4582613FC68C0FDD327D35E2CDF20D009465303773EF3870FBDB0985EE7002A95D7912BBCC78187C29DB046763B7FABFF44EABE820F8ED0D7230AA0AF24F428F82448345BC099B"
#define DH_G		"2"
#define DH_L		4096
#define CIT_CIPHERS	"ALL:RC4+RSA:+SSLv2:+TLSv1:!MD5:@STRENGTH"	/* see ciphers(1) */

int CtdlIPCNoop(CtdlIPC *ipc);
int CtdlIPCEcho(CtdlIPC *ipc, const char *arg, char *cret);
int CtdlIPCQuit(CtdlIPC *ipc);
int CtdlIPCLogout(CtdlIPC *ipc);
int CtdlIPCTryLogin(CtdlIPC *ipc, const char *username, char *cret);
int CtdlIPCTryPassword(CtdlIPC *ipc, const char *passwd, char *cret);
int CtdlIPCTryApopPassword(CtdlIPC *ipc, const char *response, char *cret);
int CtdlIPCCreateUser(CtdlIPC *ipc, const char *username, int selfservice,
		char *cret);
int CtdlIPCChangePassword(CtdlIPC *ipc, const char *passwd, char *cret);
int CtdlIPCKnownRooms(CtdlIPC *ipc, enum RoomList which, int floor,
		struct march **listing, char *cret);
int CtdlIPCGetConfig(CtdlIPC *ipc, struct ctdluser **uret, char *cret);
int CtdlIPCSetConfig(CtdlIPC *ipc, struct ctdluser *uret, char *cret);
int CtdlIPCGotoRoom(CtdlIPC *ipc, const char *room, const char *passwd,
		struct ctdlipcroom **rret, char *cret);
int CtdlIPCGetMessages(CtdlIPC *ipc, enum MessageList which, int whicharg,
		const char *mtemplate, unsigned long **mret, char *cret);
int CtdlIPCGetSingleMessage(CtdlIPC *ipc, long msgnum, int headers, int as_mime,
		struct ctdlipcmessage **mret, char *cret);
int CtdlIPCWhoKnowsRoom(CtdlIPC *ipc, char **listing, char *cret);
int CtdlIPCServerInfo(CtdlIPC *ipc, char *cret);
/* int CtdlIPCReadDirectory(CtdlIPC *ipc, struct ctdlipcfile **files, char *cret); */
int CtdlIPCReadDirectory(CtdlIPC *ipc, char **listing, char *cret);
int CtdlIPCSetLastRead(CtdlIPC *ipc, long msgnum, char *cret);
int CtdlIPCInviteUserToRoom(CtdlIPC *ipc, const char *username, char *cret);
int CtdlIPCKickoutUserFromRoom(CtdlIPC *ipc, const char *username, char *cret);
int CtdlIPCGetRoomAttributes(CtdlIPC *ipc, struct ctdlroom **qret, char *cret);
int CtdlIPCSetRoomAttributes(CtdlIPC *ipc, int forget, struct ctdlroom *qret,
		char *cret);
int CtdlIPCGetRoomAide(CtdlIPC *ipc, char *cret);
int CtdlIPCSetRoomAide(CtdlIPC *ipc, const char *username, char *cret);
int CtdlIPCPostMessage(CtdlIPC *ipc, int flag, int *subject_required, 
					   struct ctdlipcmessage *mr,
					   char *cret);
int CtdlIPCRoomInfo(CtdlIPC *ipc, char **iret, char *cret);
int CtdlIPCDeleteMessage(CtdlIPC *ipc, long msgnum, char *cret);
int CtdlIPCMoveMessage(CtdlIPC *ipc, int copy, long msgnum,
		const char *destroom, char *cret);
int CtdlIPCDeleteRoom(CtdlIPC *ipc, int for_real, char *cret);
int CtdlIPCCreateRoom(CtdlIPC *ipc, int for_real, const char *roomname,
		int type, const char *password, int floor, char *cret);
int CtdlIPCForgetRoom(CtdlIPC *ipc, char *cret);
int CtdlIPCSystemMessage(CtdlIPC *ipc, const char *message, char **mret,
		char *cret);
int CtdlIPCNextUnvalidatedUser(CtdlIPC *ipc, char *cret);
int CtdlIPCGetUserRegistration(CtdlIPC *ipc, const char *username, char **rret,
		char *cret);
int CtdlIPCValidateUser(CtdlIPC *ipc, const char *username, int axlevel,
		char *cret);
int CtdlIPCSetRoomInfo(CtdlIPC *ipc, int for_real, const char *info,
		char *cret);
int CtdlIPCUserListing(CtdlIPC *ipc, char *searchstring, char **list, char *cret);
int CtdlIPCSetRegistration(CtdlIPC *ipc, const char *info, char *cret);
int CtdlIPCMiscCheck(CtdlIPC *ipc, struct ctdlipcmisc *chek, char *cret);
int CtdlIPCDeleteFile(CtdlIPC *ipc, const char *filename, char *cret);
int CtdlIPCMoveFile(CtdlIPC *ipc, const char *filename, const char *destroom,
		char *cret);
int CtdlIPCNetSendFile(CtdlIPC *ipc, const char *filename,
		const char *destnode, char *cret);
int CtdlIPCOnlineUsers(CtdlIPC *ipc, char **listing, time_t *stamp, char *cret);
int CtdlIPCFileDownload(CtdlIPC *ipc, const char *filename, void **buf,
		size_t resume,
		void (*progress_gauge_callback)(CtdlIPC*, unsigned long, unsigned long),
		char *cret);
int CtdlIPCAttachmentDownload(CtdlIPC *ipc, long msgnum, const char *part,
		void **buf,
		void (*progress_gauge_callback)(CtdlIPC*, unsigned long, unsigned long),
		char *cret);
int CtdlIPCImageDownload(CtdlIPC *ipc, const char *filename, void **buf,
		void (*progress_gauge_callback)(CtdlIPC*, unsigned long, unsigned long),
		char *cret);
int CtdlIPCFileUpload(CtdlIPC *ipc, const char *save_as, const char *comment,
		const char *path, 
		void (*progress_gauge_callback)(CtdlIPC*, unsigned long, unsigned long),
		char *cret);
int CtdlIPCImageUpload(CtdlIPC *ipc, int for_real, const char *path,
		const char *save_as,
		void (*progress_gauge_callback)(CtdlIPC*, unsigned long, unsigned long),
		char *cret);
int CtdlIPCQueryUsername(CtdlIPC *ipc, const char *username, char *cret);
int CtdlIPCFloorListing(CtdlIPC *ipc, char **listing, char *cret);
int CtdlIPCCreateFloor(CtdlIPC *ipc, int for_real, const char *name,
		char *cret);
int CtdlIPCDeleteFloor(CtdlIPC *ipc, int for_real, int floornum, char *cret);
int CtdlIPCEditFloor(CtdlIPC *ipc, int floornum, const char *floorname,
		char *cret);
int CtdlIPCIdentifySoftware(CtdlIPC *ipc, int developerid, int clientid,
		int revision, const char *software_name, const char *hostname,
		char *cret);
int CtdlIPCSendInstantMessage(CtdlIPC *ipc, const char *username,
		const char *text, char *cret);
int CtdlIPCGetInstantMessage(CtdlIPC *ipc, char **listing, char *cret);
int CtdlIPCEnableInstantMessageReceipt(CtdlIPC *ipc, int mode, char *cret);
int CtdlIPCSetBio(CtdlIPC *ipc, char *bio, char *cret);
int CtdlIPCGetBio(CtdlIPC *ipc, const char *username, char **listing,
		char *cret);
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
				 struct ctdluser **uret, char *cret);
int CtdlIPCAideSetUserParameters(CtdlIPC *ipc, const struct ctdluser *uret, char *cret);
int CtdlIPCRenameUser(CtdlIPC *ipc, char *oldname, char *newname, char *cret);
int CtdlIPCGetMessageExpirationPolicy(CtdlIPC *ipc, int which,
		struct ExpirePolicy **policy, char *cret);
int CtdlIPCSetMessageExpirationPolicy(CtdlIPC *ipc, int which,
		struct ExpirePolicy *policy, char *cret);
int CtdlIPCGetSystemConfig(CtdlIPC *ipc, char **listing, char *cret);
int CtdlIPCSetSystemConfig(CtdlIPC *ipc, const char *listing, char *cret);
int CtdlIPCGetSystemConfigByType(CtdlIPC *ipc, const char *mimetype,
	       	char **listing, char *cret);
int CtdlIPCSetSystemConfigByType(CtdlIPC *ipc, const char *mimetype,
	       const char *listing, char *cret);
int CtdlIPCGetRoomNetworkConfig(CtdlIPC *ipc, char **listing, char *cret);
int CtdlIPCSetRoomNetworkConfig(CtdlIPC *ipc, const char *listing, char *cret);
int CtdlIPCRequestClientLogout(CtdlIPC *ipc, int session, char *cret);
int CtdlIPCSetMessageSeen(CtdlIPC *ipc, long msgnum, int seen, char *cret);
int CtdlIPCStartEncryption(CtdlIPC *ipc, char *cret);
int CtdlIPCDirectoryLookup(CtdlIPC *ipc, const char *address, char *cret);
int CtdlIPCSpecifyPreferredFormats(CtdlIPC *ipc, char *cret, char *formats);
int CtdlIPCInternalProgram(CtdlIPC *ipc, int secret, char *cret);
int CtdlIPCMessageBaseCheck(CtdlIPC *ipc, char **mret, char *cret);

/* ************************************************************************** */
/*             Stuff below this line is not for public consumption            */
/* ************************************************************************** */

INLINE void CtdlIPC_lock(CtdlIPC *ipc);
INLINE void CtdlIPC_unlock(CtdlIPC *ipc);
char *CtdlIPCReadListing(CtdlIPC *ipc, char *dest);
int CtdlIPCSendListing(CtdlIPC *ipc, const char *listing);
size_t CtdlIPCPartialRead(CtdlIPC *ipc, void **buf, size_t offset,
		size_t bytes, char *cret);
int CtdlIPCEndUpload(CtdlIPC *ipc, int discard, char *cret);
int CtdlIPCWriteUpload(CtdlIPC *ipc, FILE *uploadFP,
		void (*progress_gauge_callback)(CtdlIPC*, unsigned long, unsigned long),
		char *cret);
int CtdlIPCEndDownload(CtdlIPC *ipc, char *cret);
int CtdlIPCReadDownload(CtdlIPC *ipc, void **buf, size_t bytes, size_t resume,
		void (*progress_gauge_callback)(CtdlIPC*, unsigned long, unsigned long),
		char *cret);
int CtdlIPCHighSpeedReadDownload(CtdlIPC *ipc, void **buf, size_t bytes,
		size_t resume,
		void (*progress_gauge_callback)(CtdlIPC*, unsigned long, unsigned long),
		char *cret);
int CtdlIPCGenericCommand(CtdlIPC *ipc, const char *command,
		const char *to_send, size_t bytes_to_send, char **to_receive,
		size_t *bytes_to_receive, char *proto_response);

/* Internals */
int starttls(CtdlIPC *ipc);
void setCryptoStatusHook(void (*hook)(char *s));
void CtdlIPC_SetNetworkStatusCallback(CtdlIPC *ipc, void (*hook)(int state));
/* This is all Ford's doing.  FIXME: figure out what it's doing */
extern int (*error_printf)(char *s, ...);
void setIPCDeathHook(void (*hook)(void));
void setIPCErrorPrintf(int (*func)(char *s, ...));
void connection_died(CtdlIPC* ipc, int using_ssl);
int CtdlIPC_getsockfd(CtdlIPC* ipc);
char CtdlIPC_get(CtdlIPC* ipc);

#ifdef __cplusplus
}
#endif
