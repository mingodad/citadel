/*
 * Copyright (c) 1987-2009 by the citadel.org team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "serv_extensions.h"
#include "context.h"

/* Simple linked list structures ... used in a bunch of different places. */
struct RoomProcList {
        struct RoomProcList *next;
        char name[ROOMNAMELEN];
};
struct UserProcList {
	struct UserProcList *next;
	char user[64];
};

#define CTDLUSERIP      (IsEmptyStr(CC->cs_addr) ?  CC->cs_clientinfo: CC->cs_addr)

void cit_backtrace(void);
void cit_panic_backtrace(int SigNum);
void master_startup (void);
void master_cleanup (int exitcode);
void set_wtmpsupp (char *newtext);
void set_wtmpsupp_to_current_room(void);
void do_command_loop(void);
void do_async_loop(void);
void begin_session(struct CitContext *con);
void citproto_begin_session(void);
void GenerateRoomDisplay(char *real_room,
                        CitContext *viewed,
                        CitContext *viewer);
extern int panic_fd;
char CtdlCheckExpress(void);
extern time_t server_startup_time;
