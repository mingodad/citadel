/* 
 * Main source module for the Citadel server
 *
 * Copyright (c) 1987-2014 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <stdio.h>
#include "sysdep.h"
#include <time.h>
#if HAVE_BACKTRACE
#include <execinfo.h>
#endif
#include <libcitadel.h>

#include "ctdl_module.h"
#include "housekeeping.h"
#include "locate_host.h"
#include "citserver.h"
#include "user_ops.h"
#include "control.h"
#include "config.h"

char *unique_session_numbers;
int ScheduledShutdown = 0;
time_t server_startup_time;
int panic_fd;
int openid_level_supported = 0;

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
			syslog(LOG_ALERT, "%s\n", strings[i]);
		else
			syslog(LOG_ALERT, "%p\n", stack_frames[i]);
	}
	free(strings);
#endif
}

void cit_oneline_backtrace(void)
{
#ifdef HAVE_BACKTRACE
	void *stack_frames[50];
	size_t size, i;
	char **strings;
	StrBuf *Buf;

	size = backtrace(stack_frames, sizeof(stack_frames) / sizeof(void*));
	strings = backtrace_symbols(stack_frames, size);
	if (size > 0)
	{
		Buf = NewStrBuf();
		for (i = 1; i < size; i++) {
			if (strings != NULL)
				StrBufAppendPrintf(Buf, "%s : ", strings[i]);
			else
				StrBufAppendPrintf(Buf, "%p : ", stack_frames[i]);
		}
		free(strings);
		syslog(LOG_ALERT, "%s\n", ChrPtr(Buf));
		FreeStrBuf(&Buf);
	}
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
			syslog(LOG_ALERT, "%s\n", strings[i]);
		else
			syslog(LOG_ALERT, "%p\n", stack_frames[i]);
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
	
	syslog(LOG_DEBUG, "master_startup() started\n");
	time(&server_startup_time);
	get_config();

	syslog(LOG_INFO, "Opening databases\n");
	open_databases();
	check_ref_counts();

	syslog(LOG_INFO, "Creating base rooms (if necessary)\n");
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

	syslog(LOG_INFO, "Seeding the pseudo-random number generator...\n");
	urandom = fopen("/dev/urandom", "r");
	if (urandom != NULL) {
		rv = fread(&seed, sizeof seed, 1, urandom);
		if (rv == -1)
			syslog(LOG_EMERG, "failed to read random seed: %s\n", 
			       strerror(errno));
		fclose(urandom);
	}
	else {
		gettimeofday(&tv, NULL);
		seed = tv.tv_usec;
	}
	srand(seed);
	srandom(seed);

	put_config();

	syslog(LOG_DEBUG, "master_startup() finished\n");
}


/*
 * Cleanup routine to be called when the server is shutting down.
 */
void master_cleanup(int exitcode) {
	struct CleanupFunctionHook *fcn;
	static int already_cleaning_up = 0;

	if (already_cleaning_up) while(1) usleep(1000000);
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
	syslog(LOG_INFO, "Closing databases\n");
	close_databases();

	/* If the operator requested a halt but not an exit, halt here. */
	if (shutdown_and_halt) {
		syslog(LOG_NOTICE, "citserver: Halting server without exiting.\n");
		fflush(stdout); fflush(stderr);
		while(1) {
			sleep(32767);
		}
	}
	
	release_control();

	/* Now go away. */
	syslog(LOG_NOTICE, "citserver: Exiting with status %d\n", exitcode);
	fflush(stdout); fflush(stderr);
	
	if (restart_server != 0)
		exit(1);
	if ((running_as_daemon != 0) && ((exitcode == 0) ))
		exitcode = CTDLEXIT_SHUTDOWN;
	exit(exitcode);
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


/*
 * Check originating host against the public_clients file.  This determines
 * whether the client is allowed to change the hostname for this session
 * (for example, to show the location of the user rather than the location
 * of the client).
 */
int CtdlIsPublicClient(void)
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
		syslog(LOG_WARNING, "Warning: '%s' does not exist\n", 
				public_clients_file);
		return(0);
	}

	if (statbuf.st_mtime > pc_timestamp) {
		begin_critical_section(S_PUBLIC_CLIENTS);
		syslog(LOG_INFO, "Loading %s\n", public_clients_file);

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
		if (fp != NULL) fclose(fp);
		pc_timestamp = time(NULL);
		end_critical_section(S_PUBLIC_CLIENTS);
	}

	syslog(LOG_DEBUG, "Checking whether %s is a local or public client\n",
		CC->cs_addr);
	for (i=0; i<num_parms(public_clients); ++i) {
		extract_token(addrbuf, public_clients, i, '|', sizeof addrbuf);
		if (!strcasecmp(CC->cs_addr, addrbuf)) {
			syslog(LOG_DEBUG, "... yes its local.\n");
			return(1);
		}
	}

	/* No hits.  This is not a public client. */
	syslog(LOG_DEBUG, "... no it isn't.\n");
	return(0);
}





void citproto_begin_session() {
	if (CC->nologin==1) {
		cprintf("%d %s: Too many users are already online (maximum is %d)\n",
			ERROR + MAX_SESSIONS_EXCEEDED,
			config.c_nodename, config.c_maxsessions
		);
		CC->kill_me = KILLME_MAX_SESSIONS_EXCEEDED;
	}
	else {
		cprintf("%d %s Citadel server ready.\n", CIT_OK, config.c_nodename);
		CC->can_receive_im = 1;
	}
}


void citproto_begin_admin_session() {
	CC->internal_pgm = 1;
	cprintf("%d %s Citadel server ADMIN CONNECTION ready.\n", CIT_OK, config.c_nodename);
}




/*
 * This loop recognizes all server commands.
 */
void do_command_loop(void) {
	char cmdbuf[SIZ];
	
	time(&CC->lastcmd);
	memset(cmdbuf, 0, sizeof cmdbuf); /* Clear it, just in case */
	if (client_getln(cmdbuf, sizeof cmdbuf) < 1) {
		syslog(LOG_ERR, "Citadel client disconnected: ending session.\n");
		CC->kill_me = KILLME_CLIENT_DISCONNECTED;
		return;
	}

	/* Log the server command, but don't show passwords... */
	if ( (strncasecmp(cmdbuf, "PASS", 4)) && (strncasecmp(cmdbuf, "SETP", 4)) ) {
		syslog(LOG_INFO, "[%d][%s(%ld)] %s",
			CC->cs_pid, CC->curr_user, CC->user.usernum, cmdbuf
		);
	}
	else {
		syslog(LOG_INFO, "[%d][%s(%ld)] <password command hidden from log>",
			CC->cs_pid, CC->curr_user, CC->user.usernum
		);
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
}


/*
 * This loop performs all asynchronous functions.
 */
void do_async_loop(void) {
	PerformSessionHooks(EVT_ASYNC);
}

