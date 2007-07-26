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

#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/file.h>
#include "citadel.h"
#include "server.h"
#include "control.h"
#include "sysdep_decls.h"
#include "support.h"
#include "config.h"
#include "msgbase.h"
#include "citserver.h"
#include "tools.h"
#include "room_ops.h"

#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif

struct CitControl CitControl;
extern struct config config;
FILE *control_fp = NULL;



/*
 * lock_control  -  acquire a lock on the control record file.
 *                  This keeps multiple citservers from running concurrently.
 */
void lock_control(void)
{
#ifndef BSD_GETPWUID
/*
 * TODO: solaris manpages describe this function, but the headers
 * don't show it! 
 */
	if (flock(fileno(control_fp), (LOCK_EX | LOCK_NB))) {
		lprintf(CTDL_EMERG, "citserver: unable to lock %s.\n", file_citadel_control);
		lprintf(CTDL_EMERG, "Is another citserver already running?\n");
		exit(CTDLEXIT_CONTROL);
	}
#endif
}


/*
 * get_control  -  read the control record into memory.
 */
void get_control(void)
{
	static int already_have_control = 0;

	/*
	 * If we already have the control record in memory, there's no point
	 * in reading it from disk again.
	 */
	if (already_have_control) return;

	/* Zero it out.  If the control record on disk is missing or short,
	 * the system functions with all control record fields initialized
	 * to zero.
	 */
	memset(&CitControl, 0, sizeof(struct CitControl));
	if (control_fp == NULL) {
		control_fp = fopen(file_citadel_control, "rb+");
		if (control_fp != NULL) {
			lock_control();
			fchown(fileno(control_fp), config.c_ctdluid, -1);
		}
	}
	if (control_fp == NULL) {
		control_fp = fopen(file_citadel_control, "wb+");
		if (control_fp != NULL) {
			lock_control();
			fchown(fileno(control_fp), config.c_ctdluid, -1);
			memset(&CitControl, 0, sizeof(struct CitControl));
			fwrite(&CitControl, sizeof(struct CitControl),
			       1, control_fp);
			rewind(control_fp);
		}
	}
	if (control_fp == NULL) {
		lprintf(CTDL_ALERT, "ERROR opening %s: %s\n",
				file_citadel_control,
				strerror(errno));
		return;
	}

	rewind(control_fp);
	fread(&CitControl, sizeof(struct CitControl), 1, control_fp);
	already_have_control = 1;
}

/*
 * put_control  -  write the control record to disk.
 */
void put_control(void)
{

	if (control_fp != NULL) {
		rewind(control_fp);
		fwrite(&CitControl, sizeof(struct CitControl), 1,
		       control_fp);
		fflush(control_fp);
	}
}

/**
 * release_control - close our fd on exit
 */
void release_control(void)
{
	if (control_fp != NULL)
		fclose(control_fp);
	control_fp = NULL;
}

/*
 * get_new_message_number()  -  Obtain a new, unique ID to be used for a message.
 */
long get_new_message_number(void)
{
	long retval = 0L;
	begin_critical_section(S_CONTROL);
	get_control();
	retval = ++CitControl.MMhighest;
	put_control();
	end_critical_section(S_CONTROL);
	return(retval);
}


/*
 * get_new_user_number()  -  Obtain a new, unique ID to be used for a user.
 */
long get_new_user_number(void)
{
	long retval = 0L;
	begin_critical_section(S_CONTROL);
	get_control();
	retval = ++CitControl.MMnextuser;
	put_control();
	end_critical_section(S_CONTROL);
	return(retval);
}



/*
 * get_new_room_number()  -  Obtain a new, unique ID to be used for a room.
 */
long get_new_room_number(void)
{
	long retval = 0L;
	begin_critical_section(S_CONTROL);
	get_control();
	retval = ++CitControl.MMnextroom;
	put_control();
	end_critical_section(S_CONTROL);
	return(retval);
}



/* 
 * Get or set global configuration options
 */
