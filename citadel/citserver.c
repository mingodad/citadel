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
#include <pthread.h>
#include <syslog.h>
#include <dlfcn.h>
#include "citadel.h"
#include "server.h"
#include "sysdep_decls.h"
#include "citserver.h"
#include "config.h"
#include "database.h"
#include "housekeeping.h"
#include "user_ops.h"
#include "logging.h"
#include "support.h"
#include "msgbase.h"
#include "locate_host.h"
#include "room_ops.h"
#include "file_ops.h"
#include "dynloader.h"
#include "policy.h"

struct CitContext *ContextList = NULL;
int ScheduledShutdown = 0;

/*
 * Various things that need to be initialized at startup
 */
void master_startup(void) {
	lprintf(7, "Opening databases\n");
	open_databases();

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
	exit(0);
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
	struct ExpressMessage *emptr;

	lprintf(9, "cleanup_stuff() called\n");

	lprintf(7, "Calling logout(%d)\n", CC->cs_pid);
	logout(CC);

	rec_log(CL_TERMINATE,CC->curr_user);
	unlink(CC->temp);
	lprintf(3, "citserver[%3d]: ended.\n",CC->cs_pid);
	
	/* Run any cleanup routines registered by loadable modules */
	PerformSessionHooks(EVT_STOP);

	syslog(LOG_NOTICE,"session %d ended", CC->cs_pid);
	
	/* Deallocate any unsent express messages */
	begin_critical_section(S_SESSION_TABLE);
	while (CC->FirstExpressMessage != NULL) {
		emptr = CC->FirstExpressMessage;
		CC->FirstExpressMessage = CC->FirstExpressMessage->next;
		free(emptr);
		}
	end_critical_section(S_SESSION_TABLE);

	/* Deallocate any message list we might have in memory */
	if (CC->msglist != NULL) free(CC->msglist);

	/* Purge any stale user/room relationships */
	PurgeStaleRelationships();

	/* Now get rid of the session and context */
	lprintf(7, "cleanup_stuff() calling RemoveContext(%d)\n", CC->cs_pid);
	RemoveContext(CC);

	/* While we still have an extra thread with no user attached to it,
	 * take the opportunity to do some housekeeping before exiting.
	 */
	do_housekeeping();
	}


/*
 * set_wtmpsupp()  -  alter the session listing
 */
void set_wtmpsupp(char *newtext)
{
	strncpy(CC->cs_room,newtext,sizeof CC->cs_room);
	CC->cs_room[sizeof CC->cs_room - 1] = 0;
	time(&CC->cs_lastupdt);

	/* Run any routines registered by loadable modules */
	PerformSessionHooks(EVT_NEWROOM);
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
	cprintf("000\n");
	}

void cmd_rchg(char *newroomname)
{
   if ((newroomname) && (newroomname[0]))
   {
      bzero(CC->fake_roomname, ROOMNAMELEN);
      strncpy(CC->fake_roomname, newroomname, ROOMNAMELEN-1);
   }
   else
      CC->fake_roomname[0] = '\0';
   cprintf("%d\n",OK);
}

