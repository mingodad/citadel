/* 
 * $Id$
 *
 * Main source module for the Citadel server
 *
 */

#ifdef DLL_EXPORT
#define IN_LIBCIT
#endif

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
#include <syslog.h>
/* #include <dlfcn.h> */
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "citadel.h"
#include "server.h"
#include "dynloader.h"
#include "sysdep_decls.h"
#include "citserver.h"
#include "config.h"
#include "database.h"
#include "housekeeping.h"
#include "user_ops.h"
#include "logging.h"
#include "msgbase.h"
#include "support.h"
#include "locate_host.h"
#include "room_ops.h"
#include "file_ops.h"
#include "policy.h"
#include "control.h"
#include "tools.h"

#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif

struct CitContext *ContextList = NULL;
char *unique_session_numbers;
int ScheduledShutdown = 0;
int do_defrag = 0;

/*
 * Various things that need to be initialized at startup
 */
void master_startup(void) {
	struct timeval tv;
	
	lprintf(9, "master_startup() started\n");
	lprintf(7, "Opening databases\n");
	open_databases();

	if (do_defrag) {
		defrag_databases();
	}

	check_ref_counts();

	lprintf(7, "Creating base rooms (if necessary)\n");
	create_room(BASEROOM,		0, "", 0, 1);
	create_room(AIDEROOM,		3, "", 0, 1);
	create_room(SYSCONFIGROOM,	3, "", 0, 1);
	create_room(config.c_twitroom,	0, "", 0, 1);

	lprintf(7, "Seeding the pseudo-random number generator...\n");
	gettimeofday(&tv, NULL);
	srand(tv.tv_usec);
	lprintf(9, "master_startup() finished\n");
}



/*
 * Cleanup routine to be called when the server is shutting down.
 * WARNING: It's no longer safe to call this function to force a shutdown.
 * Instead, set time_to_die = 1.
 */
void master_cleanup(void) {
	struct CleanupFunctionHook *fcn;

	/* Run any cleanup routines registered by loadable modules */
	for (fcn = CleanupHookTable; fcn != NULL; fcn = fcn->next) {
		(*fcn->h_function_pointer)();
	}

	/* Close databases */
	lprintf(7, "Closing databases\n");
	close_databases();

	/* Do system-dependent stuff */
	sysdep_master_cleanup();

	/* Now go away. */
	lprintf(3, "citserver: exiting.\n");
	fflush(stdout); fflush(stderr);
	exit(0);
}


/*
 * Free any per-session data allocated by modules or whatever
 */
void deallocate_user_data(struct CitContext *con)
{
	struct CtdlSessData *ptr;

	begin_critical_section(S_SESSION_TABLE);
	while (con->FirstSessData != NULL) {
		lprintf(9, "Deallocating user data symbol %ld\n",
			con->FirstSessData->sym_id);
		if (con->FirstSessData->sym_data != NULL)
			phree(con->FirstSessData->sym_data);
		ptr = con->FirstSessData->next;
		phree(con->FirstSessData);
		con->FirstSessData = ptr;
	}
	end_critical_section(S_SESSION_TABLE);
}




/*
 * Terminate a session and remove its context data structure.
 */
void RemoveContext (struct CitContext *con)
{
	struct CitContext *ptr = NULL;
	struct CitContext *ToFree = NULL;

	lprintf(9, "RemoveContext() called\n");
	if (con==NULL) {
		lprintf(5, "WARNING: RemoveContext() called with NULL!\n");
		return;
		}

	/* Remove the context from the global context list.  This needs
	 * to get done FIRST to avoid concurrency problems.  It is *vitally*
	 * important to keep num_sessions accurate!!
	 */
	lprintf(7, "Removing context for session %d\n", con->cs_pid);
	begin_critical_section(S_SESSION_TABLE);
	if (ContextList == con) {
		ToFree = ContextList;
		ContextList = ContextList->next;
		--num_sessions;
		}
	else {
		for (ptr = ContextList; ptr != NULL; ptr = ptr->next) {
			if (ptr->next == con) {
				ToFree = ptr->next;
				ptr->next = ptr->next->next;
				--num_sessions;
				}
			}
		}
	end_critical_section(S_SESSION_TABLE);

	if (ToFree == NULL) {
		lprintf(9, "RemoveContext() found nothing to remove\n");
		return;
	}

	/* Run any cleanup routines registered by loadable modules.
	 * Note 1: This must occur *before* deallocate_user_data() because the
	 *         cleanup functions might touch dynamic session data.
	 * Note 2: We have to "become_session()" because the cleanup functions
	 *         might make references to "CC" assuming it's the right one.
	 */
	become_session(con);
	PerformSessionHooks(EVT_STOP);
	become_session(NULL);

	/* Now handle all of the administrivia. */
	lprintf(7, "Calling logout(%d)\n", con->cs_pid);
	logout(con);

	rec_log(CL_TERMINATE, con->curr_user);
	unlink(con->temp);
	lprintf(3, "[%3d] Session ended.\n", con->cs_pid);
	

	syslog(LOG_NOTICE,"session %d: ended", con->cs_pid);
	
	/* Deallocate any user-data attached to this session */
	deallocate_user_data(con);

	/* If the client is still connected, blow 'em away. */
	lprintf(7, "Closing socket %d\n", con->client_socket);
	close(con->client_socket);

	/* This is where we used to check for scheduled shutdowns. */

	/* Free up the memory used by this context */
	phree(con);

	lprintf(7, "Done with RemoveContext()\n");
}



