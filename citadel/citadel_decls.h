/* $Id$ */
extern char fullname[32];
extern unsigned room_flags;
extern char room_name[ROOMNAMELEN];
extern struct CtdlServInfo serv_info;
extern char axlevel;
extern char is_room_aide;
extern unsigned userflags;
extern char sigcaught;
extern char editor_path[SIZ];
extern char printcmd[SIZ];
extern char have_xterm;
extern char rc_username[32];
extern char rc_password[32];
extern char rc_floor_mode;
extern char rc_encrypt;			/* from the citadel.rc file */
extern char arg_encrypt;		/* from the command line */
extern char rc_screen;
extern char arg_screen;
extern char rc_alt_semantics;
extern char express_msgs;
void logoff(int code);
void formout(char *name);
void sighandler(int which_sig);
void do_system_configuration(void);
