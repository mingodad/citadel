/* $Id$ */

#ifndef _SGI_SOURCE
/* needed to properly enable crypt() stuff on some systems */
#define _XOPEN_SOURCE
/* needed for str[n]casecmp() on some systems if the above is defined */
#define _XOPEN_SOURCE_EXTENDED
/* needed to enable threads on some systems if the above are defined */
#define _POSIX_C_SOURCE 199506L
#endif

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <string.h>
#include <syslog.h>
#include <limits.h>
#include <pthread.h>
#include "citadel.h"
#include "server.h"
#include "database.h"
#include "user_ops.h"
#include "sysdep_decls.h"
#include "support.h"
#include "room_ops.h"
#include "logging.h"
#include "file_ops.h"
#include "control.h"
#include "msgbase.h"
#include "config.h"
#include "dynloader.h"
#include "sysdep.h"
#include "tools.h"


/*
 * getuser()  -  retrieve named user into supplied buffer.
 *               returns 0 on success
 */
int getuser(struct usersupp *usbuf, char name[]) {

	char lowercase_name[32];
	int a;
	struct cdbdata *cdbus;

	memset(usbuf, 0, sizeof(struct usersupp));
	for (a=0; a<=strlen(name); ++a) {
		lowercase_name[a] = tolower(name[a]);
		}

	cdbus = cdb_fetch(CDB_USERSUPP, lowercase_name, strlen(lowercase_name));
	if (cdbus == NULL) {
		return(1);	/* user not found */
		}

	memcpy(usbuf, cdbus->ptr,
		( (cdbus->len > sizeof(struct usersupp)) ?
		sizeof(struct usersupp) : cdbus->len) );
	cdb_free(cdbus);
	return(0);
	}


/*
 * lgetuser()  -  same as getuser() but locks the record
 */
int lgetuser(struct usersupp *usbuf, char *name)
{
	int retcode;

	retcode = getuser(usbuf,name);
	if (retcode == 0) {
		begin_critical_section(S_USERSUPP);
		}
	return(retcode);
	}


/*
 * putuser()  -  write user buffer into the correct place on disk
 */
void putuser(struct usersupp *usbuf, char *name)
{
	char lowercase_name[32];
	int a;

	for (a=0; a<=strlen(name); ++a) {
		lowercase_name[a] = tolower(name[a]);
		}

	cdb_store(CDB_USERSUPP,
		lowercase_name, strlen(lowercase_name),
		usbuf, sizeof(struct usersupp));

	}


/*
 * lputuser()  -  same as putuser() but locks the record
 */
void lputuser(struct usersupp *usbuf, char *name) {
	putuser(usbuf,name);
	end_critical_section(S_USERSUPP);
	}

/*
 * Index-generating function used by Ctdl[Get|Set]Relationship
 */
int GenerateRelationshipIndex(	char *IndexBuf,
				long RoomID,
				long RoomGen,
				long UserID) {

	struct {
		long iRoomID;
		long iRoomGen;
		long iUserID;
		} TheIndex;

	TheIndex.iRoomID = RoomID;
	TheIndex.iRoomGen = RoomGen;
	TheIndex.iUserID = UserID;

	memcpy(IndexBuf, &TheIndex, sizeof(TheIndex));
	return(sizeof(TheIndex));
	}

/*
 * Define a relationship between a user and a room
 */
void CtdlSetRelationship(struct visit *newvisit,
			struct usersupp *rel_user,
			struct quickroom *rel_room) {

	char IndexBuf[32];
	int IndexLen;

	/* We don't use these in Citadel because they're implicit by the
	 * index, but they must be present if the database is exported.
	 */
        newvisit->v_roomnum = rel_room->QRnumber;
        newvisit->v_roomgen = rel_room->QRgen;
        newvisit->v_usernum = rel_user->usernum;

	/* Generate an index */
	IndexLen = GenerateRelationshipIndex(IndexBuf,
		rel_room->QRnumber,
		rel_room->QRgen,
		rel_user->usernum);

	/* Store the record */
	cdb_store(CDB_VISIT, IndexBuf, IndexLen,
		newvisit, sizeof(struct visit)
		);
	}

/*
 * Locate a relationship between a user and a room
 */
void CtdlGetRelationship(struct visit *vbuf,
			struct usersupp *rel_user,
			struct quickroom *rel_room) {

	char IndexBuf[32];
	int IndexLen;
	struct cdbdata *cdbvisit;

	/* Generate an index */
	IndexLen = GenerateRelationshipIndex(IndexBuf,
		rel_room->QRnumber,
		rel_room->QRgen,
		rel_user->usernum);

	/* Clear out the buffer */
	memset(vbuf, 0, sizeof(struct visit));

	cdbvisit = cdb_fetch(CDB_VISIT, IndexBuf, IndexLen);
	if (cdbvisit != NULL) {
		memcpy(vbuf, cdbvisit->ptr,
			( (cdbvisit->len > sizeof(struct visit)) ?
			sizeof(struct visit) : cdbvisit->len) );
		cdb_free(cdbvisit);
		return;
		}
	}


