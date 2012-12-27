/*
 * Copyright (c) 2000-2012 by the citadel.org team
 *
 *  This program is open source software; you can redistribute it and/or modify
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

extern int NetQDebugEnabled;

#define DBGLOG(LEVEL) if ((LEVEL != LOG_DEBUG) || (NetQDebugEnabled != 0))

#define QN_syslog(LEVEL, FORMAT, ...)				\
	DBGLOG(LEVEL) syslog(LEVEL,				\
			     "CC[%d]" FORMAT, \
			     CCC->cs_pid, __VA_ARGS__)

#define QNM_syslog(LEVEL, FORMAT)				\
	DBGLOG(LEVEL) syslog(LEVEL,				\
			     "CC[%d]" FORMAT, \
			     CCC->cs_pid)

typedef struct namelist namelist;

struct namelist {
	namelist *next;
	StrBuf *Value;
};


void free_netfilter_list(void);
void load_network_filter_list(void);



void network_queue_room(struct ctdlroom *, void *);
////void destroy_network_queue_room(void);
void network_bounce(struct CtdlMessage *msg, char *reason);
int network_usetable(struct CtdlMessage *msg);

int network_talking_to(const char *nodename, long len, int operation);

/*
 * Operations that can be performed by network_talking_to()
 */
enum {
        NTT_ADD,
        NTT_REMOVE,
        NTT_CHECK
};
