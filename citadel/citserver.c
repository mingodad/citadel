/* 
 * $Id$
 *
 * Main source module for the Citadel server
 *
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
#include "citadel.h"
#include "server.h"
#include "sysdep_decls.h"
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
#include "policy.h"
#include "control.h"
#include "tools.h"
#include "euidindex.h"
#include "serv_network.h"	/* Needed for destroy_network_queue_room called from master_cleanup */

#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif

struct CitContext *ContextList = NULL;
struct CitContext* next_session = NULL;
char *unique_session_numbers;
int ScheduledShutdown = 0;
int do_defrag = 0;
time_t server_startup_time;

/*
 * Various things that need to be initialized at startup
 */
void master_startup(void) {
	struct timeval tv;
	unsigned int seed;
	FILE *urandom;
	struct ctdlroom qrbuf;
	
	lprintf(CTDL_DEBUG, "master_startup() started\n");
	time(&server_startup_time);

	lprintf(CTDL_INFO, "Opening databases\n");
	open_databases();

	if (do_defrag) {
		defrag_databases();
	}

	check_ref_counts();

	lprintf(CTDL_INFO, "Creating base rooms (if necessary)\n");
	create_room(config.c_baseroom,	0, "", 0, 1, 0, VIEW_BBS);
	create_room(AIDEROOM,		3, "", 0, 1, 0, VIEW_BBS);
	create_room(SYSCONFIGROOM,	3, "", 0, 1, 0, VIEW_BBS);
	create_room(config.c_twitroom,	0, "", 0, 1, 0, VIEW_BBS);

	/* The "Local System Configuration" room doesn't need to be visible */
        if (lgetroom(&qrbuf, SYSCONFIGROOM) == 0) {
                qrbuf.QRflags2 |= QR2_SYSTEM;
                lputroom(&qrbuf);
        }

	lprintf(CTDL_INFO, "Seeding the pseudo-random number generator...\n");
	urandom = fopen("/dev/urandom", "r");
	if (urandom != NULL) {
		fread(&seed, sizeof seed, 1, urandom);
		fclose(urandom);
	}
	else {
		gettimeofday(&tv, NULL);
		seed = tv.tv_usec;
	}
	srandom(seed);

	lprintf(CTDL_INFO, "Initializing ipgm secret\n");
	get_config();
	config.c_ipgm_secret = rand();
	put_config();

	lprintf(CTDL_DEBUG, "master_startup() finished\n");
}


/*
 * Cleanup routine to be called when the server is shutting down.
 */
void master_cleanup(int exitcode) {
	struct CleanupFunctionHook *fcn;
	struct MaintenanceThreadHook *m_fcn;
	static int already_cleaning_up = 0;

	if (already_cleaning_up) while(1) sleep(1);
	already_cleaning_up = 1;

	/* Run any cleanup routines registered by loadable modules */
	for (fcn = CleanupHookTable; fcn != NULL; fcn = fcn->next) {
		(*fcn->h_function_pointer)();
	}

	/* Close the AdjRefCount queue file */
	AdjRefCount(-1, 0);

	for (m_fcn = MaintenanceThreadHookTable; m_fcn != NULL; m_fcn = m_fcn->next) {
		lprintf(CTDL_INFO, "Waiting for maintenance thread \"%s\" to shut down\n", m_fcn->name);
		pthread_join(m_fcn->MaintenanceThread_tid, NULL);
	}
	

	/* Close databases */
	lprintf(CTDL_INFO, "Closing databases\n");
	close_databases();

	/* flush the networker stuff */
	destroy_network_queue_room();

	/* Do system-dependent stuff */
	sysdep_master_cleanup();
	
#ifdef DEBUG_MEMORY_LEAKS
	dump_heap();
#endif

	/* If the operator requested a halt but not an exit, halt here. */
	if (shutdown_and_halt) {
		lprintf(CTDL_NOTICE, "citserver: Halting server without exiting.\n");
		fflush(stdout); fflush(stderr);
		while(1) {
			sleep(32767);
		}
	}
	
	release_control();

	/* Now go away. */
	lprintf(CTDL_NOTICE, "citserver: Exiting with status %d\n", exitcode);
	fflush(stdout); fflush(stderr);
	
	if (restart_server != 0)
		exit(1);
	exit(exitcode);
}



/*
 * Terminate a session.
 */