void MailboxName(char *buf, struct usersupp *who, char *prefix) {
	sprintf(buf, "%010ld.%s", who->usernum, prefix);
	}

	
/*
 * Is the user currently logged in an Aide?
 */
int is_aide(void) {
	if (CC->usersupp.axlevel >= 6) return(1);
	else return(0);
	}


/*
 * Is the user currently logged in an Aide *or* the room aide for this room?
 */
int is_room_aide(void) {
	if ( (CC->usersupp.axlevel >= 6)
	   || (CC->quickroom.QRroomaide == CC->usersupp.usernum) ) {
		return(1);
		}
	else {
		return(0);
		}
	}

/*
 * getuserbynumber()  -  get user by number
 *			 returns 0 if user was found
 */
int getuserbynumber(struct usersupp *usbuf, long int number)
{
	struct cdbdata *cdbus;

	cdb_rewind(CDB_USERSUPP);

	while(cdbus = cdb_next_item(CDB_USERSUPP), cdbus != NULL) {
		memset(usbuf, 0, sizeof(struct usersupp));
		memcpy(usbuf, cdbus->ptr,
			( (cdbus->len > sizeof(struct usersupp)) ?
			sizeof(struct usersupp) : cdbus->len) );
		cdb_free(cdbus);
		if (usbuf->usernum == number) {
			return(0);
			}
		}
	return(-1);
	}


/*
 * USER cmd
 */
void cmd_user(char *cmdbuf)
{
	char username[256];
	char autoname[256];
	int found_user = 0;
	struct passwd *p;
	int a;

	extract(username,cmdbuf,0);
	username[25] = 0;
	strproc(username);

	if ((CC->logged_in)) {
		cprintf("%d Already logged in.\n",ERROR);
		return;
		}

	found_user = getuser(&CC->usersupp,username);
	if (found_user != 0) {
		p = (struct passwd *)getpwnam(username);
		if (p!=NULL) {
			strcpy(autoname,p->pw_gecos);
			for (a=0; a<strlen(autoname); ++a)
				if (autoname[a]==',') autoname[a]=0;
			found_user = getuser(&CC->usersupp,autoname);
			}
		}
	if (found_user == 0) {
		if (((CC->nologin)) && (CC->usersupp.axlevel < 6)) {
			cprintf("%d %s: Too many users are already online (maximum is %d)\n",
			ERROR+MAX_SESSIONS_EXCEEDED,
			config.c_nodename,config.c_maxsessions);
			}
		else {
			strcpy(CC->curr_user,CC->usersupp.fullname);
			cprintf("%d Password required for %s\n",
				MORE_DATA,CC->curr_user);
			}
		}
	else {
		cprintf("%d %s not found.\n",ERROR,username);
		}
	}



/*
 * session startup code which is common to both cmd_pass() and cmd_newu()
 */
void session_startup(void) {
	syslog(LOG_NOTICE,"user <%s> logged in",CC->curr_user);

	lgetuser(&CC->usersupp,CC->curr_user);
	++(CC->usersupp.timescalled);
	CC->fake_username[0] = '\0';
	CC->fake_postname[0] = '\0';
	CC->fake_hostname[0] = '\0';
	CC->fake_roomname[0] = '\0';
	CC->last_pager[0] = '\0';
	time(&CC->usersupp.lastcall);

	/* If this user's name is the name of the system administrator
	 * (as specified in setup), automatically assign access level 6.
	 */
	if (!strcasecmp(CC->usersupp.fullname, config.c_sysadm)) {
		CC->usersupp.axlevel = 6;
		}

	lputuser(&CC->usersupp,CC->curr_user);

        /* Run any cleanup routines registered by loadable modules */
	PerformSessionHooks(EVT_LOGIN);

	cprintf("%d %s|%d|%d|%d|%u|%ld\n",OK,CC->usersupp.fullname,CC->usersupp.axlevel,
		CC->usersupp.timescalled,CC->usersupp.posted,CC->usersupp.flags,
		CC->usersupp.usernum);
	usergoto(BASEROOM,0);		/* Enter the lobby */	
	rec_log(CL_LOGIN,CC->curr_user);
	}


/* 
 * misc things to be taken care of when a user is logged out
 */
void logout(struct CitContext *who)
{
	who->logged_in = 0;
	if (who->download_fp != NULL) {
		fclose(who->download_fp);
		who->download_fp = NULL;
		}
	if (who->upload_fp != NULL) {
		abort_upl(who);
		}

	/* Do modular stuff... */
	PerformSessionHooks(EVT_LOGOUT);
	}


