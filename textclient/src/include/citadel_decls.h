extern char fullname[USERNAME_SIZE];
extern unsigned room_flags;
extern char room_name[ROOMNAMELEN];
extern struct CtdlServInfo serv_info;
extern char axlevel;
extern char is_room_aide;
extern unsigned userflags;
extern char sigcaught;
extern char editor_paths[MAX_EDITORS][SIZ];
extern char printcmd[SIZ];
extern char imagecmd[SIZ];
extern char have_xterm;
extern char rc_username[USERNAME_SIZE];
extern char rc_password[32];
extern char rc_floor_mode;
extern time_t rc_idle_threshold;
#ifdef HAVE_OPENSSL
extern char rc_encrypt;			/* from the citadel.rc file */
extern char arg_encrypt;		/* from the command line */
#endif
#if defined(HAVE_CURSES_H) && !defined(DISABLE_CURSES)
extern char rc_screen;
extern char arg_screen;
#endif
extern char rc_alt_semantics;
extern char instant_msgs;
void ctdl_logoff(char *file, int line, CtdlIPC *ipc, int code);
#define logoff(ipc, code)	ctdl_logoff(__FILE__, __LINE__, (ipc), (code))
void formout(CtdlIPC *ipc, char *name);
void sighandler(int which_sig);
extern int secure;
void remove_march(char *roomname, int floornum);
