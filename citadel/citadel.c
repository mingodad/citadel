/*
 * Citadel/UX  
 *
 * citadel.c - Main source file.
 *
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <pwd.h>
#include <setjmp.h>

#include "citadel.h"
#include "axdefs.h"
#include "serv_info.h"
#include "routines.h"
#include "routines2.h"
#include "commands.h"
#include "rooms.h"
#include "messages.h"
#include "ipc.h"
#include "client_chat.h"
#include "citadel_decls.h"

struct march {
	struct march *next;
	char march_name[32];
	char march_floor;
	};

#define IFEXPERT if (userflags&US_EXPERT)
#define IFNEXPERT if ((userflags&US_EXPERT)==0)
#define IFAIDE if (axlevel>=6)
#define IFNAIDE if (axlevel<6)

struct march *march = NULL;

/* globals associated with the client program */
char temp[16];				/* Name of general temp file */
char temp2[16];				/* Name of general temp file */
char tempdir[16];			/* Name of general temp dir */
char editor_path[256];			/* path to external editor */
char printcmd[256];			/* print command */
int editor_pid = (-1);
char fullname[32];
jmp_buf nextbuf;
struct CtdlServInfo serv_info;		/* Info on the server connected */
int screenwidth;
int screenheight;
unsigned room_flags;
char room_name[32];
char ugname[ROOMNAMELEN];
long uglsn;				/* holds <u>ngoto info */
char is_mail = 0;			/* nonzero when we're in a mail room */
char axlevel = 0;			/* access level */
char is_room_aide = 0;			/* boolean flag, 1 if room aide */
int timescalled;
int posted;
unsigned userflags;
long usernum = 0L;			/* user number */
char newnow;
long highest_msg_read;			/* used for <A>bandon room cmd */
long maxmsgnum;				/* used for <G>oto */
char sigcaught = 0;
char have_xterm = 0;			/* are we running on an xterm? */
char rc_username[32];
char rc_password[32];
char rc_floor_mode;
char floor_mode;
char curr_floor = 0;			/* number of current floor */
char floorlist[128][256];		/* names of floors */
char express_msgs = 0;			/* express messages waiting! */
char last_paged[32]="";

/*
 * here is our 'clean up gracefully and exit' routine
 */
void logoff(int code)
{
	if (editor_pid>0) {		/* kill the editor if it's running */
		kill(editor_pid,SIGHUP);
		}

/* shut down the server... but not if the logoff code is 3, because
 * that means we're exiting because we already lost the server
 */
	if (code!=3) serv_puts("QUIT");

/*
 * now clean up various things
 */

	unlink(temp);
	unlink(temp2);
	nukedir(tempdir);

	/* Violently kill off any child processes if Citadel is
	 * the login shell. 
	 */
	if (getppid()==1) {
		kill(0-getpgrp(),SIGTERM);
		sleep(1);
		kill(0-getpgrp(),SIGKILL);
		}

	sttybbs(SB_RESTORE);		/* return the old terminal settings */
	exit(code);			/* exit with the proper exit code */
	}



/*
 * We handle "next" and "stop" much differently than in earlier versions.
 * The signal catching routine simply sets a flag and returns.
 */
void sighandler(int which_sig)
{
	signal(SIGINT,SIG_IGN);
	signal(SIGQUIT,SIG_IGN);
	sigcaught = which_sig;
	return;
	}


/*
 * signal catching function for hangups...
 */
void dropcarr(int signum) {
	logoff(SIGHUP);
	}



/* general purpose routines */

void formout(char *name) /* display a file */
            
	{
	char cmd[256];
	sprintf(cmd,"MESG %s",name);
	serv_puts(cmd);
	serv_gets(cmd);
	if (cmd[0]!='1') {
		printf("%s\n",&cmd[4]);
		return;
		}
	fmout(screenwidth,NULL,
		((userflags & US_PAGINATOR) ? 1 : 0),
		screenheight,1,1);
	}


