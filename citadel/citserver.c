/* $Id$ */
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
#include <dlfcn.h>
#include <netdb.h>
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
#include "logging.h"
#include "msgbase.h"
#include "support.h"
#include "locate_host.h"
#include "room_ops.h"
#include "file_ops.h"
#include "dynloader.h"
#include "policy.h"
#include "control.h"
#include "tools.h"

struct CitContext *ContextList = NULL;
int ScheduledShutdown = 0;
int do_defrag = 0;

/*
 * Various things that need to be initialized at startup
 */
void master_startup(void) {
	lprintf(7, "Opening databases\n");
	open_databases();

	if (do_defrag)
		defrag_databases();

	lprintf(7, "Checking floor reference counts\n");
	check_ref_counts();

	lprintf(7, "Creating base rooms (if necessary)\n");
	create_room(BASEROOM, 0, "", 0);
	create_room(AIDEROOM, 4, "", 0);
	create_room(config.c_twitroom, 0, "", 0);
	}

/*
 * Cleanup routine to be called when the server is shutting down.
 */
void master_cleanup(void) {
	struct CleanupFunctionHook *fcn;

	/* Cancel all running sessions */
	lprintf(7, "Cancelling running sessions...\n");
	while (ContextList != NULL) {
		kill_session(ContextList->cs_pid);
		}

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
 * Gracefully terminate the session and thread.
 * (This is called as a cleanup handler by the thread library.)
 *
 * All NON-system-dependent stuff is done in this function.
 * System-dependent session/thread cleanup is in cleanup() in sysdep.c
 */
void cleanup_stuff(void *arg)
{
	lprintf(9, "cleanup_stuff() called\n");

	lprintf(7, "Calling logout(%d)\n", CC->cs_pid);
	logout(CC);

	rec_log(CL_TERMINATE,CC->curr_user);
	unlink(CC->temp);
	lprintf(3, "citserver[%3d]: ended.\n",CC->cs_pid);
	
	/* Run any cleanup routines registered by loadable modules */
	PerformSessionHooks(EVT_STOP);

	syslog(LOG_NOTICE,"session %d ended", CC->cs_pid);
	
	/* Deallocate any user-data attached to this session */
	deallocate_user_data(CC);

	/* Tell the housekeeping thread to remove the session and context.
	 * This can't be done inline because the context data gets destroyed
	 * halfway through, and the context being destroyed can't be the one
	 * doing the work.
	 */
	lprintf(7, "Calling RemoveContext(%d)\n", CC->cs_pid);
	RemoveContext(CC);
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

	begin_critical_section(S_SESSION_TABLE);
	ptr->next = CC->FirstSessData;
	CC->FirstSessData = ptr;
	end_critical_section(S_SESSION_TABLE);

	lprintf(9, "CtdlAllocUserData(%ld) finished\n", requested_sym);
}






/*
 * cmd_info()  -  tell the client about this server
 */
void cmd_info(void) {
	cprintf("%d Server info:\n",LISTING_FOLLOWS);
	cprintf("%d\n",CC->cs_pid);
	cprintf("%s\n",config.c_nodename);
	cprintf("%s\n",config.c_humannode);
	cprintf("%s\n",config.c_fqdn);
	cprintf("%s\n",CITADEL);
	cprintf("%d\n",REV_LEVEL);
	cprintf("%s\n",config.c_bbs_city);
	cprintf("%s\n",config.c_sysadm);
	cprintf("%d\n",SERVER_TYPE);
	cprintf("%s\n",config.c_moreprompt);
	cprintf("1\n");	/* 1 = yes, this system supports floors */
	cprintf("1\n"); /* 1 = we support the extended paging options */
	cprintf("000\n");
	}

void cmd_rchg(char *argbuf)
{
	char newroomname[ROOMNAMELEN];

	extract(newroomname, argbuf, 0);
	newroomname[ROOMNAMELEN] = 0;
	if (strlen(newroomname) > 0) {
		safestrncpy(CC->fake_roomname, newroomname, 
			sizeof(CC->fake_roomname) );
		CC->fake_roomname[ROOMNAMELEN - 1] = 0;
		}
	else {
		strcpy(CC->fake_roomname, "");
		}
	cprintf("%d OK\n", OK);
}

void cmd_hchg(char *newhostname)
{
   if ((newhostname) && (newhostname[0]))
   {
      memset(CC->fake_hostname, 0, 25);
      safestrncpy(CC->fake_hostname, newhostname, sizeof(CC->fake_hostname));
   }
   else
      strcpy(CC->fake_hostname, "");
   cprintf("%d OK\n",OK);
}

void cmd_uchg(char *newusername)
{
   if (CC->usersupp.axlevel < 6) 
   {
      cprintf("%d You must be an Aide to use UCHG.\n",
		ERROR+HIGHER_ACCESS_REQUIRED);
      return;
   }
   if ((newusername) && (newusername[0]))
   {
      CC->cs_flags &= ~CS_STEALTH;
      memset(CC->fake_username, 0, 32);
      if (strncasecmp(newusername, CC->curr_user, strlen(CC->curr_user)))
         safestrncpy(CC->fake_username, newusername, sizeof(CC->fake_username));
   }
   else
   {
      CC->fake_username[0] = '\0';
      CC->cs_flags |= CS_STEALTH;
   }
   cprintf("%d\n",OK);
}

/*
 * returns an asterisk if there are any express messages waiting,
 * space otherwise.
 */
char check_express(void) {
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
   
   tv = time(NULL);
   
   cprintf("%d %ld\n", OK, tv);
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
	char buf[256];
	FILE *fp;

	if (hostnames_match(where,"localhost")) return(1);
	if (hostnames_match(where,config.c_fqdn)) return(1);

	fp = fopen("public_clients","r");
	if (fp == NULL) return(0);

	while (fgets(buf,256,fp)!=NULL) {
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
	char desc[256];
	char from_host[256];
	struct in_addr addr;

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

	lprintf(9, "Looking up hostname\n");
	if ((strlen(from_host)>0) && 
	   (is_public_client(CC->cs_host))) {
		if (inet_aton(from_host, &addr))
			locate_host(CC->cs_host, &addr);
	   	else {
			safestrncpy(CC->cs_host, from_host, sizeof CC->cs_host);
			CC->cs_host[24] = 0;
			}
		}

	syslog(LOG_NOTICE,"client %d/%d/%01d.%02d (%s)\n",
		dev_code,
		cli_code,
		(rev_level / 100),
		(rev_level % 100),
		desc);
		cprintf("%d Ok\n",OK);
	}


/*
 * enter or exit "stealth mode"
 */
void cmd_stel(char *cmdbuf)
{
	int requested_mode;

	requested_mode = extract_int(cmdbuf,0);
	if (requested_mode !=0) requested_mode = 1;

	if (!CC->logged_in) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
		}

	if (CC->usersupp.axlevel < 6) {
		cprintf("%d You must be an Aide to use stealth mode.\n",
			ERROR+HIGHER_ACCESS_REQUIRED);
		return;
		}

	if (CC->cs_flags & CS_STEALTH) {
		if (requested_mode == 0)
			CC->cs_flags = CC->cs_flags-CS_STEALTH;
		}
	else {
		if (requested_mode == 1)
			CC->cs_flags = CC->cs_flags|CS_STEALTH;
		}

	cprintf("%d Ok\n",OK);
	}




/*
 * display system messages or help
 */
void cmd_mesg(char *mname)
{
	FILE *mfp;
	char targ[256];
	char buf[256];
	char *dirs[2];

	extract(buf,mname,0);


	dirs[0]=mallok(64);
	dirs[1]=mallok(64);
	strcpy(dirs[0],"messages");
	strcpy(dirs[1],"help");
	mesg_locate(targ,buf,2,dirs);
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

	while (fgets(buf,255,mfp)!=NULL) {
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

	if (CC->usersupp.axlevel < 6) {
		cprintf("%d You must be an Aide to edit system messages.\n",
			ERROR+HIGHER_ACCESS_REQUIRED);
		return;
		}

	extract(buf,mname,0);
	for (a=0; a<strlen(buf); ++a) {		/* security measure */
		if (buf[a] == '/') buf[a] = '.';
		}

	dirs[0]=mallok(64);
	dirs[1]=mallok(64);
	strcpy(dirs[0],"messages");
	strcpy(dirs[1],"help");
	mesg_locate(targ,buf,2,dirs);
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
 * who's online
 */
void cmd_rwho(void) {
	struct CitContext *cptr;
	int spoofed = 0;
	int aide;
	char un[40];
	char real_room[ROOMNAMELEN], room[ROOMNAMELEN];
	char host[40], flags[5];
	
	aide = CC->usersupp.axlevel >= 6;
	cprintf("%d%c \n", LISTING_FOLLOWS, check_express() );
	
	for (cptr = ContextList; cptr != NULL; cptr = cptr->next) 
	{
		flags[0] = '\0';
		spoofed = 0;
		
		if (cptr->cs_flags & CS_POSTING)
		   strcat(flags, "*");
		else
		   strcat(flags, ".");
		   
		if (cptr->fake_username[0])
		{
		   strcpy(un, cptr->fake_username);
		   spoofed = 1;
		}
		else
		   strcpy(un, cptr->curr_user);
		   
		if (cptr->fake_hostname[0])
		{
		   strcpy(host, cptr->fake_hostname);
		   spoofed = 1;
		}
		else
		   strcpy(host, cptr->cs_host);

		GenerateRoomDisplay(real_room, cptr, CC);

		if (cptr->fake_roomname[0]) {
			strcpy(room, cptr->fake_roomname);
			spoofed = 1;
		}
		else {
			strcpy(room, real_room);
		}
		
                if ((aide) && (spoofed))
                   strcat(flags, "+");
		
		if ((cptr->cs_flags & CS_STEALTH) && (aide))
		   strcat(flags, "-");
		
		if (((cptr->cs_flags&CS_STEALTH)==0) || (aide))
		{
			cprintf("%d|%s|%s|%s|%s|%ld|%s|%s\n",
				cptr->cs_pid, un, room,
				host, cptr->cs_clientname,
				(long)(cptr->lastidle),
				cptr->lastcmdname, flags);
		}
		if ((spoofed) && (aide))
		{
			cprintf("%d|%s|%s|%s|%s|%ld|%s|%s\n",
				cptr->cs_pid, cptr->curr_user,
				real_room,
				cptr->cs_host, cptr->cs_clientname,
				(long)(cptr->lastidle),
				cptr->lastcmdname, flags);
		
		}
	}

	/* Now it's magic time.  Before we finish, call any EVT_RWHO hooks
	 * so that external paging modules such as serv_icq can add more
	 * content to the Wholist.
	 */
	PerformSessionHooks(EVT_RWHO);
	cprintf("000\n");
	}


/*
 * Terminate another running session
 */
void cmd_term(char *cmdbuf)
{
	int session_num;
	struct CitContext *ccptr;
	int session_to_kill = 0;

	if (!CC->logged_in) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
		}

	if (CC->usersupp.axlevel < 6) {
		cprintf("%d You must be an Aide to terminate sessions.\n",
			ERROR+HIGHER_ACCESS_REQUIRED);
		return;
		}

	session_num = extract_int(cmdbuf, 0);
	if (session_num == CC->cs_pid) {
		cprintf("%d You can't kill your own session.\n", ERROR);
		return;
		}

	lprintf(9, "Locating session to kill\n");
	begin_critical_section(S_SESSION_TABLE);
	for (ccptr = ContextList; ccptr != NULL; ccptr = ccptr->next) {
		if (session_num == ccptr->cs_pid) {
			session_to_kill = ccptr->cs_pid;
			}
		}
	end_critical_section(S_SESSION_TABLE);
	lprintf(9, "session_to_kill == %d\n", session_to_kill);

	if (session_to_kill > 0) {
		lprintf(9, "calling kill_session()\n");
		kill_session(session_to_kill);
		cprintf("%d Session terminated.\n", OK);
		}
	else {
		cprintf("%d No such session.\n", ERROR);
		}
	}





/* 
 * get the paginator prompt
 */
void cmd_more(void) {
	cprintf("%d %s\n",OK,config.c_moreprompt);
	}

/*
 * echo 
 */
void cmd_echo(char *etext)
{
	cprintf("%d %s\n",OK,etext);
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
		cprintf("%d Authenticated as an internal program.\n",OK);
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
	if (!CC->logged_in) {
		cprintf("%d Not logged in.\n", ERROR+NOT_LOGGED_IN);
		return;
		}

	if (CC->usersupp.axlevel < 6) {
		cprintf("%d You must be an Aide to shut down the server.\n",
			ERROR+HIGHER_ACCESS_REQUIRED);
		return;
		}

	cprintf("%d Shutting down server.  Goodbye.\n", OK);
	master_cleanup();
	}

/*
 * Schedule or cancel a server shutdown
 */
void cmd_scdn(char *argbuf)
{
	int new_state;

	if (!CC->logged_in) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
		}

	if (CC->usersupp.axlevel < 6) {
		cprintf("%d You must be an Aide to schedule a shutdown.\n",
			ERROR+HIGHER_ACCESS_REQUIRED);
		return;
		}

	new_state = extract_int(argbuf, 0);
	if ((new_state == 0) || (new_state == 1)) {
		ScheduledShutdown = new_state;
		}
	cprintf("%d %d\n", OK, ScheduledShutdown);
	}


/*
 * main context loop
 */
void *context_loop(struct CitContext *con)
{
	char cmdbuf[256];
	int num_sessions, len;
	struct sockaddr_in sin;

	/*
	 * Wedge our way into the context table.
	 */
	InitMyContext(con);

	/* 
	 * Initialize some variables specific to our context.
	 */
	CC->logged_in = 0;
	CC->internal_pgm = 0;
	CC->download_fp = NULL;
	CC->upload_fp = NULL;
	CC->cs_pid = con->client_socket;	/* not necessarily portable */
	CC->FirstExpressMessage = NULL;
	time(&CC->lastcmd);
	time(&CC->lastidle);
	strcpy(CC->lastcmdname, "    ");
	strcpy(CC->cs_clientname, "(unknown)");
	strcpy(CC->curr_user,"(not logged in)");
	strcpy(CC->net_node,"");
	snprintf(CC->temp, sizeof CC->temp, tmpnam(NULL));
	safestrncpy(CC->cs_host, config.c_fqdn, sizeof CC->cs_host);
	CC->cs_host[sizeof CC->cs_host - 1] = 0;
	len = sizeof sin;
	if (!getpeername(CC->client_socket, (struct sockaddr *) &sin, &len))
		locate_host(CC->cs_host, &sin.sin_addr);
	CC->cs_flags = 0;
	CC->upload_type = UPL_FILE;
	CC->dl_is_net = 0;
	CC->FirstSessData = NULL;

	num_sessions = session_count();
	CC->nologin = 0;
	if ((config.c_maxsessions > 0)&&(num_sessions > config.c_maxsessions))
		CC->nologin = 1;

	if (CC->nologin==1) {
	   cprintf("%d %s: Too many users are already online (maximum is %d)\n",
		ERROR+MAX_SESSIONS_EXCEEDED,
		config.c_nodename,config.c_maxsessions);
		}
	else {
	   cprintf("%d %s Citadel/UX server ready.\n",OK,config.c_nodename);
		}

	lprintf(3, "citserver[%3d]: started.\n", CC->cs_pid);

	/* Run any session startup routines registered by loadable modules */
	PerformSessionHooks(EVT_START);

	rec_log(CL_CONNECT, "");

	do {
		time(&CC->lastcmd);
		memset(cmdbuf, 0, sizeof cmdbuf); /* Clear it, just in case */
		if (client_gets(cmdbuf) < 1) cleanup(EXIT_NULL);
		lprintf(5, "citserver[%3d]: %s\n", CC->cs_pid, cmdbuf);

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
			
		if ((strncasecmp(cmdbuf, "ENT0", 4)) && (strncasecmp(cmdbuf, "MESG", 4)) && (strncasecmp(cmdbuf, "MSGS", 4)))
		{
		   CC->cs_flags &= ~CS_POSTING;
		}
		   
/*
 * This loop recognizes all server commands.
 */

		if (!strncasecmp(cmdbuf,"NOOP",4)) {
			cprintf("%d%cok\n",OK,check_express());
			}

		else if (!strncasecmp(cmdbuf,"QUIT",4)) {
			cprintf("%d Goodbye.\n",OK);
			}

		else if (!strncasecmp(cmdbuf,"LOUT",4)) {
			if (CC->logged_in) logout(CC);
			cprintf("%d logged out.\n",OK);
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

		else if (!strncasecmp(cmdbuf,"ENT3",4)) {
			cmd_ent3(&cmdbuf[5]);
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

		else if (!strncasecmp(cmdbuf,"RWHO",4)) {
			cmd_rwho();
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

		else if (!strncasecmp(cmdbuf,"NETP",4)) {
			cmd_netp(&cmdbuf[5]);
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

		else if (!strncasecmp(cmdbuf,"EBIO",4)) {
			cmd_ebio();
			}

		else if (!strncasecmp(cmdbuf,"RBIO",4)) {
			cmd_rbio(&cmdbuf[5]);
			}

		else if (!strncasecmp(cmdbuf,"LBIO",4)) {
			cmd_lbio();
			}

		else if (!strncasecmp(cmdbuf,"STEL",4)) {
			cmd_stel(&cmdbuf[5]);
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

		else if (!strncasecmp(cmdbuf, "NSET", 4)) {
			cmd_nset(&cmdbuf[5]);
			}

		else if (!strncasecmp(cmdbuf, "UIMG", 4)) {
			cmd_uimg(&cmdbuf[5]);
			}

		else if (!strncasecmp(cmdbuf, "UCHG", 4)) {
			cmd_uchg(&cmdbuf[5]);
			}

		else if (!strncasecmp(cmdbuf, "TIME", 4)) {
			cmd_time();
			}

		else if (!strncasecmp(cmdbuf, "HCHG", 4)) {
			cmd_hchg(&cmdbuf[5]);
			}

		else if (!strncasecmp(cmdbuf, "RCHG", 4)) {
			cmd_rchg(&cmdbuf[5]);
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

		} while(strncasecmp(cmdbuf, "QUIT", 4));

	cleanup(EXIT_NORMAL);
	return(NULL);
	}
