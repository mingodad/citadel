/* $Id$ */

#include "dynloader.h"

void master_startup (void);
void master_cleanup (void);
void RemoveContext (struct CitContext *);
void set_wtmpsupp (char *newtext);
void set_wtmpsupp_to_current_room(void);
void cmd_info (void);
void cmd_time (void);
int is_public_client (char *where);
void cmd_iden (char *argbuf);
void cmd_mesg (char *mname);
void cmd_emsg (char *mname);
void cmd_term (char *cmdbuf);
void cmd_more (void);
void cmd_echo (char *etext);
void cmd_ipgm (char *argbuf);
void cmd_down (void);
void cmd_scdn (char *argbuf);
void cmd_extn (char *argbuf);
void deallocate_user_data(struct CitContext *con);
void *CtdlGetUserData(unsigned long requested_sym);
void CtdlAllocUserData(unsigned long requested_sym, size_t num_bytes);
void CtdlReallocUserData(unsigned long requested_sym, size_t num_bytes);
int CtdlGetDynamicSymbol(void);
void do_command_loop(void);
void begin_session(struct CitContext *con);
void citproto_begin_session(void);
void GenerateRoomDisplay(char *real_room,
                        struct CitContext *viewed,
                        struct CitContext *viewer);
extern DLEXP int do_defrag;
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