void userlist(void) { 
	char buf[256];
	char fl[256];
	struct tm *tmbuf;
	time_t lc;
	int linecount = 2;

	serv_puts("LIST");
	serv_gets(buf);
	if (buf[0]!='1') {
		printf("%s\n",&buf[4]);
		return;
		}
	sigcaught = 0;
	sttybbs(SB_YES_INTR);
	printf("       User Name           Num  L  LastCall  Calls Posts\n");
	printf("------------------------- ----- - ---------- ----- -----\n");
	while (serv_gets(buf), strcmp(buf,"000")) {
		if (sigcaught == 0) {
			extract(fl,buf,0);
			printf("%-25s ",fl);
			printf("%5ld %d ",extract_long(buf,2),
				extract_int(buf,1));
			lc = extract_long(buf,3);
			tmbuf = (struct tm *)localtime(&lc);
			printf("%02d/%02d/%04d ",
				(tmbuf->tm_mon+1),
				tmbuf->tm_mday,
				(tmbuf->tm_year + 1900));
			printf("%5ld %5ld\n",extract_long(buf,4),extract_long(buf,5));

			++linecount;
			linecount = checkpagin(linecount,
				((userflags & US_PAGINATOR) ? 1 : 0),
				screenheight);

			}
		}
	sttybbs(SB_NO_INTR);
	printf("\n");
	}


/*
 * grab assorted info about the user...
 */
void load_user_info(char *params)
{
	extract(fullname,params,0);
	axlevel = extract_int(params,1);
	timescalled = extract_int(params,2);
	posted = extract_int(params,3);
	userflags = extract_int(params,4);
	usernum = extract_long(params,5);
	}


/*
 * Remove a room from the march list.  'floornum' is ignored unless
 * 'roomname' is set to _FLOOR_, in which case all rooms on the requested
 * floor will be removed from the march list.
 */
void remove_march(char *roomname, int floornum)
{
	struct march *mptr,*mptr2;

	if (march==NULL) return;

	if ( (!strucmp(march->march_name,roomname))
	 || ((!strucmp(roomname,"_FLOOR_"))&&(march->march_floor==floornum))) {
		mptr = march->next;
		free(march);
		march = mptr;
		return;
		}

	mptr2 = march;
	for (mptr=march; mptr!=NULL; mptr=mptr->next) {

		if ( (!strucmp(mptr->march_name,roomname))
	 	   || ((!strucmp(roomname,"_FLOOR_"))
			&&(mptr->march_floor==floornum))) {

			mptr2->next = mptr->next;
			free(mptr);
			mptr=mptr2;
			}
		else {
			mptr2=mptr;
			}
		}
	}

/*
 * sort the march list by floor
 */
void sort_march_list(void) {
	struct march *mlist[129];
	struct march *mptr = NULL;
	int a;

	if (march == NULL) return;

	for (a=0; a<129; ++a) mlist[a] = NULL;

	/* first, create 128 separate lists for each floor. */
	while (march != NULL) {

		a = (int)(march->march_floor);

		/* assign an illegal floor number of 128 to _BASEROOM_
		 * in order to force it to show up last */	
		if (!strucmp(march->march_name,"_BASEROOM_")) a = 128;

		mptr = march;
		march = march->next;
		mptr->next = mlist[a];
		mlist[a] = mptr;
		}

	/* now merge the lists, in order, into one big list, 
	 * except the current floor
	 */
	for (a=128; a>=0; --a) if (a != curr_floor) {
		while (mlist[a] != NULL) {
			mptr = mlist[a];
			mlist[a] = mlist[a]->next;
			mptr->next = march;
			march = mptr;
			}
		}

	/* now merge in rooms from the current floor */
	while (mlist[(int)curr_floor] != NULL) {
		mptr = mlist[(int)curr_floor];
		mlist[(int)curr_floor] = mlist[(int)curr_floor]->next;
		mptr->next = march;
		march = mptr;
		}

	}



/*
 * jump directly to a room
 */