void cmd_pass(char *buf)
{
	char password[256];
	int code;
	struct passwd *p;

	extract(password,buf,0);

	if ((CC->logged_in)) {
		cprintf("%d Already logged in.\n",ERROR);
		return;
		}
	if (!strcmp(CC->curr_user,"")) {
		cprintf("%d You must send a name with USER first.\n",ERROR);
		return;
		}
	if (getuser(&CC->usersupp,CC->curr_user)) {
		cprintf("%d Can't find user record!\n",ERROR+INTERNAL_ERROR);
		return;
		}

	code = (-1);
	if (CC->usersupp.USuid == BBSUID) {
		strproc(password);
		strproc(CC->usersupp.password);
		code = strcasecmp(CC->usersupp.password,password);
		}
	else {
		p = (struct passwd *)getpwuid(CC->usersupp.USuid);
#ifdef ENABLE_AUTOLOGIN
		if (p!=NULL) {
			if (!strcmp(p->pw_passwd,
			   (char *)crypt(password,p->pw_passwd))) {
				code = 0;
				lgetuser(&CC->usersupp, CC->curr_user);
				strcpy(CC->usersupp.password, password);
				lputuser(&CC->usersupp, CC->curr_user);
				}
			}
#endif
		}

	if (!code) {
		(CC->logged_in) = 1;
		session_startup();
		}
	else {
		cprintf("%d Wrong password.\n",ERROR);
		rec_log(CL_BADPW,CC->curr_user);
		}
	}


/*
 * Delete a user record *and* all of its related resources.
 */
int purge_user(char pname[]) {
	char filename[64];
	char mailboxname[ROOMNAMELEN];
	struct usersupp usbuf;
	struct quickroom qrbuf;
	char lowercase_name[32];
	int a;
	struct CitContext *ccptr;
	int user_is_logged_in = 0;

	for (a=0; a<=strlen(pname); ++a) {
		lowercase_name[a] = tolower(pname[a]);
		}

	if (getuser(&usbuf, pname) != 0) {
		lprintf(5, "Cannot purge user <%s> - not found\n", pname);
		return(ERROR+NO_SUCH_USER);
		}

	/* Don't delete a user who is currently logged in.  Instead, just
	 * set the access level to 0, and let the account get swept up
	 * during the next purge.
	 */
	user_is_logged_in = 0;
	begin_critical_section(S_SESSION_TABLE);
	for (ccptr=ContextList; ccptr!=NULL; ccptr=ccptr->next) {
		if (ccptr->usersupp.usernum == usbuf.usernum) {
			user_is_logged_in = 1;
			}
		}
	end_critical_section(S_SESSION_TABLE);
	if (user_is_logged_in == 1) {
		lprintf(5, "User <%s> is logged in; not deleting.\n", pname);
		usbuf.axlevel = 0;
		putuser(&usbuf, pname);
		return(1);
		}

	lprintf(5, "Deleting user <%s>\n", pname);

	/* Perform any purge functions registered by server extensions */
	PerformUserHooks(usbuf.fullname, usbuf.usernum, EVT_PURGEUSER);

	/* delete any existing user/room relationships */
	cdb_delete(CDB_VISIT, &usbuf.usernum, sizeof(long));

	/* Delete the user's mailbox and its contents */
	MailboxName(mailboxname, &usbuf, MAILROOM);
	if (getroom(&qrbuf, mailboxname)==0) {
		delete_room(&qrbuf);
		}

	/* delete the userlog entry */
	cdb_delete(CDB_USERSUPP, lowercase_name, strlen(lowercase_name));

	/* remove the user's bio file */	
	sprintf(filename, "./bio/%ld", usbuf.usernum);
	unlink(filename);

	/* remove the user's picture */
	sprintf(filename, "./userpics/%ld.gif", usbuf.usernum);
	unlink(filename);

	return(0);
	}


/*
 * create_user()  -  back end processing to create a new user
 */
