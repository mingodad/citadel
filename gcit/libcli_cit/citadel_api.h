/*

Creeping citadel api of death
Brian Costello
btx@calyx.net

*/

int init_session(int);

int cmd_noop(int sd);
int cmd_echo(int sd, char *echostr);
int cmd_quit(int sd);
int cmd_lout(int sd);

int cmd_user(int sd, char *username);
int cmd_pass(int sd, char *password, citadel_parms *parms);
int cmd_setp(int sd, char *password);

/* floorno < 0 = all floors listed */
int cmd_lkrn(int sd, citadel_list **list, int floorno);
int cmd_lkro(int sd, citadel_list **list, int floorno);
int cmd_lzrm(int sd, citadel_list **list, int floorno);
int cmd_lkra(int sd, citadel_list **list, int floorno);
int cmd_lrms(int sd, citadel_list **list, int floorno);

int cmd_getu(int sd, citadel_parms *parms);
int cmd_setu(int sd, int width, int height, int option_bits);
int cmd_goto(int sd, char *roomname, char *password, citadel_parms *parms);
int cmd_msgs(int sd, citadel_list **list, char *cmd, int number);
int cmd_msg0(int sd, citadel_list **list, int msgnum, int header_only);

int cmd_whok(int sd, citadel_list **list);	/* aide only */
int cmd_info(int sd, citadel_list **list);
int cmd_rdir(int sd, citadel_parms *parms, citadel_list **list);
int cmd_slrp(int sd, int msgnum, int highest, citadel_parms *parms);

int cmd_invt(int sd, char *username);		/* aide only */
int cmd_kick(int sd, char *username);
int cmd_getr(int sd, citadel_parms *parms);
int cmd_setr(int sd, char *roomname, char *password, char *directory, int flags, int bump, int floorno);

int cmd_geta(int sd, citadel_parms *parms);
int cmd_seta(int sd, char *newaide);
int cmd_ent0(int sd, int postflag, char *recipient, int anonymous, int format, char *postname, citadel_parms *parms, char *local_filename);
int cmd_rinf(int sd, citadel_list **list);

int cmd_mesg(int sd, char *msgname, citadel_list **list);
int cmd_rwho(int sd, citadel_list **list);

/* Cit 4.01 */
int cmd_iden(int sd, int, int, int, char *, char *);
int cmd_sexp(int sd, char *username, char *msg);
int cmd_pexp(int sd, citadel_list **list);

/* Cit 5.02 */

int cmd_hchg(int sd, char *hostname);
int cmd_rchg(int sd, char *roomname);
int cmd_uchg(int sd, char *username);


/* Unimplemented */

int cmd_newu(int sd, char *username);
int cmd_dele(int sd, int msgno);
int cmd_move(int sd, int msgno, char *target_room);
int cmd_kill(int sd, int act_delete, char *nextroom);
int cmd_cre8(int sd, int flag, char *newname, int access, char *password, int floorno);
int cmd_forg(int sd, char *nextroom);
int cmd_gnur(int sd, citadel_list **list);
int cmd_greg(int sd, citadel_list **list);
int cmd_vali(int sd, char *username, int access_level);
int cmd_einf(int sd, citadel_list **list);
int cmd_list(int sd, citadel_list **list);
int cmd_regi(int sd, citadel_list *list);
int cmd_chek(int sd, citadel_parms *parms);
int cmd_delf(int sd, char *filename);
int cmd_movf(int sd, char *filename, char *target_room);
int cmd_netf(int sd, char *filename, char *node_name);
int cmd_open(int sd, char *filename, citadel_parms *parms);
int cmd_clos(int sd);
int cmd_read(int sd, char *buf, int start_pos,int num_bytes);
int cmd_uopn(int sd, char *filename, citadel_parms *parms);
int cmd_ucls(int sd, int save);
int cmd_writ(int sd, char *buf, int nbytes);
int cmd_quser(int sd, char *username, citadel_parms *parms);
int cmd_oimg(int sd, char *filename, char *parm);

/* Cit/UX 4.01 cmds */

int cmd_netp(int sd, char *nodename, char *password);
int cmd_nuop(int sd);
int cmd_ndop(int sd, citadel_parms *parms);
int cmd_lflr(int sd, citadel_list **list);
int cmd_cflr(int sd, char *floorname, int makefloor);
int cmd_kflr(int sd, int floornum, int killfloor);
int cmd_eflr(int sd, int floornum, char *newname);
int cmd_iden(int sd, int, int, int, char *, char *);
int cmd_ipgm(int sd, char *password);
int cmd_chat(int sd);

/* Cit/UX 4.10 */

int cmd_ebio(int sd, citadel_list *list);
int cmd_rbio(int sd, char *username, citadel_list **list);

/* Cit/UX 4.11 */

int cmd_stel(int sd, int enter);
int cmd_lbio(int sd, citadel_list *list);
int cmd_msg2(int sd, citadel_list *list);

/* Cit/UX 5.00 */

int cmd_term(int sd, int taskno);
int cmd_down(int sd);
int cmd_scdn(int sd, int setflag);
int cmd_emsg(int sd, char *filename, char *local_filename);
int cmd_uimg(int sd, int upload, char *filename, char *local_filename);

/* Cit/UX 5.02 */

int cmd_time(int sd, long *time);
int cmd_agup(int sd, citadel_parms *parms);
int cmd_asup(int sd, citadel_parms *parms);

/* Not implemented */
/* int cmd_msg3() */
/* int cmd_ent3() */
/* int cmd_nset(int sd,  */