void dotgoto(char *towhere, int display_name)
{
	char aaa[256],bbb[256],psearch[256];
	static long ls = 0L;
	int newmailcount;
	static int oldmailcount = (-1);
	int partial_match,best_match;
	char from_floor;

	/* store ungoto information */
	strcpy(ugname,room_name);
	uglsn = ls;

	/* first try an exact match */
	sprintf(aaa,"GOTO %s",towhere);
	serv_puts(aaa);
	serv_gets(aaa);
	if (aaa[3]=='*') express_msgs = 1;
	if (!strncmp(aaa,"54",2)) {
		newprompt("Enter room password: ",bbb,9);
		sprintf(aaa,"GOTO %s|%s",towhere,bbb);
		serv_puts(aaa);
		serv_gets(aaa);
		if (aaa[3]=='*') express_msgs = 1;
		}
	if (!strncmp(aaa,"54",2)) {
		printf("Wrong password.\n");
		return;
		}


	/*
	 * If a match is not found, try a partial match.
	 * Partial matches anywhere in the string carry a weight of 1,
	 * left-aligned matches carry a weight of 2.  Pick the room that
	 * has the highest-weighted match.
	 */
	if (aaa[0]!='2') {
		best_match = 0;
		strcpy(bbb,"");
		serv_puts("LKRA");
		serv_gets(aaa);
		if (aaa[0]=='1') while (serv_gets(aaa), strcmp(aaa,"000")) {
			extract(psearch,aaa,0);
			partial_match = 0;
			if (pattern(psearch,towhere)>=0) {
				partial_match = 1;
				}
			if (!struncmp(towhere,psearch,strlen(towhere))) {
				partial_match = 2;
				}
			if (partial_match > best_match) {
				strcpy(bbb,psearch);
				best_match = partial_match;
				}
			}
		if (strlen(bbb)==0) {
			printf("No room '%s'.\n",towhere);
			return;
			}
		sprintf(aaa,"GOTO %s",bbb);
		serv_puts(aaa);
		serv_gets(aaa);
		if (aaa[3]=='*') express_msgs = 1;
		}

	if (aaa[0]!='2') {
		printf("%s\n",aaa);
		return;
		}

	extract(room_name,&aaa[4],0);
	room_flags = extract_int(&aaa[4],4);
	from_floor = curr_floor;
	curr_floor = extract_int(&aaa[4],10);

	remove_march(room_name,0);
	if (!strucmp(towhere,"_BASEROOM_")) remove_march(towhere,0);
	if ((from_floor!=curr_floor) && (display_name>0) && (floor_mode==1)) {
		if (floorlist[(int)curr_floor][0]==0) load_floorlist();
		printf("(Entering floor: %s)\n",&floorlist[(int)curr_floor][0]);
		}
	if (display_name == 1) printf("%s - ",room_name);
	if (display_name != 2) printf("%d new of %d messages.\n",
		extract_int(&aaa[4],1),
		extract_int(&aaa[4],2));
	highest_msg_read = extract_int(&aaa[4],6);
	maxmsgnum = extract_int(&aaa[4],5);
	is_mail = (char) extract_int(&aaa[4],7);
	is_room_aide = (char) extract_int(&aaa[4],8);
	ls = extract_long(&aaa[4],6);

	/* read info file if necessary */
	if (extract_int(&aaa[4],3) > 0) readinfo();

	/* check for newly arrived mail if we can */
	if (num_parms(&aaa[4])>=10) {
		newmailcount = extract_int(&aaa[4],9);
		if ( (oldmailcount >= 0) && (newmailcount > oldmailcount) )
			printf("*** You have new mail\n");
		oldmailcount = newmailcount;
		}
	}

/* Goto next room having unread messages.
 * We want to skip over rooms that the user has already been to, and take the
 * user back to the lobby when done.  The room we end up in is placed in
 * newroom - which is set to 0 (the lobby) initially.
 */
void gotonext(void) {
	char buf[256];
	struct march *mptr,*mptr2;
	char next_room[32];

	/* Check to see if the march-mode list is already allocated.
	 * If it is, pop the first room off the list and go there.
	 */
	if (march==NULL) {
		serv_puts("LKRN");
		serv_gets(buf);
		if (buf[0]=='1')
		    while (serv_gets(buf), strcmp(buf,"000")) {
			mptr = (struct march *) malloc(sizeof(struct march));
			mptr->next = NULL;
			extract(mptr->march_name,buf,0);
			mptr->march_floor = (char) (extract_int(buf,2) & 0x7F);
			if (march==NULL) {
				march = mptr;
				}
			else {
				mptr2 = march;
				while (mptr2->next != NULL)
					mptr2 = mptr2->next;
				mptr2->next = mptr;
				}
			}

/* add _BASEROOM_ to the end of the march list, so the user will end up
 * in the system base room (usually the Lobby>) at the end of the loop
 */
		mptr = (struct march *) malloc(sizeof(struct march));
		mptr->next = NULL;
		strcpy(mptr->march_name,"_BASEROOM_");
		if (march==NULL) {
			march = mptr;
			}
		else {
			mptr2 = march;
			while (mptr2->next != NULL)
				mptr2 = mptr2->next;
			mptr2->next = mptr;
			}
/*
 * ...and remove the room we're currently in, so a <G>oto doesn't make us
 * walk around in circles
 */
		remove_march(room_name,0);
		}

	sort_march_list();

	if (march!=NULL) {
		strcpy(next_room,march->march_name);
		}
	else {
		strcpy(next_room,"_BASEROOM_");
		}
	remove_march(next_room,0);
	dotgoto(next_room,1);
   }