void cmd_conf(char *argbuf)
{
	char cmd[16];
	char buf[256];
	int a;
	char *confptr;
	char confname[128];

	if (CtdlAccessCheck(ac_aide)) return;

	extract_token(cmd, argbuf, 0, '|', sizeof cmd);
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
		cprintf("%s\n", config.c_site_location);
		cprintf("%s\n", config.c_sysadm);
		cprintf("%d\n", config.c_maxsessions);
		cprintf("xxx\n"); /* placeholder -- field no longer in use */
		cprintf("%d\n", config.c_userpurge);
		cprintf("%d\n", config.c_roompurge);
		cprintf("%s\n", config.c_logpages);
		cprintf("%d\n", config.c_createax);
		cprintf("%ld\n", config.c_maxmsglen);
		cprintf("%d\n", config.c_min_workers);
		cprintf("%d\n", config.c_max_workers);
		cprintf("%d\n", config.c_pop3_port);
		cprintf("%d\n", config.c_smtp_port);
		cprintf("%d\n", config.c_rfc822_strict_from);
		cprintf("%d\n", config.c_aide_zap);
		cprintf("%d\n", config.c_imap_port);
		cprintf("%ld\n", config.c_net_freq);
		cprintf("%d\n", config.c_disable_newu);
		cprintf("1\n");	/* niu */
		cprintf("%d\n", config.c_purge_hour);
#ifdef HAVE_LDAP
		cprintf("%s\n", config.c_ldap_host);
		cprintf("%d\n", config.c_ldap_port);
		cprintf("%s\n", config.c_ldap_base_dn);
		cprintf("%s\n", config.c_ldap_bind_dn);
		cprintf("%s\n", config.c_ldap_bind_pw);
#else
		cprintf("\n");
		cprintf("0\n");
		cprintf("\n");
		cprintf("\n");
		cprintf("\n");
#endif
		cprintf("%s\n", config.c_ip_addr);
		cprintf("%d\n", config.c_msa_port);
		cprintf("%d\n", config.c_imaps_port);
		cprintf("%d\n", config.c_pop3s_port);
		cprintf("%d\n", config.c_smtps_port);
		cprintf("%d\n", config.c_enable_fulltext);
		cprintf("%d\n", config.c_auto_cull);
		cprintf("%d\n", config.c_instant_expunge);
		cprintf("%d\n", config.c_allow_spoofing);
		cprintf("%d\n", config.c_journal_email);
		cprintf("%d\n", config.c_journal_pubmsgs);
		cprintf("%s\n", config.c_journal_dest);
		cprintf("%s\n", config.c_default_cal_zone);
		cprintf("%d\n", config.c_pftcpdict_port);
		cprintf("%d\n", config.c_managesieve_port);
	        cprintf("%d\n", config.c_auth_mode);
	        cprintf("%s\n", config.c_funambol_host);
	        cprintf("%d\n", config.c_funambol_port);
	        cprintf("%s\n", config.c_funambol_source);
	        cprintf("%s\n", config.c_funambol_auth);
		cprintf("%d\n", config.c_rbl_at_greeting);
		cprintf("000\n");
	}

	else if (!strcasecmp(cmd, "SET")) {
		unbuffer_output();
		cprintf("%d Send configuration...\n", SEND_LISTING);
		a = 0;
		while (client_getln(buf, sizeof buf), strcmp(buf, "000")) {
			switch (a) {
			case 0:
				safestrncpy(config.c_nodename, buf,
					    sizeof config.c_nodename);
				break;
			case 1:
				safestrncpy(config.c_fqdn, buf,
					    sizeof config.c_fqdn);
				break;
			case 2:
				safestrncpy(config.c_humannode, buf,
					    sizeof config.c_humannode);
				break;
			case 3:
				safestrncpy(config.c_phonenum, buf,
					    sizeof config.c_phonenum);
				break;
			case 4:
				config.c_creataide = atoi(buf);
				break;
			case 5:
				config.c_sleeping = atoi(buf);
				break;
			case 6:
				config.c_initax = atoi(buf);
				if (config.c_initax < 1)
					config.c_initax = 1;
				if (config.c_initax > 6)
					config.c_initax = 6;
				break;
			case 7:
				config.c_regiscall = atoi(buf);
				if (config.c_regiscall != 0)
					config.c_regiscall = 1;
				break;
			case 8:
				config.c_twitdetect = atoi(buf);
				if (config.c_twitdetect != 0)
					config.c_twitdetect = 1;
				break;
			case 9:
				safestrncpy(config.c_twitroom, buf,
					    sizeof config.c_twitroom);
				break;
			case 10:
				safestrncpy(config.c_moreprompt, buf,
					    sizeof config.c_moreprompt);
				break;
			case 11:
				config.c_restrict = atoi(buf);
				if (config.c_restrict != 0)
					config.c_restrict = 1;
				break;
			case 12:
				safestrncpy(config.c_site_location, buf,
					    sizeof config.c_site_location);
				break;
			case 13:
				safestrncpy(config.c_sysadm, buf,
					    sizeof config.c_sysadm);
				break;
			case 14:
				config.c_maxsessions = atoi(buf);
				if (config.c_maxsessions < 0)
					config.c_maxsessions = 0;
				break;
			case 15:
				/* placeholder -- field no longer in use */
				break;
			case 16:
				config.c_userpurge = atoi(buf);
				break;
			case 17:
				config.c_roompurge = atoi(buf);
				break;
			case 18:
				safestrncpy(config.c_logpages, buf,
					    sizeof config.c_logpages);
				break;
			case 19:
				config.c_createax = atoi(buf);
				if (config.c_createax < 1)
					config.c_createax = 1;
				if (config.c_createax > 6)
					config.c_createax = 6;
				break;
			case 20:
				if (atoi(buf) >= 8192)
					config.c_maxmsglen = atoi(buf);
				break;
			case 21:
				if (atoi(buf) >= 2)
					config.c_min_workers = atoi(buf);
			case 22:
				if (atoi(buf) >= config.c_min_workers)
					config.c_max_workers = atoi(buf);
			case 23:
				config.c_pop3_port = atoi(buf);
				break;
			case 24:
				config.c_smtp_port = atoi(buf);
				break;
			case 25:
				config.c_rfc822_strict_from = atoi(buf);
				break;
			case 26:
				config.c_aide_zap = atoi(buf);
				if (config.c_aide_zap != 0)
					config.c_aide_zap = 1;
				break;
			case 27:
				config.c_imap_port = atoi(buf);
				break;
			case 28:
				config.c_net_freq = atol(buf);
				break;
			case 29:
				config.c_disable_newu = atoi(buf);
				if (config.c_disable_newu != 0)
					config.c_disable_newu = 1;
				break;
			case 30:
				/* niu */
				break;
			case 31:
				if ((config.c_purge_hour >= 0)
				   && (config.c_purge_hour <= 23)) {
					config.c_purge_hour = atoi(buf);
				}
				break;
#ifdef HAVE_LDAP
			case 32:
				safestrncpy(config.c_ldap_host, buf,
					    sizeof config.c_ldap_host);
				break;
			case 33:
				config.c_ldap_port = atoi(buf);
				break;
			case 34:
				safestrncpy(config.c_ldap_base_dn, buf,
					    sizeof config.c_ldap_base_dn);
				break;
			case 35:
				safestrncpy(config.c_ldap_bind_dn, buf,
					    sizeof config.c_ldap_bind_dn);
				break;
			case 36:
				safestrncpy(config.c_ldap_bind_pw, buf,
					    sizeof config.c_ldap_bind_pw);
				break;
#endif
			case 37:
				safestrncpy(config.c_ip_addr, buf,
						sizeof config.c_ip_addr);
			case 38:
				config.c_msa_port = atoi(buf);
				break;
			case 39:
				config.c_imaps_port = atoi(buf);
				break;
			case 40:
				config.c_pop3s_port = atoi(buf);
				break;
			case 41:
				config.c_smtps_port = atoi(buf);
				break;
			case 42:
				config.c_enable_fulltext = atoi(buf);
				break;
			case 43:
				config.c_auto_cull = atoi(buf);
				break;
			case 44:
				config.c_instant_expunge = atoi(buf);
				break;
			case 45:
				config.c_allow_spoofing = atoi(buf);
				break;
			case 46:
				config.c_journal_email = atoi(buf);
				break;
			case 47:
				config.c_journal_pubmsgs = atoi(buf);
				break;
			case 48:
				safestrncpy(config.c_journal_dest, buf,
						sizeof config.c_journal_dest);
			case 49:
				safestrncpy(config.c_default_cal_zone, buf,
						sizeof config.c_default_cal_zone);
				break;
			case 50:
				config.c_pftcpdict_port = atoi(buf);
				break;
			case 51:
				config.c_managesieve_port = atoi(buf);
				break;
			case 52:
				config.c_auth_mode = atoi(buf);
			case 53:
				safestrncpy(config.c_funambol_host, buf,
					sizeof config.c_funambol_host);
				break;
			case 54:
				config.c_funambol_port = atoi(buf);
				break;
			case 55:
				safestrncpy(config.c_funambol_source,
					buf, 
					sizeof config.c_funambol_source);
				break;
			case 56:
				safestrncpy(config.c_funambol_auth,
					buf,
					sizeof config.c_funambol_auth);
				break;
			case 57:
				config.c_rbl_at_greeting = atoi(buf);
				break;
			}
			++a;
		}
		put_config();
		snprintf(buf, sizeof buf,
			 "The global system configuration has been edited by %s.\n",
			 CC->curr_user);
		aide_message(buf,"Citadel Configuration Manager Message");

		if (strlen(config.c_logpages) > 0)
			create_room(config.c_logpages, 3, "", 0, 1, 1, VIEW_BBS);

		/* If full text indexing has been disabled, invalidate the
		 * index so it doesn't try to use it later.
		 */
		if (config.c_enable_fulltext == 0) {
			CitControl.fulltext_wordbreaker = 0;
			put_control();
		}
	}

	else if (!strcasecmp(cmd, "GETSYS")) {
		extract_token(confname, argbuf, 1, '|', sizeof confname);
		confptr = CtdlGetSysConfig(confname);
		if (confptr != NULL) {
			cprintf("%d %s\n", LISTING_FOLLOWS, confname);
			client_write(confptr, strlen(confptr));
			if (confptr[strlen(confptr) - 1] != 10)
				client_write("\n", 1);
			cprintf("000\n");
			free(confptr);
		} else {
			cprintf("%d No such configuration.\n",
				ERROR + ILLEGAL_VALUE);
		}
	}

	else if (!strcasecmp(cmd, "PUTSYS")) {
		extract_token(confname, argbuf, 1, '|', sizeof confname);
		unbuffer_output();
		cprintf("%d %s\n", SEND_LISTING, confname);
		confptr = CtdlReadMessageBody("000",
				config.c_maxmsglen, NULL, 0);
		CtdlPutSysConfig(confname, confptr);
		free(confptr);
	}

	else {
		cprintf("%d Illegal option(s) specified.\n",
			ERROR + ILLEGAL_VALUE);
	}
}
