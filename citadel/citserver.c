/* 
 * Main source module for the Citadel server
 *
 * Copyright (c) 1987-2010 by the citadel.org team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>

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

#if HAVE_BACKTRACE
#include <execinfo.h>
#endif

#include <ctype.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "sysdep_decls.h"
#include "threads.h"
#include "citserver.h"
#include "config.h"
#include "database.h"
#include "housekeeping.h"
#include "user_ops.h"
#include "msgbase.h"
#include "support.h"
#include "locate_host.h"
#include "room_ops.h"
#include "file_ops.h"
#include "control.h"
#include "euidindex.h"
#include "context.h"
#include "svn_revision.h"

#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif

#include "ctdl_module.h"

char *unique_session_numbers;
int ScheduledShutdown = 0;
time_t server_startup_time;
int panic_fd;

/*
 * print the actual stack frame.
 */
void cit_backtrace(void)
{
#ifdef HAVE_BACKTRACE
	void *stack_frames[50];
	size_t size, i;
	char **strings;


	size = backtrace(stack_frames, sizeof(stack_frames) / sizeof(void*));
	strings = backtrace_symbols(stack_frames, size);
	for (i = 0; i < size; i++) {
		if (strings != NULL)
			CtdlLogPrintf(CTDL_ALERT, "%s\n", strings[i]);
		else
			CtdlLogPrintf(CTDL_ALERT, "%p\n", stack_frames[i]);
	}
	free(strings);
#endif
}

/*
 * print the actual stack frame.
 */
void cit_panic_backtrace(int SigNum)
{
#ifdef HAVE_BACKTRACE
	void *stack_frames[10];
	size_t size, i;
	char **strings;

	printf("caught signal 11\n");
	size = backtrace(stack_frames, sizeof(stack_frames) / sizeof(void*));
	strings = backtrace_symbols(stack_frames, size);
	for (i = 0; i < size; i++) {
		if (strings != NULL)
			CtdlLogPrintf(CTDL_ALERT, "%s\n", strings[i]);
		else
			CtdlLogPrintf(CTDL_ALERT, "%p\n", stack_frames[i]);
	}
	free(strings);
#endif
	exit(-1);
}

/*
 * Various things that need to be initialized at startup
 */
void master_startup(void) {
	struct timeval tv;
	unsigned int seed;
	FILE *urandom;
	struct ctdlroom qrbuf;
	int rv;
	
	CtdlLogPrintf(CTDL_DEBUG, "master_startup() started\n");
	time(&server_startup_time);

	CtdlLogPrintf(CTDL_INFO, "Opening databases\n");
	open_databases();

	ctdl_thread_internal_init_tsd();
	
	CtdlThreadAllocTSD();
	
	check_ref_counts();

	CtdlLogPrintf(CTDL_INFO, "Creating base rooms (if necessary)\n");
	CtdlCreateRoom(config.c_baseroom,	0, "", 0, 1, 0, VIEW_BBS);
	CtdlCreateRoom(AIDEROOM,		3, "", 0, 1, 0, VIEW_BBS);
	CtdlCreateRoom(SYSCONFIGROOM,		3, "", 0, 1, 0, VIEW_BBS);
	CtdlCreateRoom(config.c_twitroom,	0, "", 0, 1, 0, VIEW_BBS);

	/* The "Local System Configuration" room doesn't need to be visible */
        if (CtdlGetRoomLock(&qrbuf, SYSCONFIGROOM) == 0) {
                qrbuf.QRflags2 |= QR2_SYSTEM;
                CtdlPutRoomLock(&qrbuf);
        }

	/* Aide needs to be public postable, else we're not RFC conformant. */
        if (CtdlGetRoomLock(&qrbuf, AIDEROOM) == 0) {
                qrbuf.QRflags2 |= QR2_SMTP_PUBLIC;
                CtdlPutRoomLock(&qrbuf);
        }

	CtdlLogPrintf(CTDL_INFO, "Seeding the pseudo-random number generator...\n");
	urandom = fopen("/dev/urandom", "r");
	if (urandom != NULL) {
		rv = fread(&seed, sizeof seed, 1, urandom);
		fclose(urandom);
	}
	else {
		gettimeofday(&tv, NULL);
		seed = tv.tv_usec;
	}
	srand(seed);
	srandom(seed);

	CtdlLogPrintf(CTDL_INFO, "Initializing ipgm secret\n");
	get_config();
	config.c_ipgm_secret = rand();
	put_config();

	CtdlLogPrintf(CTDL_DEBUG, "master_startup() finished\n");
}