int create_user(char *newusername)
{
	struct usersupp usbuf;
	int a;
	struct passwd *p = NULL;
	char username[64];
	char mailboxname[ROOMNAMELEN];

	strcpy(username, newusername);
	strproc(username);

#ifdef ENABLE_AUTOLOGIN
	p = (struct passwd *)getpwnam(username);
#endif
	if (p != NULL) {
		strcpy(username, p->pw_gecos);
		for (a=0; a<strlen(username); ++a) {
			if (username[a] == ',') username[a] = 0;
			}
		CC->usersupp.USuid = p->pw_uid;
		}
	else {
		CC->usersupp.USuid = BBSUID;
		}

	if (!getuser(&usbuf,username)) {
		return(ERROR+ALREADY_EXISTS);
		}

	strcpy(CC->curr_user,username);
	strcpy(CC->usersupp.fullname,username);
	strcpy(CC->usersupp.password,"");
	(CC->logged_in) = 1;

	/* These are the default flags on new accounts */
	CC->usersupp.flags =
		US_NEEDVALID|US_LASTOLD|US_DISAPPEAR|US_PAGINATOR|US_FLOORS;

	CC->usersupp.timescalled = 0;
	CC->usersupp.posted = 0;
	CC->usersupp.axlevel = config.c_initax;
	CC->usersupp.USscreenwidth = 80;
	CC->usersupp.USscreenheight = 24;
	time(&CC->usersupp.lastcall);
	strcpy(CC->usersupp.USname, "");
	strcpy(CC->usersupp.USaddr, "");
	strcpy(CC->usersupp.UScity, "");
	strcpy(CC->usersupp.USstate, "");
	strcpy(CC->usersupp.USzip, "");
	strcpy(CC->usersupp.USphone, "");

	/* fetch a new user number */
	CC->usersupp.usernum = get_new_user_number();

	if (CC->usersupp.usernum == 1L) {
		CC->usersupp.axlevel = 6;
		}

	/* add user to userlog */
	putuser(&CC->usersupp,CC->curr_user);
	if (getuser(&CC->usersupp,CC->curr_user)) {
		return(ERROR+INTERNAL_ERROR);
		}

	/* give the user a private mailbox */
	MailboxName(mailboxname, &CC->usersupp, MAILROOM);
	create_room(mailboxname, 4, "", 0);

	rec_log(CL_NEWUSER,CC->curr_user);
	return(0);
	}




/*
 * cmd_newu()  -  create a new user account
 */
void cmd_newu(char *cmdbuf)
{
	int a;
	char username[256];

	if ((CC->logged_in)) {
		cprintf("%d Already logged in.\n",ERROR);
		return;
		}

	if ((CC->nologin)) {
		cprintf("%d %s: Too many users are already online (maximum is %d)\n",
		ERROR+MAX_SESSIONS_EXCEEDED,
		config.c_nodename,config.c_maxsessions);
		}

	extract(username,cmdbuf,0);
	username[25] = 0;
	strproc(username);

	if (strlen(username)==0) {
		cprintf("%d You must supply a user name.\n",ERROR);
		return;
		}

	a = create_user(username);
	if ((!strcasecmp(username, "bbs")) ||
	    (!strcasecmp(username, "new")) ||
	    (!strcasecmp(username, ".")))
	{
	   cprintf("%d '%s' is an invalid login name.\n", ERROR);
	   return;
	}
	if (a==ERROR+ALREADY_EXISTS) {
		cprintf("%d '%s' already exists.\n",
			ERROR+ALREADY_EXISTS,username);
		return;
		}
	else if (a==ERROR+INTERNAL_ERROR) {
		cprintf("%d Internal error - user record disappeared?\n",
			ERROR+INTERNAL_ERROR);
		return;
		}
	else if (a==0) {
		session_startup();
		}
	else {
		cprintf("%d unknown error\n",ERROR);
		}
	rec_log(CL_NEWUSER,CC->curr_user);
	}



/*
 * set password
 */
void cmd_setp(char *new_pw)
{
	if (!(CC->logged_in)) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
		}
	if (CC->usersupp.USuid != BBSUID) {
		cprintf("%d Not allowed.  Use the 'passwd' command.\n",ERROR);
		return;
		}
	strproc(new_pw);
	if (strlen(new_pw)==0) {
		cprintf("%d Password unchanged.\n",OK);
		return;
		}
	lgetuser(&CC->usersupp,CC->curr_user);
	strcpy(CC->usersupp.password,new_pw);
	lputuser(&CC->usersupp,CC->curr_user);
	cprintf("%d Password changed.\n",OK);
	rec_log(CL_PWCHANGE,CC->curr_user);
	PerformSessionHooks(EVT_SETPASS);
	}

/*
 * get user parameters
 */
void cmd_getu(void) {
	if (!(CC->logged_in)) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
		}
	getuser(&CC->usersupp,CC->curr_user);
	cprintf("%d %d|%d|%d\n",
		OK,
		CC->usersupp.USscreenwidth,
		CC->usersupp.USscreenheight,
		(CC->usersupp.flags & US_USER_SET)
		);
	}

/*
 * set user parameters
 */
void cmd_setu(char *new_parms)
{

	if (num_parms(new_parms)!=3) {
		cprintf("%d Usage error.\n",ERROR);
		return;
		}	
	if (!(CC->logged_in)) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
		}
	lgetuser(&CC->usersupp,CC->curr_user);
	CC->usersupp.USscreenwidth = extract_int(new_parms,0);
	CC->usersupp.USscreenheight = extract_int(new_parms,1);
	CC->usersupp.flags = CC->usersupp.flags & (~US_USER_SET);
	CC->usersupp.flags = CC->usersupp.flags | 
		(extract_int(new_parms,2) & US_USER_SET);
	lputuser(&CC->usersupp,CC->curr_user);
	cprintf("%d Ok\n",OK);
	}