/*
 * Get a dynamic symbol number for per-session user data.
 * This API call should be made only ONCE per symbol per citserver run.
 */
int CtdlGetDynamicSymbol() 
{
	static unsigned int next_symbol = SYM_MAX;
	return ++next_symbol;
}



/*
 * Return a pointer to some generic per-session user data.
 * (This function returns NULL if the requested symbol is not allocated.)
 *
 * NOTE: we use critical sections for allocating and de-allocating these,
 *       but not for locating one.
 */
void *CtdlGetUserData(unsigned long requested_sym) 
{
	struct CtdlSessData *ptr;

	for (ptr = CC->FirstSessData; ptr != NULL; ptr = ptr->next)
		if (ptr->sym_id == requested_sym)
			return(ptr->sym_data);

	lprintf(2, "ERROR! CtdlGetUserData(%ld) symbol not allocated\n",
		requested_sym);
	return NULL;
}


/*
 * Allocate some generic per-session user data.
 */
void CtdlAllocUserData(unsigned long requested_sym, size_t num_bytes)
{
	struct CtdlSessData *ptr;

	lprintf(9, "CtdlAllocUserData(%ld) called\n", requested_sym);

	/* Fail silently if the symbol is already registered. */
	for (ptr = CC->FirstSessData; ptr != NULL; ptr = ptr->next)  {
		if (ptr->sym_id == requested_sym) {
			return;
		}
	}

	/* Grab us some memory!  Dem's good eatin' !!  */
	ptr = mallok(sizeof(struct CtdlSessData));
	ptr->sym_id = requested_sym;
	ptr->sym_data = mallok(num_bytes);
	memset(ptr->sym_data, 0, num_bytes);

	begin_critical_section(S_SESSION_TABLE);
	ptr->next = CC->FirstSessData;
	CC->FirstSessData = ptr;
	end_critical_section(S_SESSION_TABLE);

	lprintf(9, "CtdlAllocUserData(%ld) finished\n", requested_sym);
}


/* 
 * Change the size of a buffer allocated with CtdlAllocUserData()
 */
void CtdlReallocUserData(unsigned long requested_sym, size_t num_bytes)
{
	struct CtdlSessData *ptr;

	for (ptr = CC->FirstSessData; ptr != NULL; ptr = ptr->next)  {
		if (ptr->sym_id == requested_sym) {
			ptr->sym_data = reallok(ptr->sym_data, num_bytes);
			return;
		}
	}

	lprintf(2, "CtdlReallocUserData() ERROR: symbol %ld not found!\n",
		requested_sym);
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
	cprintf("%s\n", config.c_bbs_city);
	cprintf("%s\n", config.c_sysadm);
	cprintf("%d\n", SERVER_TYPE);
	cprintf("%s\n", config.c_moreprompt);
	cprintf("1\n");	/* 1 = yes, this system supports floors */
	cprintf("1\n"); /* 1 = we support the extended paging options */
	cprintf("%s\n", CC->cs_nonce);
	cprintf("000\n");
}


/*
 * returns an asterisk if there are any express messages waiting,
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
   struct tm *tmp;
   
   tv = time(NULL);
   tmp = localtime(&tv);
   
   /* timezone and daylight global variables are not portable. */