/*
 * Cleanup routine to be called when the server is shutting down.
 */
void master_cleanup(int exitcode) {
	struct CleanupFunctionHook *fcn;
	static int already_cleaning_up = 0;

	if (already_cleaning_up) while(1) sleep(1);
	already_cleaning_up = 1;

	/* Run any cleanup routines registered by loadable modules */
	for (fcn = CleanupHookTable; fcn != NULL; fcn = fcn->next) {
		(*fcn->h_function_pointer)();
	}

	/* Close the AdjRefCount queue file */
	AdjRefCount(-1, 0);

	/* Do system-dependent stuff */
	sysdep_master_cleanup();
	
	/* Close databases */
	CtdlLogPrintf(CTDL_INFO, "Closing databases\n");
	close_databases();

#ifdef DEBUG_MEMORY_LEAKS
	dump_heap();
#endif

	/* If the operator requested a halt but not an exit, halt here. */
	if (shutdown_and_halt) {
		CtdlLogPrintf(CTDL_NOTICE, "citserver: Halting server without exiting.\n");
		fflush(stdout); fflush(stderr);
		while(1) {
			sleep(32767);
		}
	}
	
	release_control();

	/* Now go away. */
	CtdlLogPrintf(CTDL_NOTICE, "citserver: Exiting with status %d\n", exitcode);
	fflush(stdout); fflush(stderr);
	
	if (restart_server != 0)
		exit(1);
	if ((running_as_daemon != 0) && ((exitcode == 0) ))
		exitcode = CTDLEXIT_SHUTDOWN;
	exit(exitcode);
}



/*
 * cmd_info()  -  tell the client about this server
 */
void cmd_info(char *cmdbuf) {
	cprintf("%d Server info:\n", LISTING_FOLLOWS);
	cprintf("%d\n", CC->cs_pid);
	cprintf("%s\n", config.c_nodename);
	cprintf("%s\n", config.c_humannode);
	cprintf("%s\n", config.c_fqdn);
	cprintf("%s\n", CITADEL);
	cprintf("%d\n", REV_LEVEL);
	cprintf("%s\n", config.c_site_location);
	cprintf("%s\n", config.c_sysadm);
	cprintf("%d\n", SERVER_TYPE);
	cprintf("%s\n", config.c_moreprompt);
	cprintf("1\n");	/* 1 = yes, this system supports floors */
	cprintf("1\n"); /* 1 = we support the extended paging options */
	cprintf("%s\n", CC->cs_nonce);
	cprintf("1\n"); /* 1 = yes, this system supports the QNOP command */

#ifdef HAVE_LDAP
	cprintf("1\n"); /* 1 = yes, this server is LDAP-enabled */
#else
	cprintf("0\n"); /* 1 = no, this server is not LDAP-enabled */
#endif

	if (config.c_auth_mode == AUTHMODE_NATIVE) {
		cprintf("%d\n", config.c_disable_newu);
	}
	else {
		cprintf("1\n");	/* "create new user" does not work with non-native auth modes */
	}

	cprintf("%s\n", config.c_default_cal_zone);

	/* Output load averages */
	cprintf("%f\n", CtdlThreadLoadAvg);
	cprintf("%f\n", CtdlThreadWorkerAvg);
	cprintf("%d\n", CtdlThreadGetCount());

	cprintf("1\n");		/* yes, Sieve mail filtering is supported */
	cprintf("%d\n", config.c_enable_fulltext);
	cprintf("%s\n", svn_revision());

	if (config.c_auth_mode == AUTHMODE_NATIVE) {
		cprintf("1\n");	/* OpenID is enabled when using native auth */
	}
	else {
		cprintf("0\n");	/* OpenID is disabled when using non-native auth */
	}

	cprintf("%d\n", config.c_guest_logins);
	
	cprintf("000\n");
}


/*
 * returns an asterisk if there are any instant messages waiting,
 * space otherwise.
 */
char CtdlCheckExpress(void) {
	if (CC->FirstExpressMessage == NULL) {
		return(' ');
	}
	else {
		return('*');
	}
}