/*
 * set last read pointer
 */
void cmd_slrp(char *new_ptr)
{
	long newlr;
	struct visit vbuf;

	if (!(CC->logged_in)) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
		}

	if (!strncasecmp(new_ptr,"highest",7)) {
		newlr = CC->quickroom.QRhighest;
		}
	else {
		newlr = atol(new_ptr);
		}

	lgetuser(&CC->usersupp, CC->curr_user);

	CtdlGetRelationship(&vbuf, &CC->usersupp, &CC->quickroom);
	vbuf.v_lastseen = newlr;
	CtdlSetRelationship(&vbuf, &CC->usersupp, &CC->quickroom);

	lputuser(&CC->usersupp, CC->curr_user);
	cprintf("%d %ld\n",OK,newlr);
	}


/*
 * INVT and KICK commands
 */
void cmd_invt_kick(char *iuser, int op)
            		/* user name */
        {		/* 1 = invite, 0 = kick out */
	struct usersupp USscratch;
	char bbb[256];
	struct visit vbuf;

	if (!(CC->logged_in)) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
		}

	if (is_room_aide()==0) {
		cprintf("%d Higher access required.\n",
			ERROR+HIGHER_ACCESS_REQUIRED);
		return;
		}

	if (lgetuser(&USscratch,iuser)!=0) {
		cprintf("%d No such user.\n",ERROR);
		return;
		}

	CtdlGetRelationship(&vbuf, &USscratch, &CC->quickroom);

	if (op==1) {
		vbuf.v_flags = vbuf.v_flags & ~V_FORGET & ~V_LOCKOUT;
		vbuf.v_flags = vbuf.v_flags | V_ACCESS;
		}

	if (op==0) {
		vbuf.v_flags = vbuf.v_flags & ~V_ACCESS;
		vbuf.v_flags = vbuf.v_flags | V_FORGET | V_LOCKOUT;
		}

	CtdlSetRelationship(&vbuf, &USscratch, &CC->quickroom);

	lputuser(&USscratch,iuser);

	/* post a message in Aide> saying what we just did */
	sprintf(bbb,"%s %s %s> by %s",
		iuser,
		((op == 1) ? "invited to" : "kicked out of"),
		CC->quickroom.QRname,
		CC->usersupp.fullname);
	aide_message(bbb);

	cprintf("%d %s %s %s.\n",
		OK, iuser,
		((op == 1) ? "invited to" : "kicked out of"),
		CC->quickroom.QRname);
	return;
	}


/*
 * forget (Zap) the current room
 */
void cmd_forg(void) {
	struct visit vbuf;

	if (!(CC->logged_in)) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
		}

	if (is_aide()) {
		cprintf("%d Aides cannot forget rooms.\n",ERROR);
		return;
		}

	lgetuser(&CC->usersupp,CC->curr_user);
	CtdlGetRelationship(&vbuf, &CC->usersupp, &CC->quickroom);

	vbuf.v_flags = vbuf.v_flags | V_FORGET;
	vbuf.v_flags = vbuf.v_flags & ~V_ACCESS;

	CtdlSetRelationship(&vbuf, &CC->usersupp, &CC->quickroom);
	lputuser(&CC->usersupp,CC->curr_user);
	cprintf("%d Ok\n",OK);
	usergoto(BASEROOM, 0);
	}

/*
 * Get Next Unregistered User
 */
void cmd_gnur(void) {
	struct cdbdata *cdbus;
	struct usersupp usbuf;

	if (!(CC->logged_in)) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
		}

	if (CC->usersupp.axlevel < 6) {
		cprintf("%d Higher access required.\n",
			ERROR+HIGHER_ACCESS_REQUIRED);
		return;
		}

	if ((CitControl.MMflags&MM_VALID)==0) {
		cprintf("%d There are no unvalidated users.\n",OK);
		return;
		}

	/* There are unvalidated users.  Traverse the usersupp database,
	 * and return the first user we find that needs validation.
	 */
	cdb_rewind(CDB_USERSUPP);
	while (cdbus = cdb_next_item(CDB_USERSUPP), cdbus != NULL) {
		memset(&usbuf, 0, sizeof(struct usersupp));
		memcpy(&usbuf, cdbus->ptr,
			( (cdbus->len > sizeof(struct usersupp)) ?
			sizeof(struct usersupp) : cdbus->len) );
		cdb_free(cdbus);
		if ((usbuf.flags & US_NEEDVALID)
		   &&(usbuf.axlevel > 0)) {
			cprintf("%d %s\n",MORE_DATA,usbuf.fullname);
			return;
			}
		} 

	/* If we get to this point, there are no more unvalidated users.
	 * Therefore we clear the "users need validation" flag.
	 */

	begin_critical_section(S_CONTROL);
	get_control();
	CitControl.MMflags = CitControl.MMflags&(~MM_VALID);
	put_control();
	end_critical_section(S_CONTROL);
	cprintf("%d *** End of registration.\n",OK);


	}


