/*
 * userpurge.c
 *
 * This program is a server extension which purges the user file of any user
 * who has not logged in for a period of time, or who has elected to delete
 * their account by setting their password to "deleteme".
 */

/* PURGE_TIME is the amount of time (in seconds) for which a user must not
 * have logged in for his/her account to be purged.
 */
#define PURGE_TIME (5184000L) /* two months */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include "citadel.h"

void do_user_purge(struct usersupp *us) {
	int purge;
	time_t now;

	/* The default rule is to not purge. */
	purge = 0;

	/* If the user hasn't called in two months, his/her account
	 * has expired, so purge the record.
	 */
	now = time(NULL);
	if ((now - us->lastcall) > PURGE_TIME) purge = 1;

	/* If the user set his/her password to 'deleteme', he/she
	 * wishes to be deleted, so purge the record.
	 */
	if (!strcasecmp(us->password, "deleteme")) purge = 1;

	/* If the record is marked as permanent, don't purge it.
	 */
	if (us->flags & US_PERM) purge = 0;

	/* If the access level is 0, the record should already have been
	 * deleted, but maybe the user was logged in at the time or something.
	 * Delete the record now.
	 */
	if (us->axlevel == 0) purge = 1;

	/* 0 calls is impossible.  If there are 0 calls, it must
	 * be a corrupted record, so purge it.
	 */
	if (us->timescalled == 0) purge = 1;

	if (purge == 1) {
		/* FIX add the delete call here. */
		}


	}


void MyReallyCoolModuleEntryPoint() {
	ForEachUser(do_user_purge);
	}