void cmd_time(char *argbuf)
{
   time_t tv;
   struct tm tmp;
   
   tv = time(NULL);
   localtime_r(&tv, &tmp);
   
   /* timezone and daylight global variables are not portable. */
#ifdef HAVE_STRUCT_TM_TM_GMTOFF
   cprintf("%d %ld|%ld|%d\n", CIT_OK, (long)tv, tmp.tm_gmtoff, tmp.tm_isdst);
#else
   cprintf("%d %ld|%ld|%d\n", CIT_OK, (long)tv, timezone, tmp.tm_isdst);
#endif
}


/*
 * Check originating host against the public_clients file.  This determines
 * whether the client is allowed to change the hostname for this session
 * (for example, to show the location of the user rather than the location
 * of the client).
 */
int is_public_client(void)
{
	char buf[1024];
	char addrbuf[1024];
	FILE *fp;
	int i;
	char *public_clientspos;
	char *public_clientsend;
	char *paddr = NULL;
	struct stat statbuf;
	static time_t pc_timestamp = 0;
	static char public_clients[SIZ];
	static char public_clients_file[SIZ];

#define LOCALHOSTSTR "127.0.0.1"

	snprintf(public_clients_file, sizeof public_clients_file, "%s/public_clients", ctdl_etc_dir);

	/*
	 * Check the time stamp on the public_clients file.  If it's been
	 * updated since the last time we were here (or if this is the first
	 * time we've been through the loop), read its contents and learn
	 * the IP addresses of the listed hosts.
	 */
	if (stat(public_clients_file, &statbuf) != 0) {
		/* No public_clients file exists, so bail out */
		CtdlLogPrintf(CTDL_WARNING, "Warning: '%s' does not exist\n", 
				public_clients_file);
		return(0);
	}

	if (statbuf.st_mtime > pc_timestamp) {
		begin_critical_section(S_PUBLIC_CLIENTS);
		CtdlLogPrintf(CTDL_INFO, "Loading %s\n", public_clients_file);

		public_clientspos = &public_clients[0];
		public_clientsend = public_clientspos + SIZ;
		safestrncpy(public_clientspos, LOCALHOSTSTR, sizeof public_clients);
		public_clientspos += sizeof(LOCALHOSTSTR) - 1;
		
		if (hostname_to_dotted_quad(addrbuf, config.c_fqdn) == 0) {
			*(public_clientspos++) = '|';
			paddr = &addrbuf[0];
			while (!IsEmptyStr (paddr) && 
			       (public_clientspos < public_clientsend))
				*(public_clientspos++) = *(paddr++);
		}

		fp = fopen(public_clients_file, "r");
		if (fp != NULL) 
			while ((fgets(buf, sizeof buf, fp)!=NULL) &&
			       (public_clientspos < public_clientsend)){
				char *ptr;
				ptr = buf;
				while (!IsEmptyStr(ptr)) {
					if (*ptr == '#') {
						*ptr = 0;
						break;
					}
				else ptr++;
				}
				ptr--;
				while (ptr>buf && isspace(*ptr)) {
					*(ptr--) = 0;
				}
				if (hostname_to_dotted_quad(addrbuf, buf) == 0) {
					*(public_clientspos++) = '|';
					paddr = addrbuf;
					while (!IsEmptyStr(paddr) && 
					       (public_clientspos < public_clientsend)){
						*(public_clientspos++) = *(paddr++);
					}
				}
			}
		fclose(fp);
		pc_timestamp = time(NULL);
		end_critical_section(S_PUBLIC_CLIENTS);
	}

	CtdlLogPrintf(CTDL_DEBUG, "Checking whether %s is a local or public client\n",
		CC->cs_addr);
	for (i=0; i<num_parms(public_clients); ++i) {
		extract_token(addrbuf, public_clients, i, '|', sizeof addrbuf);
		if (!strcasecmp(CC->cs_addr, addrbuf)) {
			CtdlLogPrintf(CTDL_DEBUG, "... yes it is.\n");
			return(1);
		}
	}

	/* No hits.  This is not a public client. */
	CtdlLogPrintf(CTDL_DEBUG, "... no it isn't.\n");
	return(0);
}


/*
 * the client is identifying itself to the server
 */