#ifdef HAVE_STRUCT_TM_TM_GMTOFF
   cprintf("%d %ld|%ld|%d\n", CIT_OK, (long)tv, tmp->tm_gmtoff, tmp->tm_isdst);
#else
   cprintf("%d %ld|%ld|%d\n", CIT_OK, (long)tv, timezone, tmp->tm_isdst);
#endif
}

/*
 * Check whether two hostnames match.
 * "Realname" should be an actual name of a client that is trying to connect;
 * "testname" should be the value we are comparing it with. The idea is that we
 * want to compare with both the abbreviated and fully-qualified versions of
 * "testname;" some people define "localhost" as "localhost.foo.com," etc.
 */
static int hostnames_match(const char *realname, const char *testname) {
	struct hostent *he;
	int retval = 0;

	if (!strcasecmp(realname, testname))
		return 1;

#ifdef HAVE_NONREENTRANT_NETDB
	begin_critical_section(S_NETDB);
#endif

	if ((he = gethostbyname(testname)) != NULL)
		if (!strcasecmp(realname, he->h_name))
			retval = 1;

#ifdef HAVE_NONREENTRANT_NETDB
	end_critical_section(S_NETDB);
#endif

	return retval;
	}

/*
 * check a hostname against the public_clients file
 */
int is_public_client(char *where)
{
	char buf[SIZ];
	FILE *fp;

	lprintf(9, "Checking whether %s is a public client\n", where);

	if (hostnames_match(where, "localhost")) return(1);
	if (hostnames_match(where, config.c_fqdn)) return(1);

	fp = fopen("public_clients","r");
	if (fp == NULL) return(0);

	while (fgets(buf, sizeof buf, fp)!=NULL) {
		while (isspace((buf[strlen(buf)-1]))) 
			buf[strlen(buf)-1] = 0;
		if (hostnames_match(where,buf)) {
			fclose(fp);
			return(1);
			}
		}

	fclose(fp);
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
	char desc[SIZ];
	char from_host[SIZ];
	struct in_addr addr;
	int do_lookup = 0;

	if (num_parms(argbuf)<4) {
		cprintf("%d usage error\n",ERROR);
		return;
	}

	dev_code = extract_int(argbuf,0);
	cli_code = extract_int(argbuf,1);
	rev_level = extract_int(argbuf,2);
	extract(desc,argbuf,3);

	safestrncpy(from_host, config.c_fqdn, sizeof from_host);
	from_host[sizeof from_host - 1] = 0;
	if (num_parms(argbuf)>=5) extract(from_host,argbuf,4);

	CC->cs_clientdev = dev_code;
	CC->cs_clienttyp = cli_code;
	CC->cs_clientver = rev_level;
	safestrncpy(CC->cs_clientname, desc, sizeof CC->cs_clientname);
	CC->cs_clientname[31] = 0;

	if (strlen(from_host) > 0) {
		if (CC->is_local_socket) do_lookup = 1;
		else if (is_public_client(CC->cs_host)) do_lookup = 1;
	}

	if (do_lookup) {
		lprintf(9, "Looking up hostname '%s'\n", from_host);
		if ((addr.s_addr = inet_addr(from_host)) != -1) {
			locate_host(CC->cs_host, sizeof CC->cs_host, &addr);
		}
	   	else {
			safestrncpy(CC->cs_host, from_host, sizeof CC->cs_host);
			CC->cs_host[sizeof CC->cs_host - 1] = 0;
		}
	}

	lprintf(7, "client %d/%d/%01d.%02d (%s)\n",
		dev_code,
		cli_code,
		(rev_level / 100),
		(rev_level % 100),
		desc);

	syslog(LOG_NOTICE,"session %d: client %d/%d/%01d.%02d (%s) from %s\n",
		CC->cs_pid,
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
	char targ[SIZ];
	char buf[SIZ];
	char *dirs[2];

	extract(buf,mname,0);


	dirs[0]=mallok(64);
	dirs[1]=mallok(64);
	strcpy(dirs[0],"messages");
	strcpy(dirs[1],"help");
	mesg_locate(targ,sizeof targ,buf,2,(const char **)dirs);
	phree(dirs[0]);
	phree(dirs[1]);


	if (strlen(targ)==0) {
		cprintf("%d '%s' not found.\n",ERROR,mname);
		return;
		}

	mfp = fopen(targ,"r");
	if (mfp==NULL) {
		cprintf("%d Cannot open '%s': %s\n",
			ERROR,targ,strerror(errno));
		return;
		}
	cprintf("%d %s\n",LISTING_FOLLOWS,buf);

	while (fgets(buf, (SIZ-1), mfp)!=NULL) {
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
	char targ[SIZ];
	char buf[SIZ];
	char *dirs[2];
	int a;

	if (CtdlAccessCheck(ac_aide)) return;

	extract(buf,mname,0);
	for (a=0; a<strlen(buf); ++a) {		/* security measure */
		if (buf[a] == '/') buf[a] = '.';
		}

	dirs[0]=mallok(64);
	dirs[1]=mallok(64);
	strcpy(dirs[0],"messages");
	strcpy(dirs[1],"help");
	mesg_locate(targ,sizeof targ,buf,2,(const char**)dirs);
	phree(dirs[0]);
	phree(dirs[1]);

	if (strlen(targ)==0) {
		snprintf(targ, sizeof targ, "./help/%s", buf);
		}

	mfp = fopen(targ,"w");
	if (mfp==NULL) {
		cprintf("%d Cannot open '%s': %s\n",
			ERROR,targ,strerror(errno));
		return;
		}
	cprintf("%d %s\n", SEND_LISTING, targ);

	while (client_gets(buf), strcmp(buf, "000")) {
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

	strcpy(real_room, viewed->quickroom.QRname);
	if (viewed->quickroom.QRflags & QR_MAILBOX) {
		strcpy(real_room, &real_room[11]);
	}
	if (viewed->quickroom.QRflags & QR_PRIVATE) {
		if ( (CtdlRoomAccess(&viewed->quickroom, &viewer->usersupp)
		   & UA_KNOWN) == 0) {
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
			ERROR+HIGHER_ACCESS_REQUIRED);
		return(-1);
	}

	if (CC->usersupp.axlevel >= 6) return(0);
 	if (required_level >= ac_aide) {
		cprintf("%d This command requires Aide access.\n",
			ERROR+HIGHER_ACCESS_REQUIRED);
		return(-1);
	}

	if (is_room_aide()) return(0);
	if (required_level >= ac_room_aide) {
		cprintf("%d This command requires Aide or Room Aide access.\n",
			ERROR + HIGHER_ACCESS_REQUIRED);
		return(-1);
	}

	if (CC->logged_in) return(0);
	if (required_level >= ac_logged_in) {
		cprintf("%d Not logged in.\n", ERROR+NOT_LOGGED_IN);
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
		cprintf("%d You can't kill your own session.\n", ERROR);
		return;
	}

	lprintf(9, "Locating session to kill\n");
	begin_critical_section(S_SESSION_TABLE);
	for (ccptr = ContextList; ccptr != NULL; ccptr = ccptr->next) {
		if (session_num == ccptr->cs_pid) {
			found_it = 1;
			if ((ccptr->usersupp.usernum == CC->usersupp.usernum)
			   || (CC->usersupp.axlevel >= 6)) {
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
		cprintf("%d No such session.\n", ERROR);
	}
}





/* 
 * get the paginator prompt
 */
void cmd_more(void) {
	cprintf("%d %s\n",CIT_OK,config.c_moreprompt);
	}

/*
 * echo 
 */
void cmd_echo(char *etext)
{
	cprintf("%d %s\n",CIT_OK,etext);
	}



/* 
 * identify as internal program
 */
void cmd_ipgm(char *argbuf)
{
	int secret;

	secret = extract_int(argbuf, 0);
	if (secret == config.c_ipgm_secret) {
		CC->internal_pgm = 1;
		strcpy(CC->curr_user, "<internal program>");
		CC->cs_flags = CC->cs_flags|CS_STEALTH;
		cprintf("%d Authenticated as an internal program.\n",CIT_OK);
		}
	else {
		cprintf("%d Authentication failed.\n",ERROR);
		lprintf(3, "Warning: ipgm authentication failed.\n");
		}
	}


/*
 * Shut down the server
 */
void cmd_down(void) {

	if (CtdlAccessCheck(ac_aide)) return;

	cprintf("%d Shutting down server.  Goodbye.\n", CIT_OK);
	time_to_die = 1;
	}

/*
 * Schedule or cancel a server shutdown
 */
void cmd_scdn(char *argbuf)
{
	int new_state;

	if (CtdlAccessCheck(ac_aide)) return;

	new_state = extract_int(argbuf, 0);
	if ((new_state == 0) || (new_state == 1)) {
		ScheduledShutdown = new_state;
		}
	cprintf("%d %d\n", CIT_OK, ScheduledShutdown);
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
	int len;
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
	strcpy(con->net_node,"");
	strcpy(con->fake_username, "");
	strcpy(con->fake_postname, "");
	strcpy(con->fake_hostname, "");
	strcpy(con->fake_roomname, "");
	generate_nonce(con);
	snprintf(con->temp, sizeof con->temp, tmpnam(NULL));
	safestrncpy(con->cs_host, config.c_fqdn, sizeof con->cs_host);
	con->cs_host[sizeof con->cs_host - 1] = 0;
	len = sizeof sin;
	if (!CC->is_local_socket) {
		if (!getpeername(con->client_socket,
		   (struct sockaddr *) &sin, &len))
			locate_host(con->cs_host, sizeof con->cs_host, &sin.sin_addr);
	}
	else {
		strcpy(con->cs_host, "");
	}
	con->cs_flags = 0;
	con->upload_type = UPL_FILE;
	con->dl_is_net = 0;
	con->FirstSessData = NULL;

	con->nologin = 0;
	if ((config.c_maxsessions > 0)&&(num_sessions > config.c_maxsessions))
		con->nologin = 1;

	lprintf(3, "Session started.\n");

	/* Run any session startup routines registered by loadable modules */
	PerformSessionHooks(EVT_START);

	rec_log(CL_CONNECT, "");
}


void citproto_begin_session() {
	if (CC->nologin==1) {
		cprintf("%d %s: Too many users are already online "
			"(maximum is %d)\n",
			ERROR+MAX_SESSIONS_EXCEEDED,
			config.c_nodename, config.c_maxsessions);
		}
	else {
		cprintf("%d %s Citadel/UX server ready.\n",
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
	if (client_gets(cmdbuf) < 1) {
		lprintf(3, "Client socket is broken.  Ending session.\n");
		CC->kill_me = 1;
		return;
	}
	lprintf(5, "Citadel: %s\n", cmdbuf);

	/*
	 * Let other clients see the last command we executed, and
	 * update the idle time, but not NOOP, PEXP, or GEXP.
	 */
	if ( (strncasecmp(cmdbuf, "NOOP", 4))
	   && (strncasecmp(cmdbuf, "PEXP", 4))
	   && (strncasecmp(cmdbuf, "GEXP", 4)) ) {
		strcpy(CC->lastcmdname, "    ");
		safestrncpy(CC->lastcmdname, cmdbuf, 
			sizeof(CC->lastcmdname) );
		time(&CC->lastidle);
		}
		
	if ((strncasecmp(cmdbuf, "ENT0", 4))
	   && (strncasecmp(cmdbuf, "MESG", 4))
	   && (strncasecmp(cmdbuf, "MSGS", 4)))
	{
	   CC->cs_flags &= ~CS_POSTING;
	}
		   
	if (!strncasecmp(cmdbuf,"NOOP",4)) {
		cprintf("%d%cok\n",CIT_OK,CtdlCheckExpress());
		}

	else if (!strncasecmp(cmdbuf,"QUIT",4)) {
		cprintf("%d Goodbye.\n",CIT_OK);
		CC->kill_me = 1;
		}

	else if (!strncasecmp(cmdbuf,"ASYN",4)) {
		cmd_asyn(&cmdbuf[5]);
		}

	else if (!strncasecmp(cmdbuf,"LOUT",4)) {
		if (CC->logged_in) logout(CC);
		cprintf("%d logged out.\n",CIT_OK);
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

	else if (!strncasecmp(cmdbuf,"OPNA",4)) {
		cmd_opna(&cmdbuf[5]);
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
		cmd_list();
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
		cmd_down();
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

#ifdef DEBUG_MEMORY_LEAKS
	else if (!strncasecmp(cmdbuf, "LEAK", 4)) {
		dump_tracked();
		}
#endif

	else if (!DLoader_Exec_Cmd(cmdbuf))
		{
		   cprintf("%d Unrecognized or unsupported command.\n",
		            ERROR);
	        }

	/* Run any after-each-command outines registered by modules */
	PerformSessionHooks(EVT_CMD);
}
