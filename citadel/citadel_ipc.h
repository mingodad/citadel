/* $Id$ */

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
	char mimetype[256];		/* MIME type */
	char disposition[256];		/* Content disposition */
	unsigned long length;		/* Content length */
};


struct ctdlipcmessage {
	char msgid[256];		/* Original message ID */
	char path[256];			/* Return path to sender */
	char zaps[256];			/* Message ID that this supersedes */
	char subject[256];		/* Message subject */
	char email[256];		/* Email address of sender */
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
};


struct ctdlipcmisc {
	long newmail;			/* Number of new Mail messages */
	char needregis;			/* Nonzero if user needs to register */
	char needvalid;			/* Nonzero if users need validation */
};

#ifdef __cplusplus
extern "C" {
#endif

int CtdlIPCNoop(void);
int CtdlIPCEcho(const char *arg, char *cret);
int CtdlIPCQuit(void);
int CtdlIPCLogout(void);
int CtdlIPCTryLogin(const char *username, char *cret);
int CtdlIPCTryPassword(const char *passwd, char *cret);
int CtdlIPCCreateUser(const char *username, char *cret);
int CtdlIPCChangePassword(const char *passwd, char *cret);
int CtdlIPCKnownRooms(int which, int floor, struct march **listing, char *cret);
int CtdlIPCGetConfig(struct usersupp **uret, char *cret);
int CtdlIPCSetConfig(struct usersupp *uret, char *cret);
int CtdlIPCGotoRoom(const char *room, const char *passwd,
		struct ctdlipcroom **rret, char *cret);
int CtdlIPCGetMessages(int which, int whicharg, const char *template,
		long **mret, char *cret);
int CtdlIPCGetSingleMessage(long msgnum, int headers, int as_mime,
		struct ctdlipcmessage **mret, char *cret);
int CtdlIPCWhoKnowsRoom(char **listing, char *cret);
int CtdlIPCServerInfo(char **listing, char *cret);
int CtdlIPCReadDirectory(char **listing, char *cret);
int CtdlIPCSetLastRead(long msgnum, char *cret);
int CtdlIPCInviteUserToRoom(const char *username, char *cret);
int CtdlIPCKickoutUserFromRoom(const char *username, char *cret);
int CtdlIPCGetRoomAttributes(struct quickroom **qret, char *cret);
int CtdlIPCSetRoomAttributes(int forget, struct quickroom *qret, char *cret);
int CtdlIPCGetRoomAide(char *cret);
int CtdlIPCSetRoomAide(const char *username, char *cret);
int CtdlIPCPostMessage(int flag, const struct ctdlipcmessage *mr, char *cret);
int CtdlIPCRoomInfo(char **iret, char *cret);
int CtdlIPCDeleteMessage(long msgnum, char *cret);
int CtdlIPCMoveMessage(int copy, long msgnum, const char *destroom, char *cret);
int CtdlIPCDeleteRoom(int for_real, char *cret);
int CtdlIPCCreateRoom(int for_real, const char *roomname, int type,
		const char *password, int floor, char *cret);
int CtdlIPCForgetRoom(char *cret);
int CtdlIPCSystemMessage(const char *message, char **mret, char *cret);
int CtdlIPCNextUnvalidatedUser(char *cret);
int CtdlIPCGetUserRegistration(const char *username, char **rret, char *cret);
int CtdlIPCValidateUser(const char *username, int axlevel, char *cret);
int CtdlIPCSetRoomInfo(int for_real, const char *info, char *cret);
int CtdlIPCUserListing(char **list, char *cret);
int CtdlIPCSetRegistration(const char *info, char *cret);
int CtdlIPCMiscCheck(struct ctdlipcmisc *chek, char *cret);
int CtdlIPCDeleteFile(const char *filename, char *cret);
int CtdlIPCMoveFile(const char *filename, const char *destroom, char *cret);
int CtdlIPCNetSendFile(const char *filename, const char *destnode, char *cret);
int CtdlIPCOnlineUsers(char **listing, time_t *stamp, char *cret);
int CtdlIPCFileDownload(const char *filename, void **buf, char *cret);
int CtdlIPCAttachmentDownload(long msgnum, const char *part, void **buf,
		char *cret);
int CtdlIPCImageDownload(const char *filename, void **buf, char *cret);
int CtdlIPCFileUpload(const char *filename, const char *comment, void *buf,
		size_t bytes, char *cret);
int CtdlIPCImageUpload(int for_real, const char *filename, size_t bytes,
		char *cret);
int CtdlIPCQueryUsername(const char *username, char *cret);
int CtdlIPCFloorListing(char **listing, char *cret);
int CtdlIPCCreateFloor(int for_real, const char *name, char *cret);
int CtdlIPCDeleteFloor(int for_real, int floornum, char *cret);
int CtdlIPCEditFloor(int floornum, const char *floorname, char *cret);
int CtdlIPCIdentifySoftware(int developerid, int clientid, int revision,
		const char *software_name, const char *hostname, char *cret);
int CtdlIPCSendInstantMessage(const char *username, const char *text,
		char *cret);
int CtdlIPCGetInstantMessage(char **listing, char *cret);
int CtdlIPCEnableInstantMessageReceipt(int mode, char *cret);
int CtdlIPCSetBio(char *bio, char *cret);
int CtdlIPCGetBio(const char *username, char **listing, char *cret);
int CtdlIPCListUsersWithBios(char **listing, char *cret);
int CtdlIPCStealthMode(int mode, char *cret);
int CtdlIPCTerminateSession(int sid, char *cret);
int CtdlIPCTerminateServerNow(char *cret);
int CtdlIPCTerminateServerScheduled(int mode, char *cret);
int CtdlIPCEnterSystemMessage(const char *filename, const char *text,
		char *cret);
int CtdlIPCChangeHostname(const char *hostname, char *cret);
int CtdlIPCChangeRoomname(const char *roomname, char *cret);
int CtdlIPCChangeUsername(const char *username, char *cret);
time_t CtdlIPCServerTime(char *crert);
int CtdlIPCAideGetUserParameters(struct usersupp **uret, char *cret);
int CtdlIPCAideSetUserParameters(const struct usersupp *uret, char *cret);
int CtdlIPCGetMessageExpirationPolicy(int which, char *cret);
int CtdlIPCSetMessageExpirationPolicy(int which, int policy, int value,
		char *cret);
int CtdlGetSystemConfig(char **listing, char *cret);
int CtdlSetSystemConfig(const char *listing, char *cret);
int CtdlIPCModerateMessage(long msgnum, int level, char *cret);
int CtdlIPCRequestClientLogout(int session, char *cret);
int CtdlIPCSetMessageSeen(long msgnum, int seen, char *cret);
int CtdlIPCStartEncryption(char *cret);

/* ************************************************************************** */
/*             Stuff below this line is not for public consumption            */
/* ************************************************************************** */

inline void netio_lock(void);
inline void netio_unlock(void);
char *CtdlIPCReadListing(char *dest);
int CtdlIPCSendListing(const char *listing);
size_t CtdlIPCPartialRead(void **buf, size_t offset, size_t bytes, char *cret);
int CtdlIPCEndUpload(char *cret);
int CtdlIPCWriteUpload(void *buf, size_t bytes, char *cret);
int CtdlIPCEndDownload(char *cret);
int CtdlIPCReadDownload(void **buf, size_t bytes, char *cret);
int CtdlIPCGenericCommand(const char *command, const char *to_send,
		size_t bytes_to_send, char **to_receive,
		size_t *bytes_to_receive, char *proto_response);

#ifdef __cplusplus
}
#endif
