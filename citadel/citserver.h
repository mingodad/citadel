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

