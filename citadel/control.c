/*
 * control.c
 *
 * This module handles states which are global to the entire server.
 *
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <syslog.h>
#include "citadel.h"
#include "server.h"
#include "control.h"
#include "sysdep_decls.h"

struct CitControl CitControl;

/*
 * get_control  -  read the control record into memory.
 */
void get_control(void) {
	FILE *fp;

	/* Zero it out.  If the control record on disk is missing or short,
	 * the system functions with all control record fields initialized
	 * to zero.
	 */
	memset(&CitControl, 0, sizeof(struct CitControl));
	fp = fopen("citadel.control", "rb");
	if (fp == NULL) return;

	fread(&CitControl, sizeof(struct CitControl), 1, fp);
	fclose(fp);
	}

/*
 * put_control  -  write the control record to disk.
 */
void put_control(void) {
	FILE *fp;

	fp = fopen("citadel.control", "wb");
	if (fp != NULL) {
		fwrite(&CitControl, sizeof(struct CitControl), 1, fp);
		fclose(fp);
		}
	}


/*
 * get_new_message_number()  -  Obtain a new, unique ID to be used for a message.
 */
long get_new_message_number(void) {
	begin_critical_section(S_CONTROL);
	get_control();
	++CitControl.MMhighest;
	put_control();
	end_critical_section(S_CONTROL);
	return(CitControl.MMhighest);
	}


/*
 * get_new_user_number()  -  Obtain a new, unique ID to be used for a user.
 */
long get_new_user_number(void) {
	begin_critical_section(S_CONTROL);
	get_control();
	++CitControl.MMnextuser;
	put_control();
	end_critical_section(S_CONTROL);
	return(CitControl.MMnextuser);
	}
