#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
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