void cmd_hchg(char *newhostname)
{
   if ((newhostname) && (newhostname[0]))
   {
      bzero(CC->fake_hostname, 25);
      strncpy(CC->fake_hostname, newhostname, 24);
   }
   else
      CC->fake_hostname[0] = '\0';
   cprintf("%d\n",OK);
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
      bzero(CC->fake_username, 32);
      if (strncasecmp(newusername, CC->curr_user, strlen(CC->curr_user)))
         strncpy(CC->fake_username, newusername, 31);
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
 * check a hostname against the public_clients file
 */
int is_public_client(char *where)
{
	char buf[256];
	FILE *fp;

	if (!strcasecmp(where,"localhost")) return(1);
	if (!strcasecmp(where,config.c_fqdn)) return(1);

	fp = fopen("public_clients","r");
	if (fp == NULL) return(0);

	while (fgets(buf,256,fp)!=NULL) {
		while (isspace((buf[strlen(buf)-1]))) 
			buf[strlen(buf)-1] = 0;
		if (!strcasecmp(buf,where)) {
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

	if (num_parms(argbuf)<4) {
		cprintf("%d usage error\n",ERROR);
		return;
		}

	dev_code = extract_int(argbuf,0);
	cli_code = extract_int(argbuf,1);
	rev_level = extract_int(argbuf,2);
	extract(desc,argbuf,3);

	strncpy(from_host,config.c_fqdn,sizeof from_host);
	from_host[sizeof from_host - 1] = 0;
	if (num_parms(argbuf)>=5) extract(from_host,argbuf,4);

	CC->cs_clientdev = dev_code;
	CC->cs_clienttyp = cli_code;
	CC->cs_clientver = rev_level;
	strncpy(CC->cs_clientname,desc,31);
	CC->cs_clientname[31] = 0;

	if ((strlen(from_host)>0) && 
	   (is_public_client(CC->cs_host))) {
	   	strncpy(CC->cs_host,from_host,24);
		CC->cs_host[24] = 0;
		}
	set_wtmpsupp(CC->quickroom.QRname);

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

	set_wtmpsupp(CC->quickroom.QRname);
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


	dirs[0]=malloc(64);
	dirs[1]=malloc(64);
	strcpy(dirs[0],"messages");
	strcpy(dirs[1],"help");
	mesg_locate(targ,buf,2,dirs);
	free(dirs[0]);
	free(dirs[1]);


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

	dirs[0]=malloc(64);
	dirs[1]=malloc(64);
	strcpy(dirs[0],"messages");
	strcpy(dirs[1],"help");
	mesg_locate(targ,buf,2,dirs);
	free(dirs[0]);
	free(dirs[1]);

	if (strlen(targ)==0) {
		sprintf(targ, "./help/%s", buf);
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


/*
 * who's online
 */
void cmd_rwho(void) {
	struct CitContext *cptr;
	int spoofed = 0;
	int aide;
	char un[40], room[40], host[40], flags[5];
	
	aide = CC->usersupp.axlevel >= 6;
	cprintf("%d\n",LISTING_FOLLOWS);
	
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

		if (cptr->fake_roomname[0])
		{
		   strcpy(room, cptr->fake_roomname);
		   spoofed = 1;
		}
		else
		   strcpy(room, cptr->cs_room);
		   
		
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
				cptr->cs_pid, cptr->curr_user, cptr->cs_room,
				cptr->cs_host, cptr->cs_clientname,
				(long)(cptr->lastidle),
				cptr->lastcmdname, flags);
		
		}
	}
	cprintf("000\n");
	}


/*
 * Terminate another running session
 */
void cmd_term(char *cmdbuf)
{
	int session_num;
	struct CitContext *ccptr;

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

	for (ccptr = ContextList; ccptr != NULL; ccptr = ccptr->next) {
		if (session_num == ccptr->cs_pid) {
			kill_session(ccptr->cs_pid);
			cprintf("%d Session terminated.\n", OK);
			return;
			}
		}

	cprintf("%d No such session.\n", ERROR);
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
		}
	}


/*
 * Shut down the server
 */
void cmd_down(void) {
	if (!CC->logged_in) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
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
	int session_num;

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
	CC->msglist = NULL;
	CC->num_msgs = 0;
	time(&CC->lastcmd);
	time(&CC->lastidle);
	strcpy(CC->lastcmdname, "    ");
	strcpy(CC->cs_clientname, "(unknown)");
	strcpy(CC->curr_user,"");
	strcpy(CC->net_node,"");
	sprintf(CC->temp,"/tmp/CitServer.%d.%d", getpid(), CC->cs_pid);
	strcpy(CC->cs_room, "");
	strncpy(CC->cs_host, config.c_fqdn, sizeof CC->cs_host);
	CC->cs_host[sizeof CC->cs_host - 1] = 0;
	locate_host(CC->cs_host);
	CC->cs_flags = 0;
	CC->upload_type = UPL_FILE;
	CC->dl_is_net = 0;

	session_num = session_count();
	CC->nologin = 0;
	if ((config.c_maxsessions > 0)&&(session_num >= config.c_maxsessions))
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
		if (client_gets(cmdbuf) < 1) cleanup(EXIT_NULL);
		lprintf(5, "citserver[%3d]: %s\n", CC->cs_pid, cmdbuf);

		/*
		 * Let other clients see the last command we executed, but
		 * exclude NOOP because that would be boring.
		 */
		if (strncasecmp(cmdbuf, "NOOP", 4)) {
			strcpy(CC->lastcmdname, "    ");
			strncpy(CC->lastcmdname, cmdbuf, 4);
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

		else if (!strncasecmp(cmdbuf,"GREG",4)) {
			cmd_greg(&cmdbuf[5]);
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

		else if (!strncasecmp(cmdbuf,"REGI",4)) {
			cmd_regi();
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

		else if (!DLoader_Exec_Cmd(cmdbuf))
			{
			   cprintf("%d Unrecognized or unsupported command.\n",
			            ERROR);
		        }

		} while(strncasecmp(cmdbuf, "QUIT", 4));

	cleanup(EXIT_NORMAL);
	return(NULL);
	}
