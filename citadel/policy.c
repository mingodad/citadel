/* 
 * $Id$
 *
 * Functions which manage policy for rooms (such as message expiry)
 */

#ifdef DLL_EXPORT
#define IN_LIBCIT
#endif

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <limits.h>
#include "citadel.h"
#include "server.h"
#include "database.h"
#include "serv_extensions.h"
#include "config.h"
#include "room_ops.h"
#include "sysdep_decls.h"
#include "support.h"
#include "user_ops.h"
#include "msgbase.h"
#include "citserver.h"
#include "tools.h"


/*
 * Retrieve the applicable expire policy for a specific room
 */
void GetExpirePolicy(struct ExpirePolicy *epbuf, struct ctdlroom *qrbuf) {
	struct floor *fl;

	/* If the room has its own policy, return it */	
	if (qrbuf->QRep.expire_mode != 0) {
		memcpy(epbuf, &qrbuf->QRep, sizeof(struct ExpirePolicy));
		return;
	}

	/* Otherwise, if the floor has its own policy, return it */
	fl = cgetfloor(qrbuf->QRfloor);
	if (fl->f_ep.expire_mode != 0) {
		memcpy(epbuf, &fl->f_ep, sizeof(struct ExpirePolicy));
		return;
	}

	/* Otherwise, fall back on the system default */
	memcpy(epbuf, &config.c_ep, sizeof(struct ExpirePolicy));
}


/*
 * Get Policy EXpire
 */
void cmd_gpex(char *argbuf) {
	struct ExpirePolicy exp;
	struct floor *fl;
	char which[SIZ];

	extract(which, argbuf, 0);
	if (!strcasecmp(which, "room")) {
		memcpy(&exp, &CC->room.QRep, sizeof(struct ExpirePolicy));
	}
	else if (!strcasecmp(which, "floor")) {
		fl = cgetfloor(CC->room.QRfloor);
		memcpy(&exp, &fl->f_ep, sizeof(struct ExpirePolicy));
	}
	else if (!strcasecmp(which, "site")) {
		memcpy(&exp, &config.c_ep, sizeof(struct ExpirePolicy));
	}
	else {
		cprintf("%d Invalid keyword \"%s\"\n", ERROR, which);
		return;
	}

	cprintf("%d %d|%d\n", CIT_OK, exp.expire_mode, exp.expire_value);
}


/*
 * Set Policy EXpire
 */
void cmd_spex(char *argbuf) {
	struct ExpirePolicy exp;
	struct floor flbuf;
	char which[SIZ];

	memset(&exp, 0, sizeof(struct ExpirePolicy));
	extract(which, argbuf, 0);
	exp.expire_mode = extract_int(argbuf, 1);
	exp.expire_value = extract_int(argbuf, 2);

	if ((exp.expire_mode < 0) || (exp.expire_mode > 3)) {
		cprintf("%d Invalid policy.\n", ERROR);
		return;
	}

	if (!strcasecmp(which, "room")) {
		if (!is_room_aide()) {
			cprintf("%d Higher access required.\n",
				ERROR+HIGHER_ACCESS_REQUIRED);
			return;
		}
		lgetroom(&CC->room, CC->room.QRname);
		memcpy(&CC->room.QRep, &exp, sizeof(struct ExpirePolicy));
		lputroom(&CC->room);
		cprintf("%d Room expire policy set.\n", CIT_OK);
		return;
	}

	if (CC->user.axlevel < 6) {
		cprintf("%d Higher access required.\n",
			ERROR+HIGHER_ACCESS_REQUIRED);
		return;
	}

	if (!strcasecmp(which, "floor")) {
		lgetfloor(&flbuf, CC->room.QRfloor);
		memcpy(&flbuf.f_ep, &exp, sizeof(struct ExpirePolicy));
		lputfloor(&flbuf, CC->room.QRfloor);
		cprintf("%d Floor expire policy set.\n", CIT_OK);
		return;
	}

	else if (!strcasecmp(which, "site")) {
		if (exp.expire_mode == EXPIRE_NEXTLEVEL) {
			cprintf("%d Invalid policy (no higher level)\n",
				ERROR);
			return;
		}
		memcpy(&config.c_ep, &exp, sizeof(struct ExpirePolicy));
		put_config();
		cprintf("%d Site expire policy set.\n", CIT_OK);
		return;
	}

	else {
		cprintf("%d Invalid keyword \"%s\"\n", ERROR, which);
		return;
	}

}