void RemoveContext (struct CitContext *con)
{
	if (con==NULL) {
		lprintf(CTDL_ERR,
			"WARNING: RemoveContext() called with NULL!\n");
		return;
	}
	lprintf(CTDL_DEBUG, "RemoveContext() session %d\n", con->cs_pid);

	/* Run any cleanup routines registered by loadable modules.
	 * Note: We have to "become_session()" because the cleanup functions
	 *       might make references to "CC" assuming it's the right one.
	 */
	become_session(con);
	PerformSessionHooks(EVT_STOP);
	become_session(NULL);

	/* Now handle all of the administrivia. */
	lprintf(CTDL_DEBUG, "Calling logout(%d)\n", con->cs_pid);
	logout(con);

	lprintf(CTDL_NOTICE, "[%3d] Session ended.\n", con->cs_pid);

	/* If the client is still connected, blow 'em away. */
	lprintf(CTDL_DEBUG, "Closing socket %d\n", con->client_socket);
	close(con->client_socket);

	lprintf(CTDL_DEBUG, "Done with RemoveContext()\n");
}





/*
 * cmd_info()  -  tell the client about this server
 */
void cmd_info(void) {
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

	if (config.c_auth_mode == 1) {
		cprintf("1\n");	/* "create new user" never works with host auth */
	}
	else {
		cprintf("%d\n", config.c_disable_newu); /* otherwise, site defined */
	}

	cprintf("%s\n", config.c_default_cal_zone);

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

void cmd_time(void)
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
	struct stat statbuf;
	static time_t pc_timestamp = 0;
	static char public_clients[SIZ];
	static char public_clients_file[SIZ];

	snprintf(public_clients_file, 
			 sizeof public_clients_file,
			 "%s/public_clients",
			 ctdl_etc_dir);

	/*
	 * Check the time stamp on the public_clients file.  If it's been
	 * updated since the last time we were here (or if this is the first
	 * time we've been through the loop), read its contents and learn
	 * the IP addresses of the listed hosts.
	 */
	if (stat(public_clients_file, &statbuf) != 0) {
		/* No public_clients file exists, so bail out */
		lprintf(CTDL_WARNING, "Warning: '%s' does not exist\n", 
				public_clients_file);
		return(0);
	}

	if (statbuf.st_mtime > pc_timestamp) {
		begin_critical_section(S_PUBLIC_CLIENTS);
		lprintf(CTDL_INFO, "Loading %s\n", public_clients_file);

		safestrncpy(public_clients, "127.0.0.1", sizeof public_clients);
		if (hostname_to_dotted_quad(addrbuf, config.c_fqdn) == 0) {
			strcat(public_clients, "|");
			strcat(public_clients, addrbuf);
		}

		fp = fopen(public_clients_file, "r");
		if (fp != NULL) while (fgets(buf, sizeof buf, fp)!=NULL) {
			for (i=0; i<strlen(buf); ++i) {
				if (buf[i] == '#') buf[i] = 0;
			}
			while (isspace((buf[strlen(buf)-1]))) {
				buf[strlen(buf)-1] = 0;
			}
			if (hostname_to_dotted_quad(addrbuf, buf) == 0) {
				if ((strlen(public_clients) +
				   strlen(addrbuf) + 2)
				   < sizeof(public_clients)) {
					strcat(public_clients, "|");
					strcat(public_clients, addrbuf);
				}
			}
		}
		fclose(fp);
		pc_timestamp = time(NULL);
		end_critical_section(S_PUBLIC_CLIENTS);
	}

	lprintf(CTDL_DEBUG, "Checking whether %s is a local or public client\n",
		CC->cs_addr);
	for (i=0; i<num_parms(public_clients); ++i) {
		extract_token(addrbuf, public_clients, i, '|', sizeof addrbuf);
		if (!strcasecmp(CC->cs_addr, addrbuf)) {
			lprintf(CTDL_DEBUG, "... yes it is.\n");
			return(1);
		}
	}

	/* No hits.  This is not a public client. */
	lprintf(CTDL_DEBUG, "... no it isn't.\n");
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
	struct in_addr addr;
	int do_lookup = 0;

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

	if (strlen(from_host) > 0) {
		if (CC->is_local_socket) do_lookup = 1;
		else if (is_public_client()) do_lookup = 1;
	}

	if (do_lookup) {
		lprintf(CTDL_DEBUG, "Looking up hostname '%s'\n", from_host);
		if ((addr.s_addr = inet_addr(from_host)) != -1) {
			locate_host(CC->cs_host, sizeof CC->cs_host,
				CC->cs_addr, sizeof CC->cs_addr,
				&addr);
		}
	   	else {
			safestrncpy(CC->cs_host, from_host, sizeof CC->cs_host);
			CC->cs_host[sizeof CC->cs_host - 1] = 0;
		}
	}

	lprintf(CTDL_NOTICE, "Client %d/%d/%01d.%02d (%s) from %s\n",
		dev_code,
		cli_code,
		(rev_level / 100),
		(rev_level % 100),
		desc,
		CC->cs_host);
	cprintf("%d Ok\n",CIT_OK);
}


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
		mesg_locate(targ, sizeof targ, buf2, 2, (const char **)dirs);
		if (strlen(targ) == 0) {
			snprintf(buf2, sizeof buf2, "%s.%d",
							buf, CC->cs_clientdev);
			mesg_locate(targ, sizeof targ, buf2, 2,
							(const char **)dirs);
			if (strlen(targ) == 0) {
				mesg_locate(targ, sizeof targ, buf, 2,
							(const char **)dirs);
			}	
		}
	}

	free(dirs[0]);
	free(dirs[1]);

	if (strlen(targ)==0) {
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
	for (a=0; a<strlen(buf); ++a) {		/* security measure */
		if (buf[a] == '/') buf[a] = '.';
	}

	dirs[0] = strdup(ctdl_message_dir);
	dirs[1] = strdup(ctdl_hlp_dir);

	mesg_locate(targ, sizeof targ, buf, 2, (const char**)dirs);
	free(dirs[0]);
	free(dirs[1]);

	if (strlen(targ)==0) {
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

	while (client_getln(buf, sizeof buf), strcmp(buf, "000")) {
		fprintf(mfp, "%s\n", buf);
	}

	fclose(mfp);
}


/* Don't show the names of private rooms unless the viewing
 * user also knows the rooms.
 */
void GenerateRoomDisplay(char *real_room,
			struct CitContext *viewed,
			struct CitContext *viewer) {

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
		while (strlen(real_room) < 14)
			strcat(real_room, " ");

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

	if ((required_level >= ac_logged_in) && (CC->logged_in == 0)) {
		cprintf("%d Not logged in.\n", ERROR + NOT_LOGGED_IN);
		return(-1);
	}

	if (CC->user.axlevel >= 6) return(0);
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
	struct CitContext *ccptr;
	int found_it = 0;
	int allowed = 0;

	session_num = extract_int(cmdbuf, 0);
	if (session_num == CC->cs_pid) {
		cprintf("%d You can't kill your own session.\n", ERROR + ILLEGAL_VALUE);
		return;
	}

	lprintf(CTDL_DEBUG, "Locating session to kill\n");
	begin_critical_section(S_SESSION_TABLE);
	for (ccptr = ContextList; ccptr != NULL; ccptr = ccptr->next) {
		if (session_num == ccptr->cs_pid) {
			found_it = 1;
			if ((ccptr->user.usernum == CC->user.usernum)
			   || (CC->user.axlevel >= 6)) {
				allowed = 1;
				ccptr->kill_me = 1;
			}
			else {
				allowed = 0;
			}
		}
	}
	end_critical_section(S_SESSION_TABLE);

	if (found_it) {
		if (allowed) {
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
void cmd_more(void) {
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
 * identify as internal program
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
		cprintf("%d Authentication failed.\n",
			ERROR + PASSWORD_REQUIRED);
	}
	else if (secret == config.c_ipgm_secret) {
		CC->internal_pgm = 1;
		strcpy(CC->curr_user, "<internal program>");
		CC->cs_flags = CC->cs_flags|CS_STEALTH;
		cprintf("%d Authenticated as an internal program.\n", CIT_OK);
	}
	else {
		sleep(5);
		cprintf("%d Authentication failed.\n",
			ERROR + PASSWORD_REQUIRED);
		lprintf(CTDL_ERR, "Warning: ipgm authentication failed.\n");
		CC->kill_me = 1;
	}

	/* Now change the ipgm secret for the next round.
	 * (Disabled because it breaks concurrent scripts.  The fact that
	 * we no longer accept IPGM over the network should be sufficient
	 * to prevent brute-force attacks.  If you don't agree, uncomment
	 * this block.)
	get_config();
	config.c_ipgm_secret = rand();
	put_config();
	*/
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
			Reply = "%d Restarting server.  See you soon.\n";
		if ((restart_server > 0) && !running_as_daemon)
		{
			lprintf(CTDL_ERR, "The user requested restart, but not running as deamon! Geronimooooooo!\n");
			Reply = "%d Warning, not running in deamon mode. maybe we will come up again, but don't lean on it.\n";
			state = ERROR;
		}
		cprintf(Reply, state);
	}
	else
	{
		cprintf(Reply, CIT_OK);
	}
	time_to_die = 1;
}

/*
 * Halt the server without exiting the server process.
 */
void cmd_halt(void) {

	if (CtdlAccessCheck(ac_aide)) return;

	cprintf("%d Halting server.  Goodbye.\n", CIT_OK);
	time_to_die = 1;
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
			lprintf(CTDL_ERR, "The user requested restart, but not running as deamon! Geronimooooooo!\n");
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
void generate_nonce(struct CitContext *con) {
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
void begin_session(struct CitContext *con)
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
	strcpy(con->net_node, "");
	strcpy(con->fake_username, "");
	strcpy(con->fake_hostname, "");
	strcpy(con->fake_roomname, "");
	generate_nonce(con);
	safestrncpy(con->cs_host, config.c_fqdn, sizeof con->cs_host);
	safestrncpy(con->cs_addr, "", sizeof con->cs_addr);
	con->cs_host[sizeof con->cs_host - 1] = 0;
	len = sizeof sin;
	if (!CC->is_local_socket) {
		if (!getpeername(con->client_socket, (struct sockaddr *) &sin, &len)) {
			locate_host(con->cs_host, sizeof con->cs_host,
				con->cs_addr, sizeof con->cs_addr,
				&sin.sin_addr
			);
		}
	}
	else {
		strcpy(con->cs_host, "");
	}
	con->cs_flags = 0;
	con->upload_type = UPL_FILE;
	con->dl_is_net = 0;

	con->nologin = 0;
	if ((config.c_maxsessions > 0)&&(num_sessions > config.c_maxsessions)) {
		con->nologin = 1;
	}

	lprintf(CTDL_NOTICE, "Session started.\n");

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
		cprintf("%d %s Citadel server ready.\n",
			CIT_OK, config.c_nodename);
	}
}




/*
 * This loop recognizes all server commands.
 */
void do_command_loop(void) {
	char cmdbuf[SIZ];

	time(&CC->lastcmd);
	memset(cmdbuf, 0, sizeof cmdbuf); /* Clear it, just in case */
	if (client_getln(cmdbuf, sizeof cmdbuf) < 1) {
		lprintf(CTDL_ERR, "Client disconnected: ending session.\n");
		CC->kill_me = 1;
		return;
	}

	/* Log the server command, but don't show passwords... */
	if ( (strncasecmp(cmdbuf, "PASS", 4))
	   && (strncasecmp(cmdbuf, "SETP", 4)) ) {
		lprintf(CTDL_INFO, "%s\n", cmdbuf);
	}
	else {
		lprintf(CTDL_INFO, "<password command sent>\n");
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
		   
	if (!strncasecmp(cmdbuf, "NOOP", 4)) {
		cprintf("%d%cok\n", CIT_OK, CtdlCheckExpress() );
	}
	
	else if (!strncasecmp(cmdbuf, "QNOP", 4)) {
		/* do nothing, this command returns no response */
	}

	else if (!strncasecmp(cmdbuf,"QUIT",4)) {
		cprintf("%d Goodbye.\n", CIT_OK);
		CC->kill_me = 1;
	}

	else if (!strncasecmp(cmdbuf,"ASYN",4)) {
		cmd_asyn(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"LOUT",4)) {
		if (CC->logged_in) logout(CC);
		cprintf("%d logged out.\n", CIT_OK);
	}

	else if (!strncasecmp(cmdbuf,"USER",4)) {
		cmd_user(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"PASS",4)) {
		cmd_pass(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"NEWU",4)) {
		cmd_newu(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"CREU",4)) {
		cmd_creu(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"SETP",4)) {
		cmd_setp(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"LRMS",4)) {
		cmd_lrms(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"LKRA",4)) {
		cmd_lkra(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"LKRN",4)) {
		cmd_lkrn(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"LKRO",4)) {
		cmd_lkro(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"LZRM",4)) {
		cmd_lzrm(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"LPRM",4)) {
		cmd_lprm(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"GETU",4)) {
		cmd_getu();
	}

	else if (!strncasecmp(cmdbuf,"SETU",4)) {
		cmd_setu(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"GOTO",4)) {
		cmd_goto(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"MSGS",4)) {
		cmd_msgs(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"WHOK",4)) {
		cmd_whok();
	}

	else if (!strncasecmp(cmdbuf,"RDIR",4)) {
		cmd_rdir();
	}

	else if (!strncasecmp(cmdbuf,"EUID",4)) {
		cmd_euid(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"MSG0",4)) {
		cmd_msg0(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"MSG2",4)) {
		cmd_msg2(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"MSG3",4)) {
		cmd_msg3(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"MSG4",4)) {
		cmd_msg4(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"MSGP",4)) {
		cmd_msgp(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"OPNA",4)) {
		cmd_opna(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"DLAT",4)) {
		cmd_dlat(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"INFO",4)) {
		cmd_info();
	}

	else if (!strncasecmp(cmdbuf,"SLRP",4)) {
		cmd_slrp(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"INVT",4)) {
		cmd_invt_kick(&cmdbuf[5],1);
	}

	else if (!strncasecmp(cmdbuf,"KICK",4)) {
		cmd_invt_kick(&cmdbuf[5],0);
	}

	else if (!strncasecmp(cmdbuf,"GETR",4)) {
		cmd_getr();
	}

	else if (!strncasecmp(cmdbuf,"SETR",4)) {
		cmd_setr(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"GETA",4)) {
		cmd_geta();
	}

	else if (!strncasecmp(cmdbuf,"SETA",4)) {
		cmd_seta(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"ENT0",4)) {
		cmd_ent0(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"RINF",4)) {
		cmd_rinf();
	}

	else if (!strncasecmp(cmdbuf,"DELE",4)) {
		cmd_dele(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"KILL",4)) {
		cmd_kill(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"CRE8",4)) {
		cmd_cre8(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"MOVE",4)) {
		cmd_move(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"FORG",4)) {
		cmd_forg();
	}

	else if (!strncasecmp(cmdbuf,"MESG",4)) {
		cmd_mesg(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"EMSG",4)) {
		cmd_emsg(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"GNUR",4)) {
		cmd_gnur();
	}

	else if (!strncasecmp(cmdbuf,"VALI",4)) {
		cmd_vali(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"EINF",4)) {
		cmd_einf(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"LIST",4)) {
		cmd_list(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"CHEK",4)) {
		cmd_chek();
	}

	else if (!strncasecmp(cmdbuf,"DELF",4)) {
		cmd_delf(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"MOVF",4)) {
		cmd_movf(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"NETF",4)) {
		cmd_netf(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"OPEN",4)) {
		cmd_open(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"CLOS",4)) {
		cmd_clos();
	}

	else if (!strncasecmp(cmdbuf,"UOPN",4)) {
		cmd_uopn(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"UCLS",4)) {
		cmd_ucls(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"READ",4)) {
		cmd_read(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"WRIT",4)) {
		cmd_writ(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"QUSR",4)) {
		cmd_qusr(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"ECHO",4)) {
		cmd_echo(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"OIMG",4)) {
		cmd_oimg(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"MORE",4)) {
		cmd_more();
	}

	else if (!strncasecmp(cmdbuf,"NDOP",4)) {
		cmd_ndop(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"NUOP",4)) {
		cmd_nuop(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"LFLR",4)) {
		cmd_lflr();
	}

	else if (!strncasecmp(cmdbuf,"CFLR",4)) {
		cmd_cflr(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"KFLR",4)) {
		cmd_kflr(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"EFLR",4)) {
		cmd_eflr(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"IDEN",4)) {
		cmd_iden(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"IPGM",4)) {
		cmd_ipgm(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"TERM",4)) {
		cmd_term(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"DOWN",4)) {
		cmd_down(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf,"HALT",4)) {
		cmd_halt();
	}

	else if (!strncasecmp(cmdbuf,"SCDN",4)) {
		cmd_scdn(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf, "UIMG", 4)) {
		cmd_uimg(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf, "TIME", 4)) {
		cmd_time();
	}

	else if (!strncasecmp(cmdbuf, "AGUP", 4)) {
		cmd_agup(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf, "ASUP", 4)) {
		cmd_asup(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf, "GPEX", 4)) {
		cmd_gpex(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf, "SPEX", 4)) {
		cmd_spex(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf, "CONF", 4)) {
		cmd_conf(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf, "SEEN", 4)) {
		cmd_seen(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf, "GTSN", 4)) {
		cmd_gtsn(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf, "VIEW", 4)) {
		cmd_view(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf, "ISME", 4)) {
		cmd_isme(&cmdbuf[5]);
	}

	else if (!DLoader_Exec_Cmd(cmdbuf)) {
		cprintf("%d Unrecognized or unsupported command.\n",
			ERROR + CMD_NOT_SUPPORTED);
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