/*
 * forget all rooms on a given floor
 */
void forget_all_rooms_on(int ffloor)
{
	char buf[256];
	struct march *flist,*fptr;

	printf("Forgetting all rooms on %s...\r",&floorlist[ffloor][0]);
	fflush(stdout);
	sprintf(buf,"LKRA %d",ffloor);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0]!='1') {
		printf("%-72s\n",&buf[4]);
		return;
		}
	flist = NULL;
	while (serv_gets(buf), strcmp(buf,"000")) {
		fptr = (struct march *) malloc(sizeof(struct march));
		fptr->next = flist;
		flist = fptr;
		extract(fptr->march_name,buf,0);
		}
	while (flist != NULL) {
		sprintf(buf,"GOTO %s",flist->march_name);
		serv_puts(buf);
		serv_gets(buf);
		if (buf[0]=='2') {
			serv_puts("FORG");
			serv_gets(buf);
			}
		fptr = flist;
		flist = flist->next;
		free(fptr);
		}
	printf("%-72s\r","");
	}


/*
 * routine called by gotofloor() to move to a new room on a new floor
 */
void gf_toroom(char *towhere, int mode)
{
	int floor_being_left;

	floor_being_left = curr_floor;

	if (mode == GF_GOTO) {		/* <;G>oto mode */
		updatels();
		dotgoto(towhere,1);
		}

	if (mode == GF_SKIP) {		/* <;S>kip mode */
		dotgoto(towhere,1);
		remove_march("_FLOOR_",floor_being_left);
		}

	if (mode == GF_ZAP) {		/* <;Z>ap mode */
		dotgoto(towhere,1);
		remove_march("_FLOOR_",floor_being_left);
		forget_all_rooms_on(floor_being_left);
		}
	}


/*
 * go to a new floor
 */
void gotofloor(char *towhere, int mode)
{
	int a,tofloor;
	struct march *mptr;
	char buf[256],targ[256];

	if (floorlist[0][0]==0) load_floorlist();
	tofloor = (-1);
	for (a=0; a<128; ++a) if (!strucmp(&floorlist[a][0],towhere))
		tofloor = a;

	if (tofloor<0) {
		for (a=0; a<128; ++a) {
		    if (!struncmp(&floorlist[a][0],towhere,strlen(towhere))) {
			tofloor = a;
			}
		    }
		}
	
	if (tofloor<0) {
		for (a=0; a<128; ++a)
		    if (pattern(towhere,&floorlist[a][0])>0)
			tofloor = a;
		}
	
	if (tofloor<0) {
		printf("No floor '%s'.\n",towhere);
		return;
		}

	for (mptr = march; mptr != NULL; mptr = mptr->next) {
		if ((mptr->march_floor) == tofloor) 
			gf_toroom(mptr->march_name,mode);
			return;
		}

	strcpy(targ,"");
	sprintf(buf,"LKRA %d",tofloor);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0]=='1') while (serv_gets(buf), strcmp(buf,"000")) {
		if ((extract_int(buf,2)==tofloor)&&(strlen(targ)==0))
			 extract(targ,buf,0);
		}
	if (strlen(targ)>0) {
		gf_toroom(targ,mode);
		}
	else {
		printf("There are no rooms on '%s'.\n",&floorlist[tofloor][0]);
		}
	}


/*
 * forget all rooms on current floor
 */
void forget_this_floor(void) {
	
	if (curr_floor == 0) {
		printf("Can't forget this floor.\n");
		return;
		}

	if (floorlist[0][0]==0) load_floorlist();
	printf("Are you sure you want to forget all rooms on %s? ",
		&floorlist[(int)curr_floor][0]);
	if (yesno()==0) return;

	gf_toroom("_BASEROOM_",GF_ZAP);	
	}


/* 
 * Figure out the physical screen dimensions, if we can
 */
