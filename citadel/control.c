/*
 * control.c
 *
 * This module handles states which are global to the entire server.
 *
 * $Id$
 *
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif
#include <syslog.h>
#include "citadel.h"
#include "server.h"
#include "control.h"
#include "sysdep_decls.h"
#include "support.h"
#include "config.h"
#include "msgbase.h"
#include "tools.h"
#include "room_ops.h"

struct CitControl CitControl;
struct config config;

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



/*
 * get_new_room_number()  -  Obtain a new, unique ID to be used for a room.
 */
long get_new_room_number(void) {
	begin_critical_section(S_CONTROL);
	get_control();
	++CitControl.MMnextroom;
	put_control();
	end_critical_section(S_CONTROL);
	return(CitControl.MMnextroom);
	}



/* 
 * Get or set global configuration options
 */
void cmd_conf(char *argbuf) {
	char cmd[256];
	char buf[256];
	int a;

	if (!(CC->logged_in)) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
		}

	if (CC->usersupp.axlevel < 6) {
		cprintf("%d Higher access required.\n",
			ERROR+HIGHER_ACCESS_REQUIRED);
		return;
		}

	extract(cmd, argbuf, 0);
	if (!strcasecmp(cmd, "GET")) {
		cprintf("%d Configuration...\n", LISTING_FOLLOWS);
		cprintf("%s\n", config.c_nodename);
		cprintf("%s\n", config.c_fqdn);
		cprintf("%s\n", config.c_humannode);
		cprintf("%s\n", config.c_phonenum);
		cprintf("%d\n", config.c_creataide);
		cprintf("%d\n", config.c_sleeping);
		cprintf("%d\n", config.c_initax);
		cprintf("%d\n", config.c_regiscall);
		cprintf("%d\n", config.c_twitdetect);
		cprintf("%s\n", config.c_twitroom);
		cprintf("%s\n", config.c_moreprompt);
		cprintf("%d\n", config.c_restrict);
		cprintf("%s\n", config.c_bbs_city);
		cprintf("%s\n", config.c_sysadm);
		cprintf("%d\n", config.c_maxsessions);
		cprintf("%s\n", config.c_net_password);
		cprintf("%d\n", config.c_userpurge);
		cprintf("%d\n", config.c_roompurge);
		cprintf("%s\n", config.c_logpages);
		cprintf("000\n");
		}

	else if (!strcasecmp(cmd, "SET")) {
		cprintf("%d Send configuration...\n", SEND_LISTING);
		a = 0;
		while (client_gets(buf), strcmp(buf, "000")) {
		    switch(a) {
			case 0:	strncpy(config.c_nodename, buf, 16);
				break;
			case 1:	strncpy(config.c_fqdn, buf, 64);
				break;
			case 2:	strncpy(config.c_humannode, buf, 21);
				break;
			case 3:	strncpy(config.c_phonenum, buf, 16);
				break;
			case 4:	config.c_creataide = atoi(buf);
				break;
			case 5:	config.c_sleeping = atoi(buf);
				break;
			case 6:	config.c_initax = atoi(buf);
				if (config.c_initax < 1) config.c_initax = 1;
				if (config.c_initax > 6) config.c_initax = 6;
				break;
			case 7:	config.c_regiscall = atoi(buf);
				if (config.c_regiscall != 0)
					config.c_regiscall = 1;
				break;
			case 8:	config.c_twitdetect = atoi(buf);
				if (config.c_twitdetect != 0)
					config.c_twitdetect = 1;
				break;
			case 9:	strncpy(config.c_twitroom,
					buf, ROOMNAMELEN);
				break;
			case 10: strncpy(config.c_moreprompt, buf, 80);
				break;
			case 11: config.c_restrict = atoi(buf);
				if (config.c_restrict != 0)
					config.c_restrict = 1;
				break;
			case 12: strncpy(config.c_bbs_city, buf, 32);
				break;
			case 13: strncpy(config.c_sysadm, buf, 26);
				break;
			case 14: config.c_maxsessions = atoi(buf);
				if (config.c_maxsessions < 1)
					config.c_maxsessions = 1;
				break;
			case 15: strncpy(config.c_net_password, buf, 20);
				break;
			case 16: config.c_userpurge = atoi(buf);
				break;
			case 17: config.c_roompurge = atoi(buf);
				break;
			case 18: strncpy(config.c_logpages,
					buf, ROOMNAMELEN);
				break;
				}
		    ++a;
		    }
		put_config();
		snprintf(buf,sizeof buf,
			 "Global system configuration edited by %s",
			 CC->curr_user);
		aide_message(buf);

		if (strlen(config.c_logpages) > 0)
			create_room(config.c_logpages, 4, "", 0);
		}

	else {
		cprintf("%d The only valid options are GET and SET.\n",
			ERROR+ILLEGAL_VALUE);
		}
	}