void cmd_iden(char *argbuf)
{
	int dev_code;
	int cli_code;
	int rev_level;
	char desc[128];
	char from_host[128];

	if (num_parms(argbuf)<4) {
		cprintf("%d usage error\n", ERROR + ILLEGAL_VALUE);
		return;
	}

	dev_code = extract_int(argbuf,0);
	cli_code = extract_int(argbuf,1);
	rev_level = extract_int(argbuf,2);
	extract_token(desc, argbuf, 3, '|', sizeof desc);

	safestrncpy(from_host, config.c_fqdn, sizeof from_host);
	from_host[sizeof from_host - 1] = 0;
	if (num_parms(argbuf)>=5) extract_token(from_host, argbuf, 4, '|', sizeof from_host);

	CC->cs_clientdev = dev_code;
	CC->cs_clienttyp = cli_code;
	CC->cs_clientver = rev_level;
	safestrncpy(CC->cs_clientname, desc, sizeof CC->cs_clientname);
	CC->cs_clientname[31] = 0;

	/* For local sockets and public clients, trust the hostname supplied by the client */
	if ( (CC->is_local_socket) || (is_public_client()) ) {
		safestrncpy(CC->cs_host, from_host, sizeof CC->cs_host);
		CC->cs_host[sizeof CC->cs_host - 1] = 0;
		CC->cs_addr[0] = 0;
	}

	CtdlLogPrintf(CTDL_NOTICE, "Client %d/%d/%01d.%02d (%s) from %s\n",
		dev_code,
		cli_code,
		(rev_level / 100),
		(rev_level % 100),
		desc,
		CC->cs_host
	);
	cprintf("%d Ok\n",CIT_OK);
}

typedef const char *ccharp;
/*
 * display system messages or help
 */
void cmd_mesg(char *mname)
{
	FILE *mfp;
	char targ[256];
	char buf[256];
	char buf2[256];
	char *dirs[2];
	DIR *dp;
	struct dirent *d;

	extract_token(buf, mname, 0, '|', sizeof buf);

	dirs[0] = strdup(ctdl_message_dir);
	dirs[1] = strdup(ctdl_hlp_dir);

	snprintf(buf2, sizeof buf2, "%s.%d.%d",
		buf, CC->cs_clientdev, CC->cs_clienttyp);

	/* If the client requested "?" then produce a listing */
	if (!strcmp(buf, "?")) {
		cprintf("%d %s\n", LISTING_FOLLOWS, buf);
		dp = opendir(dirs[1]);
		if (dp != NULL) {
			while (d = readdir(dp), d != NULL) {
				if (d->d_name[0] != '.') {
					cprintf(" %s\n", d->d_name);
				}
			}
			closedir(dp);
		}
		cprintf("000\n");
		free(dirs[0]);
		free(dirs[1]);
		return;
	}

	/* Otherwise, look for the requested file by name. */
	else {
		mesg_locate(targ, sizeof targ, buf2, 2, (const ccharp*)dirs);
		if (IsEmptyStr(targ)) {
			snprintf(buf2, sizeof buf2, "%s.%d",
							buf, CC->cs_clientdev);
			mesg_locate(targ, sizeof targ, buf2, 2,
				    (const ccharp*)dirs);
			if (IsEmptyStr(targ)) {
				mesg_locate(targ, sizeof targ, buf, 2,
					    (const ccharp*)dirs);
			}	
		}
	}

	free(dirs[0]);
	free(dirs[1]);

	if (IsEmptyStr(targ)) {
		cprintf("%d '%s' not found.  (Searching in %s and %s)\n",
			ERROR + FILE_NOT_FOUND,
			mname,
			ctdl_message_dir,
			ctdl_hlp_dir
		);
		return;
	}

	mfp = fopen(targ, "r");
	if (mfp==NULL) {
		cprintf("%d Cannot open '%s': %s\n",
			ERROR + INTERNAL_ERROR, targ, strerror(errno));
		return;
	}
	cprintf("%d %s\n", LISTING_FOLLOWS,buf);

	while (fgets(buf, (sizeof buf - 1), mfp) != NULL) {
		buf[strlen(buf)-1] = 0;
		do_help_subst(buf);
		cprintf("%s\n",buf);
	}

	fclose(mfp);
	cprintf("000\n");
}


/*
 * enter system messages or help
 */