void check_screen_dims(void) {
#ifdef TIOCGWINSZ
	struct {
		unsigned short height;		/* rows */
		unsigned short width;		/* columns */
		unsigned short xpixels;
		unsigned short ypixels;		/* pixels */
	} xwinsz;

	if (have_xterm) {	/* dynamically size screen if on an xterm */
		if ( ioctl(0, TIOCGWINSZ, &xwinsz) == 0 ) {
			if (xwinsz.height) screenheight = (int) xwinsz.height;
			if (xwinsz.width) screenwidth = (int) xwinsz.width;
			}
		}
#endif
	}


/*
 * set floor mode depending on client, server, and user settings
 */
void set_floor_mode(void) {
	if (serv_info.serv_ok_floors == 0) {
		floor_mode = 0;		/* Don't use floors if the server */
		}			/* doesn't support them!          */
	else {
		if (rc_floor_mode == RC_NO) {	/* never use floors */
			floor_mode = 0;
			}
		if (rc_floor_mode == RC_YES) {	/* always use floors */
			floor_mode = 1;
			}
		if (rc_floor_mode == RC_DEFAULT) {	/* user choice */
			floor_mode = ((userflags & US_FLOORS) ? 1 : 0);
			}
		}
	}

/*
 * Set or change the user's password
 */
int set_password(void) {
	char pass1[20];
	char pass2[20];
	char buf[256];

	if (strlen(rc_password) > 0) {
		strcpy(pass1,rc_password);
		strcpy(pass2,rc_password);
		}
	else {
		IFNEXPERT formout("changepw");
		newprompt("Enter a new password: ", pass1, -19);
		newprompt("Enter it again to confirm: ", pass2, -19);
		}
	if (!strucmp(pass1,pass2)) {
		sprintf(buf,"SETP %s",pass1);
		serv_puts(buf);
		serv_gets(buf);
		printf("%s\n",&buf[4]);
		return(0);
		}
	else {
		printf("*** They don't match... try again.\n");
		return(1);
		}
	}



/*
 * get info about the server we've connected to
 */
void get_serv_info(void) {
	char buf[256];

	CtdlInternalGetServInfo(&serv_info);

	/* be nice and identify ourself to the server */
	sprintf(buf,"IDEN %d|%d|%d|%s|",
		SERVER_TYPE,0,REV_LEVEL,
		(server_is_local ? "local" : CITADEL));
	locate_host(&buf[strlen(buf)]);	/* append to the end */
	serv_puts(buf);
	serv_gets(buf); /* we don't care about the result code */
	}





/*
 * Display list of users currently logged on to the server
 */
void who_is_online(int longlist) 
{
	char buf[128], username[128], roomname[128], fromhost[128], flags[128];
	char tbuf[128], timestr[128], idlebuf[128], clientsoft[128];
	time_t timenow=0;
	time_t idletime, idlehours, idlemins, idlesecs;
	
	if (longlist)
	{
   	   serv_puts("TIME");
	   serv_gets(tbuf);
	   extract(timestr, tbuf, 1);
	   timenow = atol(timestr);
	}
	else {
	printf("FLG ###        User Name                 Room                 From host\n");
	printf("--- --- ------------------------- -------------------- ------------------------\n");
		}
	serv_puts("RWHO");
	serv_gets(buf);
	if (buf[0]=='1') 
	{
		while(serv_gets(buf), strcmp(buf,"000")) 
		{
			extract(username,buf,1);
			extract(roomname,buf,2);
			extract(fromhost,buf,3);
			extract(clientsoft, buf, 4);
			extract(idlebuf, buf,5);
			extract(flags,buf,7);

			if (longlist) {
				idletime = timenow - atol(idlebuf);
				idlehours = idletime / 3600;
				idlemins = (idletime - (idlehours*3600)) / 60;
				idlesecs = (idletime - (idlehours*3600) - (idlemins*60) );
				printf("\nFlags: %-3s  Sess# %-3d  Name: %-25s  Room: %s\n",
					flags, extract_int(buf,0), username, roomname);
				printf("from <%s> using <%s>, idle %ld:%02ld:%02ld\n",
					fromhost, clientsoft,
					(long)idlehours, (long)idlemins, (long)idlesecs);

				}
			else {
			printf("%-3s %-3d %-25s %-20s %-24s\n",
				flags, extract_int(buf,0), username,
				roomname, fromhost);
				}
			}
		}
	}

