int hash (char *str);
int getuser (struct usersupp *usbuf, char *name);
int lgetuser (struct usersupp *usbuf, char *name);
void putuser (struct usersupp *usbuf, char *name);
void lputuser (struct usersupp *usbuf, char *name);
int is_aide (void);
int is_room_aide (void);
int getuserbynumber (struct usersupp *usbuf, long int number);
void cmd_user (char *cmdbuf);
void session_startup (void);
void logout (struct CitContext *who);
void cmd_pass (char *buf);
int purge_user (char *pname);
int create_user (char *newusername);
void cmd_newu (char *cmdbuf);
void cmd_setp (char *new_pw);
void cmd_getu (void);
void cmd_setu (char *new_parms);
void cmd_slrp (char *new_ptr);
void cmd_invt_kick (char *iuser, int op);
void cmd_forg (void);
void cmd_gnur (void);
void cmd_greg (char *who);
void cmd_vali (char *v_args);
void ForEachUser(void (*CallBack)(struct usersupp *EachUser));
void ListThisUser(struct usersupp *usbuf);
void cmd_list (void);
void cmd_regi (void);
void cmd_chek (void);
void cmd_qusr (char *who);
void cmd_ebio (void);
void cmd_rbio (char *cmdbuf);
void cmd_lbio (void);
void cmd_agup (char *cmdbuf);
void cmd_asup (char *cmdbuf);
int NewMailCount(void);
void CtdlGetRelationship(struct visit *vbuf,
                        struct usersupp *rel_user,
                        struct quickroom *rel_room);
void CtdlSetRelationship(struct visit *newvisit,
                        struct usersupp *rel_user,
                        struct quickroom *rel_room);
void MailboxName(char *buf, struct usersupp *who, char *prefix);