void cmd_emsg(char *mname)
{
	FILE *mfp;
	char targ[256];
	char buf[256];
	char *dirs[2];
	int a;

	unbuffer_output();

	if (CtdlAccessCheck(ac_aide)) return;

	extract_token(buf, mname, 0, '|', sizeof buf);
	for (a=0; !IsEmptyStr(&buf[a]); ++a) {		/* security measure */
		if (buf[a] == '/') buf[a] = '.';
	}

	dirs[0] = strdup(ctdl_message_dir);
	dirs[1] = strdup(ctdl_hlp_dir);

	mesg_locate(targ, sizeof targ, buf, 2, (const ccharp*)dirs);
	free(dirs[0]);
	free(dirs[1]);

	if (IsEmptyStr(targ)) {
		snprintf(targ, sizeof targ, 
				 "%s/%s",
				 ctdl_hlp_dir, buf);
	}

	mfp = fopen(targ,"w");
	if (mfp==NULL) {
		cprintf("%d Cannot open '%s': %s\n",
			ERROR + INTERNAL_ERROR, targ, strerror(errno));
		return;
	}
	cprintf("%d %s\n", SEND_LISTING, targ);

	while (client_getln(buf, sizeof buf) >=0 && strcmp(buf, "000")) {
		fprintf(mfp, "%s\n", buf);
	}

	fclose(mfp);
}


/* Don't show the names of private rooms unless the viewing
 * user also knows the rooms.
 */
void GenerateRoomDisplay(char *real_room,
			CitContext *viewed,
			CitContext *viewer) {

	int ra;

	strcpy(real_room, viewed->room.QRname);
	if (viewed->room.QRflags & QR_MAILBOX) {
		strcpy(real_room, &real_room[11]);
	}
	if (viewed->room.QRflags & QR_PRIVATE) {
		CtdlRoomAccess(&viewed->room, &viewer->user, &ra, NULL);
		if ( (ra & UA_KNOWN) == 0) {
			strcpy(real_room, "<private room>");
		}
	}

	if (viewed->cs_flags & CS_CHAT) {
		while (strlen(real_room) < 14) {
			strcat(real_room, " ");
		}
		strcpy(&real_room[14], "<chat>");
	}

}

/*
 * Convenience function.
 */
int CtdlAccessCheck(int required_level) {

	if (CC->internal_pgm) return(0);
	if (required_level >= ac_internal) {
		cprintf("%d This is not a user-level command.\n",
			ERROR + HIGHER_ACCESS_REQUIRED);
		return(-1);
	}

	if ((required_level >= ac_logged_in_or_guest) && (CC->logged_in == 0) && (!config.c_guest_logins)) {
		cprintf("%d Not logged in.\n", ERROR + NOT_LOGGED_IN);
		return(-1);
	}

	if ((required_level >= ac_logged_in) && (CC->logged_in == 0)) {
		cprintf("%d Not logged in.\n", ERROR + NOT_LOGGED_IN);
		return(-1);
	}

	if (CC->user.axlevel >= AxAideU) return(0);
 	if (required_level >= ac_aide) {
		cprintf("%d This command requires Aide access.\n",
			ERROR + HIGHER_ACCESS_REQUIRED);
		return(-1);
	}

	if (is_room_aide()) return(0);
	if (required_level >= ac_room_aide) {
		cprintf("%d This command requires Aide or Room Aide access.\n",
			ERROR + HIGHER_ACCESS_REQUIRED);
		return(-1);
	}

	/* shhh ... succeed quietly */
	return(0);
}



/*
 * Terminate another running session
 */
void cmd_term(char *cmdbuf)
{
	int session_num;
	int terminated = 0;

	session_num = extract_int(cmdbuf, 0);

	terminated = CtdlTerminateOtherSession(session_num);

	if (terminated < 0) {
		cprintf("%d You can't kill your own session.\n", ERROR + ILLEGAL_VALUE);
		return;
	}

	if (terminated & TERM_FOUND) {
		if (terminated == TERM_KILLED) {
			cprintf("%d Session terminated.\n", CIT_OK);
		}
		else {
			cprintf("%d You are not allowed to do that.\n",
				ERROR + HIGHER_ACCESS_REQUIRED);
		}
	}
	else {
		cprintf("%d No such session.\n", ERROR + ILLEGAL_VALUE);
	}
}


/* 
 * get the paginator prompt
 */
void cmd_more(char *argbuf) {
	cprintf("%d %s\n", CIT_OK, config.c_moreprompt);
}


/*
 * echo 
 */
void cmd_echo(char *etext)
{
	cprintf("%d %s\n", CIT_OK, etext);
}


/* 
 * Perform privilege escalation for an internal program
 */