/*
 * get registration info for a user
 */
void cmd_greg(char *who)
{
	struct usersupp usbuf;
	int a,b;
	char pbuf[32];

	if (!(CC->logged_in)) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
		}

	if (!strcasecmp(who,"_SELF_")) strcpy(who,CC->curr_user);

	if ((CC->usersupp.axlevel < 6) && (strcasecmp(who,CC->curr_user))) {
		cprintf("%d Higher access required.\n",
			ERROR+HIGHER_ACCESS_REQUIRED);
		return;
		}

	if (getuser(&usbuf,who) != 0) {
		cprintf("%d '%s' not found.\n",ERROR+NO_SUCH_USER,who);
		return;
		}

	cprintf("%d %s\n",LISTING_FOLLOWS,usbuf.fullname);
	cprintf("%ld\n",usbuf.usernum);
	cprintf("%s\n",usbuf.password);
	cprintf("%s\n",usbuf.USname);
	cprintf("%s\n",usbuf.USaddr);
	cprintf("%s\n%s\n%s\n",
		usbuf.UScity,usbuf.USstate,usbuf.USzip);
	strcpy(pbuf,usbuf.USphone);
	usbuf.USphone[0]=0;
	for (a=0; a<strlen(pbuf); ++a) {
		if ((pbuf[a]>='0')&&(pbuf[a]<='9')) {
			b=strlen(usbuf.USphone);
			usbuf.USphone[b]=pbuf[a];
			usbuf.USphone[b+1]=0;
			}
		}
	while(strlen(usbuf.USphone)<10) {
		strcpy(pbuf,usbuf.USphone);
		strcpy(usbuf.USphone," ");
		strcat(usbuf.USphone,pbuf);
		}

	cprintf("(%c%c%c) %c%c%c-%c%c%c%c\n",
		usbuf.USphone[0],usbuf.USphone[1],
		usbuf.USphone[2],usbuf.USphone[3],
		usbuf.USphone[4],usbuf.USphone[5],
		usbuf.USphone[6],usbuf.USphone[7],
		usbuf.USphone[8],usbuf.USphone[9]);

	cprintf("%d\n",usbuf.axlevel);
	cprintf("%s\n",usbuf.USemail);
	cprintf("000\n");
	}

/*
 * validate a user
 */
void cmd_vali(char *v_args)
{
	char user[256];
	int newax;
	struct usersupp userbuf;

	extract(user,v_args,0);
	newax = extract_int(v_args,1);

	if (!(CC->logged_in)) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
		}

	if (CC->usersupp.axlevel < 6) {
		cprintf("%d Higher access required.\n",
			ERROR+HIGHER_ACCESS_REQUIRED);
		return;
		}

	if (lgetuser(&userbuf,user)!=0) {
		cprintf("%d '%s' not found.\n",ERROR+NO_SUCH_USER,user);
		return;
		}

	userbuf.axlevel = newax;
	userbuf.flags = (userbuf.flags & ~US_NEEDVALID);

	lputuser(&userbuf,user);

	/* If the access level was set to zero, delete the user */
	if (newax == 0) {
		if (purge_user(user)==0) {
			cprintf("%d %s Deleted.\n", OK, userbuf.fullname);
			return;
			}
		}

	cprintf("%d ok\n",OK);
	}



/* 
 *  Traverse the user file...
 */
void ForEachUser(void (*CallBack)(struct usersupp *EachUser)) {
	struct usersupp usbuf;
	struct cdbdata *cdbus;

	cdb_rewind(CDB_USERSUPP);

	while(cdbus = cdb_next_item(CDB_USERSUPP), cdbus != NULL) {
		memset(&usbuf, 0, sizeof(struct usersupp));
		memcpy(&usbuf, cdbus->ptr,
			( (cdbus->len > sizeof(struct usersupp)) ?
			sizeof(struct usersupp) : cdbus->len) );
		cdb_free(cdbus);
		(*CallBack)(&usbuf);
		}
	}


/*
 * List one user (this works with cmd_list)
 */
void ListThisUser(struct usersupp *usbuf) {
	if (usbuf->axlevel > 0) {
		if ((CC->usersupp.axlevel>=6)
		   ||((usbuf->flags&US_UNLISTED)==0)
		   ||((CC->internal_pgm))) {
			cprintf("%s|%d|%ld|%ld|%d|%d|",
				usbuf->fullname,
				usbuf->axlevel,
				usbuf->usernum,
				usbuf->lastcall,
				usbuf->timescalled,
				usbuf->posted);
			if (CC->usersupp.axlevel >= 6)
				cprintf("%s",usbuf->password);
			cprintf("\n");
			}
		}
	}

