/* $Id$ */
int hash (char *str);
int getuser (struct ctdluser *, char *);
int lgetuser (struct ctdluser *, char *);
void putuser (struct ctdluser *);
void lputuser (struct ctdluser *);
int is_aide (void);
int is_room_aide (void);
int getuserbynumber (struct ctdluser *usbuf, long int number);
void cmd_user (char *cmdbuf);
void session_startup (void);
void logout (struct CitContext *who);
void cmd_pass (char *buf);
int purge_user (char *pname);
int create_user (char *newusername, int become_user);
void do_login(void);
void cmd_newu (char *cmdbuf);
void cmd_creu (char *cmdbuf);
void cmd_setp (char *new_pw);
void cmd_getu (void);
void cmd_setu (char *new_parms);
void cmd_slrp (char *new_ptr);
int CtdlInvtKick(char *iuser, int op);
void cmd_invt_kick (char *iuser, int op);
void cmd_forg (void);
void cmd_gnur (void);
void cmd_vali (char *v_args);
void ForEachUser(void (*CallBack)(struct ctdluser *EachUser, void *out_data),
	void *in_data);
void ListThisUser(struct ctdluser *usbuf, void *data);
void cmd_list (char *);
void cmd_chek (void);
void cmd_qusr (char *who);
void cmd_agup (char *cmdbuf);
void cmd_asup (char *cmdbuf);
void cmd_view (char *cmdbuf);
void cmd_renu (char *cmdbuf);
int NewMailCount(void);
int InitialMailCheck(void);
void put_visit(struct visit *newvisit);
void CtdlGetRelationship(struct visit *vbuf,
                        struct ctdluser *rel_user,
                        struct ctdlroom *rel_room);
void CtdlSetRelationship(struct visit *newvisit,
                        struct ctdluser *rel_user,
                        struct ctdlroom *rel_room);
void MailboxName(char *buf, size_t n, const struct ctdluser *who,
		 const char *prefix);
int GenerateRelationshipIndex(  char *IndexBuf,
                                long RoomID,
                                long RoomGen,
                                long UserID);
int CtdlAssociateSystemUser(char *screenname, char *loginname);
int CtdlLoginExistingUser(char *authname, char *username);

/*
 * Values which may be returned by CtdlLoginExistingUser()
 */
enum {
	pass_ok,
	pass_already_logged_in,
	pass_no_user,
	pass_internal_error,
	pass_wrong_password
};




int CtdlTryPassword(char *password);

/*
 * Values which may be returned by CtdlTryPassword()
 */
enum {
	login_ok,
	login_already_logged_in,
	login_too_many_users,
	login_not_found
};


int CtdlForgetThisRoom(void);
void cmd_seen(char *argbuf);
void cmd_gtsn(char *argbuf);
void BumpNewMailCounter(long);
void start_chkpwd_daemon(void);


#define RENAMEUSER_OK			0	/* Operation succeeded */
#define RENAMEUSER_LOGGED_IN		1	/* Cannot rename a user who is currently logged in */
#define RENAMEUSER_NOT_FOUND		2	/* The old user name does not exist */
#define RENAMEUSER_ALREADY_EXISTS	3	/* An account with the desired new name already exists */

int rename_user(char *oldname, char *newname);