void cmd_ipgm(char *argbuf)
{
	int secret;

	secret = extract_int(argbuf, 0);

	/* For security reasons, we do NOT allow this command to run
	 * over the network.  Local sockets only.
	 */
	if (!CC->is_local_socket) {
		sleep(5);
		cprintf("%d Authentication failed.\n", ERROR + PASSWORD_REQUIRED);
	}
	else if (secret == config.c_ipgm_secret) {
		CC->internal_pgm = 1;
		strcpy(CC->curr_user, "<internal program>");
		CC->cs_flags = CC->cs_flags|CS_STEALTH;
		cprintf("%d Authenticated as an internal program.\n", CIT_OK);
	}
	else {
		sleep(5);
		cprintf("%d Authentication failed.\n", ERROR + PASSWORD_REQUIRED);
		CtdlLogPrintf(CTDL_ERR, "Warning: ipgm authentication failed.\n");
		CC->kill_me = 1;
	}
}


/*
 * Shut down the server
 */
void cmd_down(char *argbuf) {
	char *Reply ="%d Shutting down server.  Goodbye.\n";

	if (CtdlAccessCheck(ac_aide)) return;

	if (!IsEmptyStr(argbuf))
	{
		int state = CIT_OK;
		restart_server = extract_int(argbuf, 0);
		
		if (restart_server > 0)
		{
			Reply = "%d citserver will now shut down and automatically restart.\n";
		}
		if ((restart_server > 0) && !running_as_daemon)
		{
			CtdlLogPrintf(CTDL_ERR, "The user requested restart, but not running as daemon! Geronimooooooo!\n");
			Reply = "%d Warning: citserver is not running in daemon mode and is therefore unlikely to restart automatically.\n";
			state = ERROR;
		}
		cprintf(Reply, state);
	}
	else
	{
		cprintf(Reply, CIT_OK + SERVER_SHUTTING_DOWN); 
	}
	CC->kill_me = 1; /* Even the DOWN command has to follow correct proceedure when disconecting */
	CtdlThreadStopAll();
}


/*
 * Halt the server without exiting the server process.
 */
void cmd_halt(char *argbuf) {

	if (CtdlAccessCheck(ac_aide)) return;

	cprintf("%d Halting server.  Goodbye.\n", CIT_OK);
	CtdlThreadStopAll();
	shutdown_and_halt = 1;
}


/*
 * Schedule or cancel a server shutdown
 */
void cmd_scdn(char *argbuf)
{
	int new_state;
	int state = CIT_OK;
	char *Reply = "%d %d\n";

	if (CtdlAccessCheck(ac_aide)) return;

	new_state = extract_int(argbuf, 0);
	if ((new_state == 2) || (new_state == 3))
	{
		restart_server = 1;
		if (!running_as_daemon)
		{
			CtdlLogPrintf(CTDL_ERR, "The user requested restart, but not running as deamon! Geronimooooooo!\n");
			Reply = "%d %d Warning, not running in deamon mode. maybe we will come up again, but don't lean on it.\n";
			state = ERROR;
		}

		restart_server = extract_int(argbuf, 0);
		new_state -= 2;
	}
	if ((new_state == 0) || (new_state == 1)) {
		ScheduledShutdown = new_state;
	}
	cprintf(Reply, state, ScheduledShutdown);
}


/*
 * Set or unset asynchronous protocol mode
 */
void cmd_asyn(char *argbuf)
{
	int new_state;

	new_state = extract_int(argbuf, 0);
	if ((new_state == 0) || (new_state == 1)) {
		CC->is_async = new_state;
	}
	cprintf("%d %d\n", CIT_OK, CC->is_async);
}


/*
 * Generate a "nonce" for APOP-style authentication.
 *
 * RFC 1725 et al specify a PID to be placed in front of the nonce.
 * Quoth BTX: That would be stupid.
 */
void generate_nonce(CitContext *con) {
	struct timeval tv;

	memset(con->cs_nonce, NONCE_SIZE, 0);
	gettimeofday(&tv, NULL);
	memset(con->cs_nonce, NONCE_SIZE, 0);
	snprintf(con->cs_nonce, NONCE_SIZE, "<%d%ld@%s>",
		rand(), (long)tv.tv_usec, config.c_fqdn);
}


/*
 * Back-end function for starting a session
 */
