#ifndef __USER_OPS_H__
#define __USER_OPS_H__

#include <ctype.h>
#include <syslog.h>

int hash (char *str);
int is_aide (void);
int is_room_aide (void);
int CtdlCheckInternetMailPermission(struct ctdluser *who);
void rebuild_usersbynumber(void);
void session_startup (void);
void logged_in_response(void);
int purge_user (char *pname);
int create_user (const char *newusername, long len, int become_user);
void do_login(void);
int CtdlInvtKick(char *iuser, int op);
void ForEachUser(void (*CallBack)(struct ctdluser *EachUser, void *out_data),
	void *in_data);
void ListThisUser(struct ctdluser *usbuf, void *data);
int NewMailCount(void);
int InitialMailCheck(void);
void put_visit(visit *newvisit);
/* MailboxName is deprecated us CtdlMailboxName instead */
void MailboxName(char *buf, size_t n, const struct ctdluser *who,
		 const char *prefix) __attribute__ ((deprecated));
int GenerateRelationshipIndex(  char *IndexBuf,
                                long RoomID,
                                long RoomGen,
                                long UserID);
int CtdlAssociateSystemUser(char *screenname, char *loginname);




void CtdlSetPassword(char *new_pw);

int CtdlForgetThisRoom(void);

void cmd_newu (char *cmdbuf);
void start_chkpwd_daemon(void);


#define RENAMEUSER_OK			0	/* Operation succeeded */
#define RENAMEUSER_LOGGED_IN		1	/* Cannot rename a user who is currently logged in */
#define RENAMEUSER_NOT_FOUND		2	/* The old user name does not exist */
#define RENAMEUSER_ALREADY_EXISTS	3	/* An account with the desired new name already exists */

int rename_user(char *oldname, char *newname);

///#ifndef CTDL_INLINE_USR
////#define CTDL_INLINE_USR static INLINE
///#endif

///CTDL_INLINE_USR 
static INLINE long cutuserkey(char *username) { 
	long len;
	len = strlen(username);
	if (len >= USERNAME_SIZE)
	{
		syslog(LOG_INFO, "Username too long: %s", username);
		cit_backtrace ();
		len = USERNAME_SIZE - 1; 
		username[len]='\0';
	}
	return len;
}

/*
 * makeuserkey() - convert a username into the format used as a database key
 *		 (it's just the username converted into lower case)
 */
///CTDL_INLINE_USR 
static INLINE void makeuserkey(char *key, const char *username, long len) {
	int i;

	if (len >= USERNAME_SIZE)
	{
		syslog(LOG_INFO, "Username too long: %s", username);
		cit_backtrace ();
		len = USERNAME_SIZE - 1; 
	}
	for (i=0; i<=len; ++i) {
		key[i] = tolower(username[i]);
	}
}


int internal_create_user (const char *username, long len, struct ctdluser *usbuf, uid_t uid);

#endif