/* 
 *  List users
 */
void cmd_list(void) {
	cprintf("%d \n",LISTING_FOLLOWS);
	ForEachUser(ListThisUser);
	cprintf("000\n");
	}


/*
 * enter registration info
 */
void cmd_regi(void) {
	int a,b,c;
	char buf[256];

	char tmpname[256];
	char tmpaddr[256];
	char tmpcity[256];
	char tmpstate[256];
	char tmpzip[256];
	char tmpphone[256];
	char tmpemail[256];

	if (!(CC->logged_in)) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
		}

	strcpy(tmpname,"");
	strcpy(tmpaddr,"");
	strcpy(tmpcity,"");
	strcpy(tmpstate,"");
	strcpy(tmpzip,"");
	strcpy(tmpphone,"");
	strcpy(tmpemail,"");

	cprintf("%d Send registration...\n",SEND_LISTING);
	a=0;
	while (client_gets(buf), strcmp(buf,"000")) {
		if (a==0) strcpy(tmpname,buf);
		if (a==1) strcpy(tmpaddr,buf);
		if (a==2) strcpy(tmpcity,buf);
		if (a==3) strcpy(tmpstate,buf);
		if (a==4) {
			for (c=0; c<strlen(buf); ++c) {
				if ((buf[c]>='0')&&(buf[c]<='9')) {
					b=strlen(tmpzip);
					tmpzip[b]=buf[c];
					tmpzip[b+1]=0;
					}
				}
			}
		if (a==5) {
			for (c=0; c<strlen(buf); ++c) {
				if ((buf[c]>='0')&&(buf[c]<='9')) {
					b=strlen(tmpphone);
					tmpphone[b]=buf[c];
					tmpphone[b+1]=0;
					}
				}
			}
		if (a==6) strncpy(tmpemail,buf,31);
		++a;
		}

	tmpname[29]=0;
	tmpaddr[24]=0;
	tmpcity[14]=0;
	tmpstate[2]=0;
	tmpzip[9]=0;
	tmpphone[10]=0;
	tmpemail[31]=0;

	lgetuser(&CC->usersupp,CC->curr_user);
	strcpy(CC->usersupp.USname,tmpname);
	strcpy(CC->usersupp.USaddr,tmpaddr);
	strcpy(CC->usersupp.UScity,tmpcity);
	strcpy(CC->usersupp.USstate,tmpstate);
	strcpy(CC->usersupp.USzip,tmpzip);
	strcpy(CC->usersupp.USphone,tmpphone);
	strcpy(CC->usersupp.USemail,tmpemail);
	CC->usersupp.flags=(CC->usersupp.flags|US_REGIS|US_NEEDVALID);
	lputuser(&CC->usersupp,CC->curr_user);

	/* set global flag calling for validation */
	begin_critical_section(S_CONTROL);
	get_control();
	CitControl.MMflags = CitControl.MMflags | MM_VALID ;
	put_control();
	end_critical_section(S_CONTROL);
	}


/*
 * assorted info we need to check at login
 */
void cmd_chek(void) {
	int mail = 0;
	int regis = 0;
	int vali = 0;
	
	if (!(CC->logged_in)) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
		}

	getuser(&CC->usersupp,CC->curr_user); /* no lock is needed here */
	if ((REGISCALL!=0)&&((CC->usersupp.flags&US_REGIS)==0)) regis = 1;

	if (CC->usersupp.axlevel >= 6) {
		get_control();
		if (CitControl.MMflags&MM_VALID) vali = 1;
		}


	/* check for mail */
	mail = NewMailCount();

	cprintf("%d %d|%d|%d\n",OK,mail,regis,vali);
	}


/*
 * check to see if a user exists
 */
void cmd_qusr(char *who)
{
	struct usersupp usbuf;

	if (getuser(&usbuf,who) == 0) {
		cprintf("%d %s\n",OK,usbuf.fullname);
		}
	else {
		cprintf("%d No such user.\n",ERROR+NO_SUCH_USER);
		}
	}


/*
 * enter user bio
 */
void cmd_ebio(void) {
	char buf[256];
	FILE *fp;

	if (!(CC->logged_in)) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
		}

	sprintf(buf,"./bio/%ld",CC->usersupp.usernum);
	fp = fopen(buf,"w");
	if (fp == NULL) {
		cprintf("%d Cannot create file\n",ERROR);
		return;
		}
	cprintf("%d  \n",SEND_LISTING);
	while(client_gets(buf), strcmp(buf,"000")) {
		fprintf(fp,"%s\n",buf);
		}
	fclose(fp);
	}

/*
 * read user bio
 */