void begin_session(CitContext *con)
{
	socklen_t len;
	struct sockaddr_in sin;

	/* 
	 * Initialize some variables specific to our context.
	 */
	con->logged_in = 0;
	con->internal_pgm = 0;
	con->download_fp = NULL;
	con->upload_fp = NULL;
	con->FirstExpressMessage = NULL;
	time(&con->lastcmd);
	time(&con->lastidle);
	strcpy(con->lastcmdname, "    ");
	strcpy(con->cs_clientname, "(unknown)");
	strcpy(con->curr_user, NLI);
	*con->net_node = '\0';
	*con->fake_username = '\0';
	*con->fake_hostname = '\0';
	*con->fake_roomname = '\0';
	*con->cs_clientinfo = '\0';
	generate_nonce(con);
	safestrncpy(con->cs_host, config.c_fqdn, sizeof con->cs_host);
	safestrncpy(con->cs_addr, "", sizeof con->cs_addr);
	con->cs_UDSclientUID = -1;
	con->cs_host[sizeof con->cs_host - 1] = 0;
	len = sizeof sin;
	if (!CC->is_local_socket) {
		locate_host(con->cs_host, sizeof con->cs_host,
			con->cs_addr, sizeof con->cs_addr,
			con->client_socket
		);
	}
	else {
		con->cs_host[0] = 0;
		con->cs_addr[0] = 0;
#ifdef HAVE_STRUCT_UCRED
		{
			/* as http://www.wsinnovations.com/softeng/articles/uds.html told us... */
			struct ucred credentials;
			socklen_t ucred_length = sizeof(struct ucred);
			
			/*fill in the user data structure */
			if(getsockopt(con->client_socket, SOL_SOCKET, SO_PEERCRED, &credentials, &ucred_length)) {
				CtdlLogPrintf(CTDL_NOTICE, "could obtain credentials from unix domain socket");
				
			}
			else {		
				/* the process ID of the process on the other side of the socket */
				/* credentials.pid; */
				
				/* the effective UID of the process on the other side of the socket  */
				con->cs_UDSclientUID = credentials.uid;
				
				/* the effective primary GID of the process on the other side of the socket */
				/* credentials.gid; */
				
				/* To get supplemental groups, we will have to look them up in our account
				   database, after a reverse lookup on the UID to get the account name.
				   We can take this opportunity to check to see if this is a legit account.
				*/
				snprintf(con->cs_clientinfo, sizeof(con->cs_clientinfo),
					 "PID: "F_PID_T"; UID: "F_UID_T"; GID: "F_XPID_T" ", 
					 credentials.pid,
					 credentials.uid,
					 credentials.gid);
			}
		}
#endif
	}
	con->cs_flags = 0;
	con->upload_type = UPL_FILE;
	con->dl_is_net = 0;

	con->nologin = 0;
	if (((config.c_maxsessions > 0)&&(num_sessions > config.c_maxsessions)) || CtdlWantSingleUser()) {
		con->nologin = 1;
	}

	if (!CC->is_local_socket) {
		CtdlLogPrintf(CTDL_NOTICE, "Session (%s) started from %s (%s).\n", con->ServiceName, con->cs_host, con->cs_addr);
	}
	else {
		CtdlLogPrintf(CTDL_NOTICE, "Session (%s) started via local socket UID:%d.\n", con->ServiceName, con->cs_UDSclientUID);
	}

	/* Run any session startup routines registered by loadable modules */
	PerformSessionHooks(EVT_START);
}


void citproto_begin_session() {
	if (CC->nologin==1) {
		cprintf("%d %s: Too many users are already online (maximum is %d)\n",
			ERROR + MAX_SESSIONS_EXCEEDED,
			config.c_nodename, config.c_maxsessions
		);
		CC->kill_me = 1;
	}
	else {
		cprintf("%d %s Citadel server ready.\n", CIT_OK, config.c_nodename);
		CC->can_receive_im = 1;
	}
}


void cmd_noop(char *argbuf)
{
	cprintf("%d%cok\n", CIT_OK, CtdlCheckExpress() );
}


void cmd_qnop(char *argbuf)
{
	/* do nothing, this command returns no response */
}


void cmd_quit(char *argbuf)
{
	cprintf("%d Goodbye.\n", CIT_OK);
	CC->kill_me = 1;
}


void cmd_lout(char *argbuf)
{
	if (CC->logged_in) 
		CtdlUserLogout();
	cprintf("%d logged out.\n", CIT_OK);
}


/*
 * This loop recognizes all server commands.
 */
