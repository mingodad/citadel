/*
 * $Id$
 *
 * This module handles states which are global to the entire server.
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
#include <syslog.h>
#include <sys/types.h>
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
FILE *control_fp = NULL;

/*
 * get_control  -  read the control record into memory.
 */
void get_control(void) {

	/* Zero it out.  If the control record on disk is missing or short,
	 * the system functions with all control record fields initialized
	 * to zero.
	 */
	memset(&CitControl, 0, sizeof(struct CitControl));
	if (control_fp == NULL) {
		control_fp = fopen("citadel.control", "rb+");
		if (control_fp != NULL) {
			fchown(fileno(control_fp), config.c_bbsuid, -1);
		}
	}
	if (control_fp == NULL) {
		control_fp = fopen("citadel.control", "wb+");
		if (control_fp != NULL) {
			fchown(fileno(control_fp), config.c_bbsuid, -1);
			memset(&CitControl, 0, sizeof(struct CitControl));
			fwrite(&CitControl, sizeof(struct CitControl),
				1, control_fp);
			rewind(control_fp);
		}
	}
	if (control_fp == NULL) {
		lprintf(1, "ERROR opening citadel.control: %s\n",
			strerror(errno));
		return;
	}

	rewind(control_fp);
	fread(&CitControl, sizeof(struct CitControl), 1, control_fp);
	}

/*
 * put_control  -  write the control record to disk.
 */
void put_control(void) {

	if (control_fp != NULL) {
		rewind(control_fp);
		fwrite(&CitControl, sizeof(struct CitControl), 1, control_fp);
		fflush(control_fp);
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
	char cmd[SIZ];
	char *ibuf;
	int a;
	char *confptr;
	char confname[SIZ];

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
		cprintf("%d\n", config.c_createax);
		cprintf("%d\n", config.c_maxmsglen);
		cprintf("%d\n", config.c_min_workers);
		cprintf("%d\n", config.c_max_workers);
		cprintf("%d\n", config.c_pop3_port);
		cprintf("%d\n", config.c_smtp_port);
		cprintf("%d\n", config.c_default_filter);
		cprintf("%d\n", config.c_aide_zap);
		cprintf("%d\n", config.c_imap_port);
		cprintf("000\n");
		}

	else if (!strcasecmp(cmd, "SET")) {
	    char buf[SIZ];
		cprintf("%d Send configuration...\n", SEND_LISTING);
		a = 0;
		while (client_gets(&ibuf), strcmp(ibuf, "000")) {
		    switch(a) {
			case 0:	safestrncpy(config.c_nodename, ibuf,
					sizeof config.c_nodename);
				break;
			case 1:	safestrncpy(config.c_fqdn, ibuf,
					sizeof config.c_fqdn);
				break;
			case 2:	safestrncpy(config.c_humannode, ibuf,
					sizeof config.c_humannode);
				break;
			case 3:	safestrncpy(config.c_phonenum, ibuf,
					sizeof config.c_phonenum);
				break;
			case 4:	config.c_creataide = atoi(ibuf);
				break;
			case 5:	config.c_sleeping = atoi(ibuf);
				break;
			case 6:	config.c_initax = atoi(ibuf);
				if (config.c_initax < 1) config.c_initax = 1;
				if (config.c_initax > 6) config.c_initax = 6;
				break;
			case 7:	config.c_regiscall = atoi(ibuf);
				if (config.c_regiscall != 0)
					config.c_regiscall = 1;
				break;
			case 8:	config.c_twitdetect = atoi(ibuf);
				if (config.c_twitdetect != 0)
					config.c_twitdetect = 1;
				break;
			case 9:	safestrncpy(config.c_twitroom, ibuf,
					sizeof config.c_twitroom);
				break;
			case 10: safestrncpy(config.c_moreprompt, ibuf,
					sizeof config.c_moreprompt);
				break;
			case 11: config.c_restrict = atoi(ibuf);
				if (config.c_restrict != 0)
					config.c_restrict = 1;
				break;
			case 12: safestrncpy(config.c_bbs_city, ibuf,
					sizeof config.c_bbs_city);
				break;
			case 13: safestrncpy(config.c_sysadm, ibuf,
					sizeof config.c_sysadm);
				break;
			case 14: config.c_maxsessions = atoi(ibuf);
				if (config.c_maxsessions < 1)
					config.c_maxsessions = 1;
				break;
			case 15: safestrncpy(config.c_net_password, ibuf,
					sizeof config.c_net_password);
				break;
			case 16: config.c_userpurge = atoi(ibuf);
				break;
			case 17: config.c_roompurge = atoi(ibuf);
				break;
			case 18: safestrncpy(config.c_logpages, ibuf,
					sizeof config.c_logpages);
				break;
			case 19: config.c_createax = atoi(ibuf);
				if (config.c_createax < 1)
					config.c_createax = 1;
				if (config.c_createax > 6)
					config.c_createax = 6;
				break;
			case 20: if (atoi(ibuf) >= 8192)
					config.c_maxmsglen = atoi(ibuf);
				break;
			case 21: if (atoi(ibuf) >= 2)
					config.c_min_workers = atoi(ibuf);
			case 22: if (atoi(ibuf) >= config.c_min_workers)
					config.c_max_workers = atoi(ibuf);
			case 23: config.c_pop3_port = atoi(ibuf);
				break;
			case 24: config.c_smtp_port = atoi(ibuf);
				break;
			case 25: config.c_default_filter = atoi(ibuf);
				break;
			case 26: config.c_aide_zap = atoi(ibuf);
				if (config.c_aide_zap != 0)
					config.c_aide_zap = 1;
				break;
			case 27: config.c_imap_port = atoi(ibuf);
				break;
			}
		    ++a;
		    }
		put_config();
		snprintf(buf,sizeof buf,
			 "Global system configuration edited by %s\n",
			 CC->curr_user);
		aide_message(buf);

		if (strlen(config.c_logpages) > 0)
			create_room(config.c_logpages, 3, "", 0, 1);
		}

	else if (!strcasecmp(cmd, "GETSYS")) {
		extract(confname, argbuf, 1);
		confptr = CtdlGetSysConfig(confname);
		if (confptr != NULL) {
			cprintf("%d %s\n", LISTING_FOLLOWS, confname);
			client_write(confptr, strlen(confptr));
			if (confptr[strlen(confptr)-1] != 10)
				client_write("\n", 1);
			cprintf("000\n");
			phree(confptr);
		}
		else {
			cprintf("%d No such configuration.\n",
				ERROR+ILLEGAL_VALUE);
		}
	}

	else if (!strcasecmp(cmd, "PUTSYS")) {
		extract(confname, argbuf, 1);
		cprintf("%d %s\n", SEND_LISTING, confname);
		confptr = CtdlReadMessageBody("000", config.c_maxmsglen, NULL);
		CtdlPutSysConfig(confname, confptr);
		phree(confptr);
	}

	else {
		cprintf("%d Illegal option(s) specified.\n",
			ERROR+ILLEGAL_VALUE);
		}
	}
