/* $Id$ */
int hash (char *str);
int getuser (struct usersupp *, char *);
int lgetuser (struct usersupp *, char *);
void putuser (struct usersupp *);
void lputuser (struct usersupp *);
int is_aide (void);
int is_room_aide (void);
int getuserbynumber (struct usersupp *usbuf, long int number);
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
void cmd_invt_kick (char *iuser, int op);
void cmd_forg (void);
void cmd_gnur (void);
void cmd_vali (char *v_args);
void ForEachUser(void (*CallBack)(struct usersupp *EachUser, void *out_data),
	void *in_data);
void ListThisUser(struct usersupp *usbuf, void *data);
void cmd_list (void);
void cmd_chek (void);
void cmd_qusr (char *who);
void cmd_agup (char *cmdbuf);
void cmd_asup (char *cmdbuf);
void cmd_view (char *cmdbuf);
int NewMailCount(void);
int InitialMailCheck(void);
void put_visit(struct visit *newvisit);
void CtdlGetRelationship(struct visit *vbuf,
                        struct usersupp *rel_user,
                        struct quickroom *rel_room);
void CtdlSetRelationship(struct visit *newvisit,
                        struct usersupp *rel_user,
                        struct quickroom *rel_room);
void MailboxName(char *buf, size_t n, const struct usersupp *who,
		 const char *prefix);
int GenerateRelationshipIndex(  char *IndexBuf,
                                long RoomID,
                                long RoomGen,
                                long UserID);

int CtdlLoginExistingUser(char *username);

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