void do_command_loop(void) {
	char cmdbuf[SIZ];
	const char *old_name = NULL;
	
	old_name = CtdlThreadName("do_command_loop");
	
	time(&CC->lastcmd);
	memset(cmdbuf, 0, sizeof cmdbuf); /* Clear it, just in case */
	if (client_getln(cmdbuf, sizeof cmdbuf) < 1) {
		CtdlLogPrintf(CTDL_ERR, "Client disconnected: ending session.\n");
		CC->kill_me = 1;
		CtdlThreadName(old_name);
		return;
	}

	/* Log the server command, but don't show passwords... */
	if ( (strncasecmp(cmdbuf, "PASS", 4)) && (strncasecmp(cmdbuf, "SETP", 4)) ) {
		CtdlLogPrintf(CTDL_INFO, "CtdlCommand [%s] [%s] %s\n", CTDLUSERIP, CC->curr_user, cmdbuf);
	}
	else {
		CtdlLogPrintf(CTDL_INFO, "CtdlCommand [%s] [%s] <password command hidden from log>\n", CTDLUSERIP, CC->curr_user);
	}

	buffer_output();

	/*
	 * Let other clients see the last command we executed, and
	 * update the idle time, but not NOOP, QNOP, PEXP, GEXP, RWHO, or TIME.
	 */
	if ( (strncasecmp(cmdbuf, "NOOP", 4))
	   && (strncasecmp(cmdbuf, "QNOP", 4))
	   && (strncasecmp(cmdbuf, "PEXP", 4))
	   && (strncasecmp(cmdbuf, "GEXP", 4))
	   && (strncasecmp(cmdbuf, "RWHO", 4))
	   && (strncasecmp(cmdbuf, "TIME", 4)) ) {
		strcpy(CC->lastcmdname, "    ");
		safestrncpy(CC->lastcmdname, cmdbuf, sizeof(CC->lastcmdname));
		time(&CC->lastidle);
	}
	
	CtdlThreadName(cmdbuf);
		
	if ((strncasecmp(cmdbuf, "ENT0", 4))
	   && (strncasecmp(cmdbuf, "MESG", 4))
	   && (strncasecmp(cmdbuf, "MSGS", 4)))
	{
	   CC->cs_flags &= ~CS_POSTING;
	}
		   
	if (!DLoader_Exec_Cmd(cmdbuf)) {
		cprintf("%d Unrecognized or unsupported command.\n", ERROR + CMD_NOT_SUPPORTED);
	}	

	unbuffer_output();

	/* Run any after-each-command routines registered by modules */
	PerformSessionHooks(EVT_CMD);
	CtdlThreadName(old_name);
}


/*
 * This loop performs all asynchronous functions.
 */
void do_async_loop(void) {
	PerformSessionHooks(EVT_ASYNC);
}


/*****************************************************************************/
/*                      MODULE INITIALIZATION STUFF                          */
/*****************************************************************************/

CTDL_MODULE_INIT(citserver)
{
	if (!threading) {
		CtdlRegisterProtoHook(cmd_noop, "NOOP", "no operation");
		CtdlRegisterProtoHook(cmd_qnop, "QNOP", "no operation with no response");
		CtdlRegisterProtoHook(cmd_quit, "QUIT", "log out and disconnect from server");
		CtdlRegisterProtoHook(cmd_lout, "LOUT", "log out but do not disconnect from server");
		CtdlRegisterProtoHook(cmd_asyn, "ASYN", "enable asynchronous server responses");
		CtdlRegisterProtoHook(cmd_info, "INFO", "fetch server capabilities and configuration");
		CtdlRegisterProtoHook(cmd_mesg, "MESG", "fetch system banners");
		CtdlRegisterProtoHook(cmd_emsg, "EMSG", "submit system banners");
		CtdlRegisterProtoHook(cmd_echo, "ECHO", "echo text back to the client");
		CtdlRegisterProtoHook(cmd_more, "MORE", "fetch the paginator prompt");
		CtdlRegisterProtoHook(cmd_iden, "IDEN", "identify the client software and location");
		CtdlRegisterProtoHook(cmd_ipgm, "IPGM", "perform privilege escalation for internal programs");
		CtdlRegisterProtoHook(cmd_term, "TERM", "terminate another running session");
		CtdlRegisterProtoHook(cmd_down, "DOWN", "perform a server shutdown");
		CtdlRegisterProtoHook(cmd_halt, "HALT", "halt the server without exiting the server process");
		CtdlRegisterProtoHook(cmd_scdn, "SCDN", "schedule or cancel a server shutdown");
		CtdlRegisterProtoHook(cmd_time, "TIME", "fetch the date and time from the server");
	}
        /* return our id for the Log */
	return "citserver";
}