void enternew(char *desc, char *buf, int maxlen)
{
   char bbb[128];
   sprintf(bbb, "Enter in your new %s: ", desc);
   newprompt(bbb, buf, maxlen);
}

/*
 * main
 */
int main(int argc, char **argv)
{
int a,b,mcmd;
int termn8 = 0;
char aaa[100],bbb[100],ccc[100],eee[100];	/* general purpose variables */
char argbuf[32];				/* command line buf */


load_command_set();		/* parse the citadel.rc file */
sttybbs(SB_SAVE);		/* Store the old terminal parameters */
sttybbs(SB_NO_INTR);		/* Install the new ones */
signal(SIGINT,SIG_IGN);
signal(SIGQUIT,SIG_IGN);
signal(SIGHUP,dropcarr);	/* Cleanup gracefully if carrier is dropped */
signal(SIGTERM,dropcarr);	/* Cleanup gracefully if terminated */

send_ansi_detect();
printf("Attaching to server...\r");
fflush(stdout);
attach_to_server(argc,argv);

cls();
color(7);
serv_gets(aaa);
if (aaa[0]!='2') {
	printf("%s\n",&aaa[4]);
	logoff(atoi(aaa));
	}
get_serv_info();

printf("%-22s\n%s\n%s\n",serv_info.serv_software,serv_info.serv_humannode,
	serv_info.serv_bbs_city);
screenwidth = 80;		/* default screen dimensions */
screenheight = 24;

printf(" pause    next    stop\n");
printf(" ctrl-s  ctrl-o  ctrl-c\n\n");
formout("hello");		/* print the opening greeting */
printf("\n");

	/* if we're not the login shell, try auto-login */
if (getppid()!=1) {
	serv_puts("AUTO");
	serv_gets(aaa);
	if (aaa[0]=='2') {
		load_user_info(&aaa[4]);
		goto PWOK;
		}
	}

look_for_ansi();

GSTA:	termn8=0; newnow=0;
	do {
		if (strlen(rc_username) > 0) {
			strcpy(fullname,rc_username);
			}
		else {
			newprompt("Enter your name: ",fullname,29);
			}
		strproc(fullname); 
		if (!strucmp(fullname,"new")) {		/* just in case */
		   printf("Please enter the name you wish to log in with.\n");
		   }
		} while( 
			(!strucmp(fullname,"bbs"))
			|| (!strucmp(fullname,"new"))
			|| (strlen(fullname)==0) );

	if (!strucmp(fullname,"off")) {
		mcmd=29;
		goto TERMN8;
		}

	/* sign on to the server */
	sprintf(aaa,"USER %s",fullname);
	serv_puts(aaa);
	serv_gets(aaa);
	if (aaa[0]!='3') goto NEWUSR;

	/* password authentication */
	if (strlen(rc_password)>0) {
		strcpy(eee,rc_password);
		}
	else {
		newprompt("\rPlease enter your password: ",eee,-19);
		}
	strproc(eee);
	sprintf(aaa,"PASS %s",eee);
	serv_puts(aaa);
	serv_gets(aaa);
	if (aaa[0]=='2') {
		load_user_info(&aaa[4]);
		goto PWOK;
		}
	
	printf("<< wrong password >>\n");
	if (strlen(rc_password)>0) logoff(0);
	goto GSTA;

NEWUSR:	if (strlen(rc_password)==0) {
		printf("No record. Enter as new user? ");
		if (yesno()==0) goto GSTA;
		}

	sprintf(aaa,"NEWU %s",fullname);
	serv_puts(aaa);
	serv_gets(aaa);
	if (aaa[0]!='2') {
		printf("%s\n",aaa);
		goto GSTA;
		}
	load_user_info(&aaa[4]);

	while (set_password() != 0) ;;
	newnow=1;

	enter_config(1);
	

PWOK:	printf("%s\nAccess level: %d (%s)\nUser #%ld / Call #%d\n",
		fullname,axlevel,axdefs[(int)axlevel],
		usernum,timescalled);

	serv_puts("CHEK");
	serv_gets(aaa);
	if (aaa[0]=='2') {
		b = extract_int(&aaa[4],0);
		if (b==1) printf("*** You have a new private message in Mail>\n");
		if (b>1)  printf("*** You have %d new private messages in Mail>\n",b);

		if ((axlevel>=6) && (extract_int(&aaa[4],2)>0)) {
			printf("*** Users need validation\n");
			}

		if (extract_int(&aaa[4],1)>0) {
			printf("*** Please register.\n");
			formout("register");
			entregis();
			}
		}

	/* Make up some temporary filenames for use in various parts of the
	 * program.  Don't mess with these once they've been set, because we
	 * will be unlinking them later on in the program and we don't
	 * want to delete something that we didn't create. */
	sprintf(temp,"/tmp/citA%d",getpid());
	sprintf(temp2,"/tmp/citB%d",getpid());
	sprintf(tempdir,"/tmp/citC%d",getpid());

	/* Get screen dimensions.  First we go to a default of 80x24.  Then
	 * we try to get the user's actual screen dimensions off the server.
	 * However, if we're running on an xterm, all this stuff is
	 * irrelevant because we're going to dynamically size the screen
	 * during the session.
	 */
	screenwidth = 80;
	screenheight = 24;
	serv_puts("GETU");
	serv_gets(aaa);
	if (aaa[0]=='2') {
		screenwidth = extract_int(&aaa[4],0);
		screenheight = extract_int(&aaa[4],1);
		}
	if (getenv("TERM")!=NULL) if (!strcmp(getenv("TERM"),"xterm")) {
		have_xterm = 1;
		}
#ifdef TIOCGWINSZ
	check_screen_dims();
#endif

	set_floor_mode();


	/* Enter the lobby */
	dotgoto("_BASEROOM_",1);

/* Main loop for the system... user is logged in. */
	strcpy(ugname,"");
	uglsn = 0L;

	if (newnow==1)	readmsgs(3,1,5);
		else	readmsgs(1,1,0);

do {	/* MAIN LOOP OF PROGRAM */
	signal(SIGINT,SIG_IGN);
	signal(SIGQUIT,SIG_IGN);
	mcmd=getcmd(argbuf);

#ifdef TIOCGWINSZ
	check_screen_dims();
#endif

	if (termn8==0) switch(mcmd) {
	   case 1:	formout("help");
			break;
	   case 4:	entmsg(0,0);
			break;
	   case 36:	entmsg(0,1);
			break;
	   case 46:	entmsg(0,2);
			break;
	   case 78:	newprompt("What do you want your username to be? ", aaa, 32);
	   		sprintf(bbb, "ENT0 2|0|0|0|%s", aaa);
	   		serv_puts(bbb);
	   		serv_gets(aaa);
	   		if (strncmp("200", aaa, 3))
	   		   printf("\n%s\n", aaa);
	   		else
	   		   entmsg(0, 0);
			break;
	   case 5:	updatels();
			gotonext();
			break;
	   case 47:	updatelsa();
			gotonext();
			break;
	   case 58:	updatelsa();
			dotgoto("_MAIL_",1);
			break;
	   case 20:	updatels();
	   case 52:	dotgoto(argbuf,0);
			break;
	   case 10:	readmsgs(0,1,0);
			break;
	   case 9:	readmsgs(3,1,5);
			break;
	   case 13:	readmsgs(1,1,0);
			break;
	   case 11:	readmsgs(0,(-1),0);
			break;
	   case 12:	readmsgs(2,(-1),0);
			break;
	   case 71:	readmsgs(3, 1, atoi(argbuf));
			break;
	   case 7:	forget();	break;
	   case 18:	subshell();	break;
	   case 38:	updatels();
			entroom();	break;
	   case 22:	killroom();	break;
	   case 32:	userlist();	break;
	   case 27:	invite();	break;
	   case 28:	kickout();	break;
	   case 23:	editthisroom();	break;
	   case 14:	roomdir();	break;
	   case 33:	download(0);	break;
	   case 34:	download(1);	break;
	   case 31:	download(2);	break;
	   case 43:	download(3);	break;
	   case 45:	download(4);	break;
	   case 55:	download(5);	break;
	   case 39:	upload(0);	break;
	   case 40:	upload(1);	break;
	   case 42:	upload(2);	break;
	   case 44:	upload(3);	break;
	   case 57:	cli_upload();	break;
	   case 16:	ungoto();	break;
	   case 24:	whoknows();	break;
	   case 26:	validate();	break;
	   case 29:	updatels();
			termn8=1;
			break;
	   case 30:	updatels();
			termn8=1;
			break;
	   case 48:	enterinfo();
			break;
	   case 49:	readinfo();
			break;
	   case 72:	cli_image_upload("_userpic_");
			break;
	   case 73:	cli_image_upload("_roompic_");
			break;

case 74:
	sprintf(aaa, "_floorpic_|%d", curr_floor);
	cli_image_upload(aaa);
	break;
	
case 75:
	enternew("roomname", aaa, 20);
	sprintf(bbb, "RCHG %s", aaa);
	serv_puts(bbb);
	serv_gets(aaa);
	if (strncmp("200",aaa, 3))
	   printf("\n%s\n", aaa);
	break;
case 76:
	enternew("hostname", aaa, 25);
	sprintf(bbb, "HCHG %s", aaa);
	serv_puts(bbb);
	serv_gets(aaa);
	if (strncmp("200",aaa, 3))
	   printf("\n%s\n", aaa);
	break;
case 77:
	enternew("username", aaa, 32);
	sprintf(bbb, "UCHG %s", aaa);
	serv_puts(bbb);
	serv_gets(aaa);
	if (strncmp("200",aaa, 3))
	   printf("\n%s\n", aaa);
	break;

case 35:
	set_password();
	break;

case 21:
	if (argbuf[0]==0) strcpy(aaa,"?");
	display_help(argbuf);
	break;

case 41:
	formout("register");
	entregis();
	break;

case 15:
	printf("Are you sure (y/n)? ");
	if (yesno()==1) {
		updatels();
		a=0;
		termn8=1;
		}
	break;

case 6:
	gotonext();
	break;

case 3:	chatmode();
	break;

case 2: if (server_is_local) {
		sttybbs(SB_RESTORE);
sprintf(aaa,"USERNAME=\042%s\042; export USERNAME; exec ./subsystem %ld %d %d",
			fullname,
			usernum,screenwidth,axlevel);
		ka_system(aaa);
		sttybbs(SB_NO_INTR);
		}
	else {
		printf("*** Can't run doors when server is not local.\n");
		}
	break;

case 17:
	who_is_online(0);
	break;

case 79:
	who_is_online(1);
	break;

case 50:
	enter_config(2);
	break;

case 37:
	enter_config(0);
	set_floor_mode();
	break;

case 59:
	enter_config(3);
	set_floor_mode();
	break;

case 60:
	gotofloor(argbuf,GF_GOTO);
	break;
	
case 61:
	gotofloor(argbuf,GF_SKIP);
	break;
	
case 62:
	forget_this_floor();
	break;

case 63:
	create_floor();
	break;

case 64:
	edit_floor();
	break;

case 65:
	kill_floor();
	break;

case 66:
	enter_bio();
	break;

case 67:
	read_bio();
	break;

case 25:
	edituser();
	break;

case 8:
	knrooms(floor_mode);
	printf("\n");
	break;

case 68:
	knrooms(2);
	printf("\n");
	break;

case 69:
	misc_server_cmd(argbuf);
	break;

case 70:
	edit_system_message(argbuf);
	break;

case 19:
	listzrooms();
	printf("\n");
	break;

case 51:
	deletefile();
	break;

case 53:
	netsendfile();
	break;

case 54:
	movefile();
	break;

case 56:
        if (last_paged[0])
           sprintf(bbb, "Page who [%s]? ", last_paged);
        else
           sprintf(bbb, "Page who? ");
	newprompt(bbb,aaa,30);
	if (!aaa[0])
	   strcpy(aaa, last_paged);
	strproc(aaa);
	newprompt("Message:  ",bbb,69);
	sprintf(ccc,"SEXP %s|%s",aaa,bbb);
	serv_puts(ccc);
	serv_gets(ccc);
	if (!strncmp("200", ccc, 3))
	   strcpy(last_paged, aaa);
	printf("%s\n",&ccc[4]);
	break;

	} /* end switch */
    } while(termn8==0);

TERMN8:	printf("%s logged out.\n",fullname);
	while (march!=NULL) remove_march(march->march_name,0);
	if (mcmd==30)
		printf("\n\nType 'off' to hang up, or next user...\n");
	sprintf(aaa,"LOUT");
	serv_puts(aaa);
	serv_gets(aaa);
	if ((mcmd==29)||(mcmd==15)) {
		formout("goodbye");
		logoff(0);
		}
	goto GSTA;

} /* end main() */

