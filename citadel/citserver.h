/* $Id$ */
void master_startup (void);
void master_cleanup (void);
void cleanup_stuff (void *arg);
void set_wtmpsupp (char *newtext);
void set_wtmpsupp_to_current_room(void);
void cmd_info (void);
void cmd_rchg (char *newroomname);
void cmd_hchg (char *newhostname);
void cmd_uchg (char *newusername);
void cmd_time (void);
int is_public_client (char *where);
void cmd_iden (char *argbuf);
void cmd_stel (char *cmdbuf);
void cmd_mesg (char *mname);
void cmd_emsg (char *mname);
void cmd_rwho (void);
void cmd_term (char *cmdbuf);
void cmd_more (void);
void cmd_echo (char *etext);
void cmd_ipgm (char *argbuf);
void cmd_down (void);
void cmd_scdn (char *argbuf);
void cmd_extn (char *argbuf);
void *context_loop (struct CitContext *con);
void deallocate_user_data(struct CitContext *con);
void *CtdlGetUserData(unsigned long requested_sym);
void CtdlAllocUserData(unsigned long requested_sym, size_t num_bytes);
int CtdlGetDynamicSymbol(void);
void enter_housekeeping_cmd(char *);


extern int do_defrag;
