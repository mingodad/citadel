/* $Id$ */

#include "serv_extensions.h"

/* Simple linked list structures ... used in a bunch of different places. */
struct RoomProcList {
        struct RoomProcList *next;
        char name[ROOMNAMELEN];
};
struct UserProcList {
	struct UserProcList *next;
	char user[64];
};

void cit_backtrace(void);
void cit_panic_backtrace(int SigNum);
void master_startup (void);
void master_cleanup (int exitcode);
void RemoveContext (struct CitContext *);
void set_wtmpsupp (char *newtext);
void set_wtmpsupp_to_current_room(void);
void cmd_info (void);
void cmd_time (void);
void cmd_iden (char *argbuf);
void cmd_mesg (char *mname);
void cmd_emsg (char *mname);
void cmd_term (char *cmdbuf);
void cmd_more (void);
void cmd_echo (char *etext);
void cmd_ipgm (char *argbuf);
void cmd_down (char *argbuf);
void cmd_halt (void);
void cmd_scdn (char *argbuf);
void cmd_extn (char *argbuf);
void do_command_loop(void);
void do_async_loop(void);
void begin_session(struct CitContext *con);
void citproto_begin_session(void);
void GenerateRoomDisplay(char *real_room,
                        struct CitContext *viewed,
                        struct CitContext *viewer);
extern int panic_fd;
char CtdlCheckExpress(void);

int CtdlAccessCheck(int);

/* 'required access level' values which may be passed to CtdlAccessCheck()
 */
enum {
	ac_none,
	ac_logged_in,
	ac_room_aide,
	ac_aide,
	ac_internal
};



extern time_t server_startup_time;