void cmd_rbio(char *cmdbuf)
{
	struct usersupp ruser;
	char buf[256];
	FILE *fp;

	extract(buf,cmdbuf,0);
	if (getuser(&ruser,buf)!=0) {
		cprintf("%d No such user.\n",ERROR+NO_SUCH_USER);
		return;
		}
	sprintf(buf,"./bio/%ld",ruser.usernum);
	
	fp = fopen(buf,"r");
	if (fp == NULL) {
		cprintf("%d %s has no bio on file.\n",
			ERROR+FILE_NOT_FOUND,ruser.fullname);
		return;
		}
	cprintf("%d  \n",LISTING_FOLLOWS);
	while (fgets(buf,256,fp)!=NULL) cprintf("%s",buf);
	fclose(fp);
	cprintf("000\n");
	}

/*
 * list of users who have entered bios
 */
void cmd_lbio(void) {
	char buf[256];
	FILE *ls;
	struct usersupp usbuf;

	ls=popen("cd ./bio; ls","r");
	if (ls==NULL) {
		cprintf("%d Cannot open listing.\n",ERROR+FILE_NOT_FOUND);
		return;
		}

	cprintf("%d\n",LISTING_FOLLOWS);
	while (fgets(buf,255,ls)!=NULL)
		if (getuserbynumber(&usbuf,atol(buf))==0)
			cprintf("%s\n",usbuf.fullname);
	pclose(ls);
	cprintf("000\n");
	}


/*
 * Administrative Get User Parameters
 */
void cmd_agup(char *cmdbuf) {
	struct usersupp usbuf;
	char requested_user[256];

	if ( (CC->internal_pgm==0)
	   && ( (CC->logged_in == 0) || (is_aide()==0) ) ) {
		cprintf("%d Higher access required.\n", 
			ERROR + HIGHER_ACCESS_REQUIRED);
		return;
		}

	extract(requested_user, cmdbuf, 0);
	if (getuser(&usbuf, requested_user) != 0) {
		cprintf("%d No such user.\n", ERROR + NO_SUCH_USER);
		return;
		}

	cprintf("%d %s|%s|%u|%d|%d|%d|%ld|%ld|%d\n", 
		OK,
		usbuf.fullname,
		usbuf.password,
		usbuf.flags,
		usbuf.timescalled,
		usbuf.posted,
		(int)usbuf.axlevel,
		usbuf.usernum,
		usbuf.lastcall,
		usbuf.USuserpurge);
	}



/*
 * Administrative Set User Parameters
 */
void cmd_asup(char *cmdbuf) {
	struct usersupp usbuf;
	char requested_user[256];
	int np;
	int newax;
	
	if ( (CC->internal_pgm==0)
	   && ( (CC->logged_in == 0) || (is_aide()==0) ) ) {
		cprintf("%d Higher access required.\n", 
			ERROR + HIGHER_ACCESS_REQUIRED);
		return;
		}

	extract(requested_user, cmdbuf, 0);
	if (lgetuser(&usbuf, requested_user) != 0) {
		cprintf("%d No such user.\n", ERROR + NO_SUCH_USER);
		return;
		}

	np = num_parms(cmdbuf);
	if (np > 1) extract(usbuf.password, cmdbuf, 1);
	if (np > 2) usbuf.flags = extract_int(cmdbuf, 2);
	if (np > 3) usbuf.timescalled = extract_int(cmdbuf, 3);
	if (np > 4) usbuf.posted = extract_int(cmdbuf, 4);
	if (np > 5) {
		newax = extract_int(cmdbuf, 5);
		if ((newax >=0) && (newax <= 6)) {
			usbuf.axlevel = extract_int(cmdbuf, 5);
			}
		}
	if (np > 7) {
		usbuf.lastcall = extract_long(cmdbuf, 7);
		}
	if (np > 8) {
		usbuf.USuserpurge = extract_int(cmdbuf, 8);
		}

	lputuser(&usbuf, requested_user);
	if (usbuf.axlevel == 0) {
		if (purge_user(requested_user)==0) {
			cprintf("%d %s deleted.\n", OK, requested_user);
			}
		}
	cprintf("%d Ok\n", OK);
	}


/*
 * Count the number of new mail messages the user has
 */
int NewMailCount() {
	int num_newmsgs = 0;
	int a;
	char mailboxname[32];
	struct quickroom mailbox;
	struct visit vbuf;

	MailboxName(mailboxname, &CC->usersupp, MAILROOM);
	if (getroom(&mailbox, mailboxname)!=0) return(0);
	CtdlGetRelationship(&vbuf, &CC->usersupp, &mailbox);

	get_msglist(&mailbox);
	for (a=0; a<CC->num_msgs; ++a) {
		if (MessageFromList(a)>0L) {
			if (MessageFromList(a) > vbuf.v_lastseen) {
				++num_newmsgs;
				}
			}
		}

	return(num_newmsgs);
	}
