/* $Id$ */
#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif
#include <time.h>
#include <limits.h>
#include "citadel.h"
#include "server.h"
#include "database.h"
#include "config.h"
#include "room_ops.h"
#include "sysdep_decls.h"
#include "support.h"
#include "user_ops.h"
#include "msgbase.h"
#include "serv_chat.h"
#include "citserver.h"
#include "tools.h"


/*
 * Retrieve the applicable expire policy for a specific room
 */
void GetExpirePolicy(struct ExpirePolicy *epbuf, struct quickroom *qrbuf) {
	struct floor flbuf;

	/* If the room has its own policy, return it */	
	if (qrbuf->QRep.expire_mode != 0) {
		memcpy(epbuf, &qrbuf->QRep, sizeof(struct ExpirePolicy));
		return;
		}

	/* Otherwise, if the floor has its own policy, return it */	
	getfloor(&flbuf, qrbuf->QRfloor);
	if (flbuf.f_ep.expire_mode != 0) {
		memcpy(epbuf, &flbuf.f_ep, sizeof(struct ExpirePolicy));
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
	struct floor flbuf;
	char which[256];

	extract(which, argbuf, 0);
	if (!strcasecmp(which, "room")) {
		memcpy(&exp, &CC->quickroom.QRep, sizeof(struct ExpirePolicy));
		}
	else if (!strcasecmp(which, "floor")) {
		getfloor(&flbuf, CC->quickroom.QRfloor);
		memcpy(&exp, &flbuf.f_ep, sizeof(struct ExpirePolicy));
		}
	else if (!strcasecmp(which, "site")) {
		memcpy(&exp, &config.c_ep, sizeof(struct ExpirePolicy));
		}
	else {
		cprintf("%d Invalid keyword.\n", ERROR);
		return;
		}

	cprintf("%d %d|%d\n", OK, exp.expire_mode, exp.expire_value);
	}


/*
 * Set Policy EXpire
 */
void cmd_spex(char *argbuf) {
	struct ExpirePolicy exp;
	struct floor flbuf;
	char which[256];

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
		lgetroom(&CC->quickroom, CC->quickroom.QRname);
		memcpy(&CC->quickroom.QRep, &exp, sizeof(struct ExpirePolicy));
		lputroom(&CC->quickroom, CC->quickroom.QRname);
		cprintf("%d ok\n", OK);
		return;
		}

	if (CC->usersupp.axlevel < 6) {
		cprintf("%d Higher access required.\n",
			ERROR+HIGHER_ACCESS_REQUIRED);
		return;
		}

	if (!strcasecmp(which, "floor")) {
		lgetfloor(&flbuf, CC->quickroom.QRfloor);
		memcpy(&flbuf.f_ep, &exp, sizeof(struct ExpirePolicy));
		lputfloor(&flbuf, CC->quickroom.QRfloor);
		cprintf("%d ok\n", OK);
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
		cprintf("%d ok\n", OK);
		return;
		}

	else {
		cprintf("%d Invalid keyword.\n", ERROR);
		return;
		}

	}


