/* 
 * $Id$
 *
 * Server functions which perform operations on user objects.
 *
 */

#ifdef DLL_EXPORT
#define IN_LIBCIT
#endif

#include "sysdep.h"
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/wait.h>

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

#include <string.h>
#include <syslog.h>
#include <limits.h>
#ifndef ENABLE_CHKPWD
#include "auth.h"
#endif
#include "citadel.h"
#include "server.h"
#include "database.h"
#include "user_ops.h"
#include "dynloader.h"
#include "sysdep_decls.h"
#include "support.h"
#include "room_ops.h"
#include "logging.h"
#include "file_ops.h"
#include "control.h"
#include "msgbase.h"
#include "config.h"
#include "tools.h"
#include "citserver.h"


/*
 * getuser()  -  retrieve named user into supplied buffer.
 *               returns 0 on success
 */
int getuser(struct usersupp *usbuf, char name[])
{

	char lowercase_name[32];
	int a;
	struct cdbdata *cdbus;

	memset(usbuf, 0, sizeof(struct usersupp));
	for (a = 0; a <= strlen(name); ++a) {
		if (a < sizeof(lowercase_name))
			lowercase_name[a] = tolower(name[a]);
	}
	lowercase_name[sizeof(lowercase_name) - 1] = 0;

	cdbus = cdb_fetch(CDB_USERSUPP, lowercase_name, strlen(lowercase_name));
	if (cdbus == NULL) {
		return (1);	/* user not found */
	}
	memcpy(usbuf, cdbus->ptr,
	       ((cdbus->len > sizeof(struct usersupp)) ?
		sizeof(struct usersupp) : cdbus->len));
	cdb_free(cdbus);

	return (0);
}


/*
 * lgetuser()  -  same as getuser() but locks the record
 */
int lgetuser(struct usersupp *usbuf, char *name)
{
	int retcode;

	retcode = getuser(usbuf, name);
	if (retcode == 0) {
		begin_critical_section(S_USERSUPP);
	}
	return (retcode);
}


/*
 * putuser()  -  write user buffer into the correct place on disk
 */
void putuser(struct usersupp *usbuf)
{
	char lowercase_name[32];
	int a;

	for (a = 0; a <= strlen(usbuf->fullname); ++a) {
		if (a < sizeof(lowercase_name))
			lowercase_name[a] = tolower(usbuf->fullname[a]);
	}
	lowercase_name[sizeof(lowercase_name) - 1] = 0;

	usbuf->version = REV_LEVEL;
	cdb_store(CDB_USERSUPP,
		  lowercase_name, strlen(lowercase_name),
		  usbuf, sizeof(struct usersupp));

}


/*
 * lputuser()  -  same as putuser() but locks the record
 */
void lputuser(struct usersupp *usbuf)
{
	putuser(usbuf);
	end_critical_section(S_USERSUPP);
}

/*
 * Index-generating function used by Ctdl[Get|Set]Relationship
 */
int GenerateRelationshipIndex(char *IndexBuf,
			      long RoomID,
			      long RoomGen,
			      long UserID)
{

	struct {
		long iRoomID;
		long iRoomGen;
		long iUserID;
	} TheIndex;

	TheIndex.iRoomID = RoomID;
	TheIndex.iRoomGen = RoomGen;
	TheIndex.iUserID = UserID;

	memcpy(IndexBuf, &TheIndex, sizeof(TheIndex));
	return (sizeof(TheIndex));
}



/*
 * Back end for CtdlSetRelationship()
 */
void put_visit(struct visit *newvisit)
{
	char IndexBuf[32];
	int IndexLen;

	/* Generate an index */
	IndexLen = GenerateRelationshipIndex(IndexBuf,
					     newvisit->v_roomnum,
					     newvisit->v_roomgen,
					     newvisit->v_usernum);

	/* Store the record */
	cdb_store(CDB_VISIT, IndexBuf, IndexLen,
		  newvisit, sizeof(struct visit)
	);
}




/*
 * Define a relationship between a user and a room
 */
void CtdlSetRelationship(struct visit *newvisit,
			 struct usersupp *rel_user,
			 struct quickroom *rel_room)
{


	/* We don't use these in Citadel because they're implicit by the
	 * index, but they must be present if the database is exported.
	 */
	newvisit->v_roomnum = rel_room->QRnumber;
	newvisit->v_roomgen = rel_room->QRgen;
	newvisit->v_usernum = rel_user->usernum;

	put_visit(newvisit);
}

/*
 * Locate a relationship between a user and a room
 */
void CtdlGetRelationship(struct visit *vbuf,
			 struct usersupp *rel_user,
			 struct quickroom *rel_room)
{

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
		       ((cdbvisit->len > sizeof(struct visit)) ?
			sizeof(struct visit) : cdbvisit->len));
		cdb_free(cdbvisit);
	}
	else {
		/* If this is the first time the user has seen this room,
		 * set the view to be the default for the room.
		 */
		vbuf->v_view = rel_room->QRdefaultview;
	}

	/* Set v_seen if necessary */
	if (vbuf->v_seen[0] == 0) {
		snprintf(vbuf->v_seen, sizeof vbuf->v_seen, "*:%ld", vbuf->v_lastseen);
	}
}


void MailboxName(char *buf, size_t n, const struct usersupp *who, const char *prefix)
{
	snprintf(buf, n, "%010ld.%s", who->usernum, prefix);
}


/*
 * Is the user currently logged in an Aide?
 */
int is_aide(void)
{
	if (CC->usersupp.axlevel >= 6)
		return (1);
	else
		return (0);
}


/*
 * Is the user currently logged in an Aide *or* the room aide for this room?
 */
int is_room_aide(void)
{

	if (!CC->logged_in) {
		return (0);
	}

	if ((CC->usersupp.axlevel >= 6)
	    || (CC->quickroom.QRroomaide == CC->usersupp.usernum)) {
		return (1);
	} else {
		return (0);
	}
}

/*
 * getuserbynumber()  -  get user by number
 *                       returns 0 if user was found
 *
 * WARNING: don't use this function unless you absolutely have to.  It does
 *          a sequential search and therefore is computationally expensive.
 */
int getuserbynumber(struct usersupp *usbuf, long int number)
{
	struct cdbdata *cdbus;

	cdb_rewind(CDB_USERSUPP);

	while (cdbus = cdb_next_item(CDB_USERSUPP), cdbus != NULL) {
		memset(usbuf, 0, sizeof(struct usersupp));
		memcpy(usbuf, cdbus->ptr,
		       ((cdbus->len > sizeof(struct usersupp)) ?
			sizeof(struct usersupp) : cdbus->len));
		cdb_free(cdbus);
		if (usbuf->usernum == number) {
			cdb_close_cursor(CDB_USERSUPP);
			return (0);
		}
	}
	return (-1);
}


/*
 * Back end for cmd_user() and its ilk
 */
int CtdlLoginExistingUser(char *trythisname)
{
	char username[SIZ];
	char autoname[SIZ];
	int found_user = 0;
	struct passwd *p;
	int a;

	if (trythisname == NULL) return login_not_found;
	safestrncpy(username, trythisname, sizeof username);
	strproc(username);

	if ((CC->logged_in)) {
		return login_already_logged_in;
	}
	found_user = getuser(&CC->usersupp, username);
	if (found_user != 0) {
		p = (struct passwd *) getpwnam(username);
		if (p != NULL) {
			strcpy(autoname, p->pw_gecos);
			for (a = 0; a < strlen(autoname); ++a)
				if (autoname[a] == ',')
					autoname[a] = 0;
			found_user = getuser(&CC->usersupp, autoname);
		}
	}
	if (found_user == 0) {
		if (((CC->nologin)) && (CC->usersupp.axlevel < 6)) {
			return login_too_many_users;
		} else {
			strcpy(CC->curr_user, CC->usersupp.fullname);
			return login_ok;
		}
	}
	return login_not_found;
}



/*
 * USER cmd
 */
void cmd_user(char *cmdbuf)
{
	char username[SIZ];
	int a;

	extract(username, cmdbuf, 0);
	username[25] = 0;
	strproc(username);

	a = CtdlLoginExistingUser(username);
	switch (a) {
	case login_already_logged_in:
		cprintf("%d Already logged in.\n", ERROR);
		return;
	case login_too_many_users:
		cprintf("%d %s: "
			"Too many users are already online "
			"(maximum is %d)\n",
			ERROR + MAX_SESSIONS_EXCEEDED,
			config.c_nodename, config.c_maxsessions);
		return;
	case login_ok:
		cprintf("%d Password required for %s\n",
			MORE_DATA, CC->curr_user);
		return;
	case login_not_found:
		cprintf("%d %s not found.\n", ERROR, username);
		return;
		cprintf("%d Internal error\n", ERROR);
	}
}



/*
 * session startup code which is common to both cmd_pass() and cmd_newu()
 */
void session_startup(void)
{
	syslog(LOG_NOTICE, "session %d: user <%s> logged in",
	       CC->cs_pid, CC->curr_user);

	lgetuser(&CC->usersupp, CC->curr_user);
	++(CC->usersupp.timescalled);
	time(&CC->usersupp.lastcall);

	/* If this user's name is the name of the system administrator
	 * (as specified in setup), automatically assign access level 6.
	 */
	if (!strcasecmp(CC->usersupp.fullname, config.c_sysadm)) {
		CC->usersupp.axlevel = 6;
	}
	lputuser(&CC->usersupp);

	/* Run any startup routines registered by loadable modules */
	PerformSessionHooks(EVT_LOGIN);

	/* Create any personal rooms required by the system.
	 * (Technically, MAILROOM should be there already, but just in case...)
	 */
	create_room(MAILROOM, 4, "", 0, 1, 0);
	create_room(SENTITEMS, 4, "", 0, 1, 0);

	/* Enter the lobby */
	usergoto(config.c_baseroom, 0, 0, NULL, NULL);

	/* Record this login in the Citadel log */
	rec_log(CL_LOGIN, CC->curr_user);
}


void logged_in_response(void)
{
	cprintf("%d %s|%d|%ld|%ld|%u|%ld|%ld\n",
		CIT_OK, CC->usersupp.fullname, CC->usersupp.axlevel,
		CC->usersupp.timescalled, CC->usersupp.posted,
		CC->usersupp.flags, CC->usersupp.usernum,
		CC->usersupp.lastcall);
}



/* 
 * misc things to be taken care of when a user is logged out
 */
void logout(struct CitContext *who)
{
	who->logged_in = 0;

	/*
	 * If there is a download in progress, abort it.
	 */
	if (who->download_fp != NULL) {
		fclose(who->download_fp);
		who->download_fp = NULL;
	}

	/*
	 * If there is an upload in progress, abort it.
	 */
	if (who->upload_fp != NULL) {
		abort_upl(who);
	}

	/*
	 * If we were talking to a network node, we're not anymore...
	 */
	if (strlen(who->net_node) > 0) {
		network_talking_to(who->net_node, NTT_REMOVE);
	}

	/*
	 * Yes, we really need to free EVERY LAST BYTE we allocated.
	 */
	if (who->cs_inet_email != NULL) {
		phree(who->cs_inet_email);
		who->cs_inet_email = NULL;
	}

	/* Do modular stuff... */
	PerformSessionHooks(EVT_LOGOUT);
}

#ifdef ENABLE_CHKPWD
/*
 * an alternate version of validpw() which executes `chkpwd' instead of
 * verifying the password directly
 */
static int validpw(uid_t uid, const char *pass)
{
	pid_t pid;
	int status, pipev[2];
	char buf[24];

	if (pipe(pipev)) {
		lprintf(1, "pipe failed (%s): denying autologin access for "
			"uid %ld\n", strerror(errno), (long)uid);
		return 0;
	}
	switch (pid = fork()) {
	case -1:
		lprintf(1, "fork failed (%s): denying autologin access for "
			"uid %ld\n", strerror(errno), (long)uid);
		close(pipev[0]);
		close(pipev[1]);
		return 0;

	case 0:
		close(pipev[1]);
		if (dup2(pipev[0], 0) == -1) {
			perror("dup2");
			exit(1);
		}
		close(pipev[0]);

		execl(BBSDIR "/chkpwd", BBSDIR "/chkpwd", NULL);
		perror(BBSDIR "/chkpwd");
		exit(1);
	}

	close(pipev[0]);
	write(pipev[1], buf,
	      snprintf(buf, sizeof buf, "%lu\n", (unsigned long) uid));
	write(pipev[1], pass, strlen(pass));
	write(pipev[1], "\n", 1);
	close(pipev[1]);

	while (waitpid(pid, &status, 0) == -1)
		if (errno != EINTR) {
			lprintf(1, "waitpid failed (%s): denying autologin "
				"access for uid %ld\n",
				strerror(errno), (long)uid);
			return 0;
		}
	if (WIFEXITED(status) && !WEXITSTATUS(status))
		return 1;

	return 0;
}
#endif

void do_login()
{
	(CC->logged_in) = 1;
	session_startup();
}


int CtdlTryPassword(char *password)
{
	int code;

	if ((CC->logged_in)) {
		lprintf(5, "CtdlTryPassword: already logged in\n");
		return pass_already_logged_in;
	}
	if (!strcmp(CC->curr_user, NLI)) {
		lprintf(5, "CtdlTryPassword: no user selected\n");
		return pass_no_user;
	}
	if (getuser(&CC->usersupp, CC->curr_user)) {
		lprintf(5, "CtdlTryPassword: internal error\n");
		return pass_internal_error;
	}
	if (password == NULL) {
		lprintf(5, "CtdlTryPassword: NULL password string supplied\n");
		return pass_wrong_password;
	}
	code = (-1);
	if (CC->usersupp.uid == BBSUID) {
		strproc(password);
		strproc(CC->usersupp.password);
		code = strcasecmp(CC->usersupp.password, password);
	}
#ifdef ENABLE_AUTOLOGIN
	else {
		if (validpw(CC->usersupp.uid, password)) {
			code = 0;
			lgetuser(&CC->usersupp, CC->curr_user);
			safestrncpy(CC->usersupp.password, password,
				    sizeof CC->usersupp.password);
			lputuser(&CC->usersupp);
		}
	}
#endif

	if (!code) {
		do_login();
		return pass_ok;
	} else {
		rec_log(CL_BADPW, CC->curr_user);
		return pass_wrong_password;
	}
}


void cmd_pass(char *buf)
{
	char password[SIZ];
	int a;

	extract(password, buf, 0);
	a = CtdlTryPassword(password);

	switch (a) {
	case pass_already_logged_in:
		cprintf("%d Already logged in.\n", ERROR);
		return;
	case pass_no_user:
		cprintf("%d You must send a name with USER first.\n",
			ERROR);
		return;
	case pass_wrong_password:
		cprintf("%d Wrong password.\n", ERROR);
		return;
	case pass_ok:
		logged_in_response();
		return;
		cprintf("%d Can't find user record!\n",
			ERROR + INTERNAL_ERROR);
	}
}



/*
 * Delete a user record *and* all of its related resources.
 */
int purge_user(char pname[])
{
	char filename[64];
	struct usersupp usbuf;
	char lowercase_name[32];
	int a;
	struct CitContext *ccptr;
	int user_is_logged_in = 0;

	for (a = 0; a <= strlen(pname); ++a) {
		lowercase_name[a] = tolower(pname[a]);
	}

	if (getuser(&usbuf, pname) != 0) {
		lprintf(5, "Cannot purge user <%s> - not found\n", pname);
		return (ERROR + NO_SUCH_USER);
	}
	/* Don't delete a user who is currently logged in.  Instead, just
	 * set the access level to 0, and let the account get swept up
	 * during the next purge.
	 */
	user_is_logged_in = 0;
	begin_critical_section(S_SESSION_TABLE);
	for (ccptr = ContextList; ccptr != NULL; ccptr = ccptr->next) {
		if (ccptr->usersupp.usernum == usbuf.usernum) {
			user_is_logged_in = 1;
		}
	}
	end_critical_section(S_SESSION_TABLE);
	if (user_is_logged_in == 1) {
		lprintf(5, "User <%s> is logged in; not deleting.\n", pname);
		usbuf.axlevel = 0;
		putuser(&usbuf);
		return (1);
	}
	lprintf(5, "Deleting user <%s>\n", pname);

	/* Perform any purge functions registered by server extensions */
	PerformUserHooks(usbuf.fullname, usbuf.usernum, EVT_PURGEUSER);

	/* delete any existing user/room relationships */
	cdb_delete(CDB_VISIT, &usbuf.usernum, sizeof(long));

	/* delete the userlog entry */
	cdb_delete(CDB_USERSUPP, lowercase_name, strlen(lowercase_name));

	/* remove the user's bio file */
	snprintf(filename, sizeof filename, "./bio/%ld", usbuf.usernum);
	unlink(filename);

	/* remove the user's picture */
	snprintf(filename, sizeof filename, "./userpics/%ld.gif", usbuf.usernum);
	unlink(filename);

	return (0);
}


/*
 * create_user()  -  back end processing to create a new user
 *
 * Set 'newusername' to the desired account name.
 * Set 'become_user' to nonzero if this is self-service account creation and we want
 * to actually log in as the user we just created, otherwise set it to 0.
 */
int create_user(char *newusername, int become_user)
{
	struct usersupp usbuf;
	struct quickroom qrbuf;
	struct passwd *p = NULL;
	char username[SIZ];
	char mailboxname[ROOMNAMELEN];
	uid_t uid;

	strcpy(username, newusername);
	strproc(username);

#ifdef ENABLE_AUTOLOGIN
	p = (struct passwd *) getpwnam(username);
#endif
	if (p != NULL) {
		extract_token(username, p->pw_gecos, 0, ',');
		uid = p->pw_uid;
	} else {
		uid = BBSUID;
	}

	if (!getuser(&usbuf, username)) {
		return (ERROR + ALREADY_EXISTS);
	}

	/* Go ahead and initialize a new user record */
	memset(&usbuf, 0, sizeof(struct usersupp));
	strcpy(usbuf.fullname, username);
	strcpy(usbuf.password, "");
	usbuf.uid = uid;

	/* These are the default flags on new accounts */
	usbuf.flags = US_LASTOLD | US_DISAPPEAR | US_PAGINATOR | US_FLOORS;

	usbuf.timescalled = 0;
	usbuf.posted = 0;
	usbuf.axlevel = config.c_initax;
	usbuf.USscreenwidth = 80;
	usbuf.USscreenheight = 24;
	usbuf.lastcall = time(NULL);

	/* fetch a new user number */
	usbuf.usernum = get_new_user_number();

	/* The very first user created on the system will always be an Aide */
	if (usbuf.usernum == 1L) {
		usbuf.axlevel = 6;
	}

	/* add user to userlog */
	putuser(&usbuf);

	/*
	 * Give the user a private mailbox and a configuration room.
	 * Make the latter an invisible system room.
	 */
	MailboxName(mailboxname, sizeof mailboxname, &usbuf, MAILROOM);
	create_room(mailboxname, 5, "", 0, 1, 1);

	MailboxName(mailboxname, sizeof mailboxname, &usbuf, USERCONFIGROOM);
	create_room(mailboxname, 5, "", 0, 1, 1);
        if (lgetroom(&qrbuf, USERCONFIGROOM) == 0) {
                qrbuf.QRflags2 |= QR2_SYSTEM;
                lputroom(&qrbuf);
        }

	/* Everything below this line can be bypassed if administratively
	   creating a user, instead of doing self-service account creation
	 */

	if (become_user) {
		/* Now become the user we just created */
		memcpy(&CC->usersupp, &usbuf, sizeof(struct usersupp));
		strcpy(CC->curr_user, username);
		CC->logged_in = 1;
	
		/* Check to make sure we're still who we think we are */
		if (getuser(&CC->usersupp, CC->curr_user)) {
			return (ERROR + INTERNAL_ERROR);
		}
	
		rec_log(CL_NEWUSER, CC->curr_user);
	}

	return (0);
}




/*
 * cmd_newu()  -  create a new user account and log in as that user
 */
void cmd_newu(char *cmdbuf)
{
	int a;
	char username[SIZ];

	if (config.c_disable_newu) {
		cprintf("%d Self-service user account creation "
			"is disabled on this system.\n", ERROR);
		return;
	}

	if (CC->logged_in) {
		cprintf("%d Already logged in.\n", ERROR);
		return;
	}
	if (CC->nologin) {
		cprintf("%d %s: Too many users are already online (maximum is %d)\n",
			ERROR + MAX_SESSIONS_EXCEEDED,
			config.c_nodename, config.c_maxsessions);
	}
	extract(username, cmdbuf, 0);
	username[25] = 0;
	strproc(username);

	if (strlen(username) == 0) {
		cprintf("%d You must supply a user name.\n", ERROR);
		return;
	}

	if ((!strcasecmp(username, "bbs")) ||
	    (!strcasecmp(username, "new")) ||
	    (!strcasecmp(username, "."))) {
		cprintf("%d '%s' is an invalid login name.\n", ERROR, username);
		return;
	}

	a = create_user(username, 1);

	if (a == 0) {
		session_startup();
		logged_in_response();
	} else if (a == ERROR + ALREADY_EXISTS) {
		cprintf("%d '%s' already exists.\n",
			ERROR + ALREADY_EXISTS, username);
		return;
	} else if (a == ERROR + INTERNAL_ERROR) {
		cprintf("%d Internal error - user record disappeared?\n",
			ERROR + INTERNAL_ERROR);
		return;
	} else {
		cprintf("%d unknown error\n", ERROR);
	}
	rec_log(CL_NEWUSER, CC->curr_user);
}



/*
 * set password
 */
void cmd_setp(char *new_pw)
{
	if (CtdlAccessCheck(ac_logged_in)) {
		return;
	}

	if (CC->usersupp.uid != BBSUID) {
		cprintf("%d Not allowed.  Use the 'passwd' command.\n", ERROR);
		return;
	}
	strproc(new_pw);
	if (strlen(new_pw) == 0) {
		cprintf("%d Password unchanged.\n", CIT_OK);
		return;
	}
	lgetuser(&CC->usersupp, CC->curr_user);
	strcpy(CC->usersupp.password, new_pw);
	lputuser(&CC->usersupp);
	cprintf("%d Password changed.\n", CIT_OK);
	rec_log(CL_PWCHANGE, CC->curr_user);
	PerformSessionHooks(EVT_SETPASS);
}


/*
 * cmd_creu()  -  administratively create a new user account (do not log in to it)
 */
void cmd_creu(char *cmdbuf)
{
	int a;
	char username[SIZ];

	if (CtdlAccessCheck(ac_aide)) {
		return;
	}

	extract(username, cmdbuf, 0);
	username[25] = 0;
	strproc(username);

	if (strlen(username) == 0) {
		cprintf("%d You must supply a user name.\n", ERROR);
		return;
	}

	a = create_user(username, 0);

	if (a == 0) {
		cprintf("%d ok\n", CIT_OK);
		return;
	} else if (a == ERROR + ALREADY_EXISTS) {
		cprintf("%d '%s' already exists.\n",
			ERROR + ALREADY_EXISTS, username);
		return;
	} else {
		cprintf("%d An error occured creating the user account.\n", ERROR);
	}
	rec_log(CL_NEWUSER, username);
}



/*
 * get user parameters
 */
void cmd_getu(void)
{

	if (CtdlAccessCheck(ac_logged_in))
		return;

	getuser(&CC->usersupp, CC->curr_user);
	cprintf("%d %d|%d|%d|\n",
		CIT_OK,
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
	if (CtdlAccessCheck(ac_logged_in))
		return;

	if (num_parms(new_parms) < 3) {
		cprintf("%d Usage error.\n", ERROR);
		return;
	}
	lgetuser(&CC->usersupp, CC->curr_user);
	CC->usersupp.USscreenwidth = extract_int(new_parms, 0);
	CC->usersupp.USscreenheight = extract_int(new_parms, 1);
	CC->usersupp.flags = CC->usersupp.flags & (~US_USER_SET);
	CC->usersupp.flags = CC->usersupp.flags |
	    (extract_int(new_parms, 2) & US_USER_SET);

	lputuser(&CC->usersupp);
	cprintf("%d Ok\n", CIT_OK);
}

/*
 * set last read pointer
 */
void cmd_slrp(char *new_ptr)
{
	long newlr;
	struct visit vbuf;

	if (CtdlAccessCheck(ac_logged_in)) {
		return;
	}

	if (!strncasecmp(new_ptr, "highest", 7)) {
		newlr = CC->quickroom.QRhighest;
	} else {
		newlr = atol(new_ptr);
	}

	lgetuser(&CC->usersupp, CC->curr_user);

	CtdlGetRelationship(&vbuf, &CC->usersupp, &CC->quickroom);
	vbuf.v_lastseen = newlr;
	snprintf(vbuf.v_seen, sizeof vbuf.v_seen, "*:%ld", newlr);
	CtdlSetRelationship(&vbuf, &CC->usersupp, &CC->quickroom);

	lputuser(&CC->usersupp);
	cprintf("%d %ld\n", CIT_OK, newlr);
}


void cmd_seen(char *argbuf) {
	long target_msgnum = 0L;
	int target_setting = 0;

	if (CtdlAccessCheck(ac_logged_in)) {
		return;
	}

	if (num_parms(argbuf) != 2) {
		cprintf("%d Invalid parameters\n", ERROR);
		return;
	}

	target_msgnum = extract_long(argbuf, 0);
	target_setting = extract_int(argbuf, 1);

	CtdlSetSeen(target_msgnum, target_setting);
	cprintf("%d OK\n", CIT_OK);
}


void cmd_gtsn(char *argbuf) {
	char buf[SIZ];

	if (CtdlAccessCheck(ac_logged_in)) {
		return;
	}

	CtdlGetSeen(buf);
	cprintf("%d %s\n", CIT_OK, buf);
}



/*
 * INVT and KICK commands
 */
void cmd_invt_kick(char *iuser, int op)
			/* user name */
{				/* 1 = invite, 0 = kick out */
	struct usersupp USscratch;
	char bbb[SIZ];
	struct visit vbuf;

	/*
	 * These commands are only allowed by aides, room aides,
	 * and room namespace owners
	 */
	if (is_room_aide()
	   || (atol(CC->quickroom.QRname) == CC->usersupp.usernum) ) {
		/* access granted */
	} else {
		/* access denied */
                cprintf("%d Higher access or room ownership required.\n",
                        ERROR + HIGHER_ACCESS_REQUIRED);
                return;
        }

	if (!strncasecmp(CC->quickroom.QRname, config.c_baseroom,
			 ROOMNAMELEN)) {
		cprintf("%d Can't add/remove users from this room.\n",
			ERROR + NOT_HERE);
		return;
	}

	if (lgetuser(&USscratch, iuser) != 0) {
		cprintf("%d No such user.\n", ERROR);
		return;
	}
	CtdlGetRelationship(&vbuf, &USscratch, &CC->quickroom);

	if (op == 1) {
		vbuf.v_flags = vbuf.v_flags & ~V_FORGET & ~V_LOCKOUT;
		vbuf.v_flags = vbuf.v_flags | V_ACCESS;
	}
	if (op == 0) {
		vbuf.v_flags = vbuf.v_flags & ~V_ACCESS;
		vbuf.v_flags = vbuf.v_flags | V_FORGET | V_LOCKOUT;
	}
	CtdlSetRelationship(&vbuf, &USscratch, &CC->quickroom);

	lputuser(&USscratch);

	/* post a message in Aide> saying what we just did */
	snprintf(bbb, sizeof bbb, "%s %s %s> by %s\n",
		iuser,
		((op == 1) ? "invited to" : "kicked out of"),
		CC->quickroom.QRname,
		CC->usersupp.fullname);
	aide_message(bbb);

	cprintf("%d %s %s %s.\n",
		CIT_OK, iuser,
		((op == 1) ? "invited to" : "kicked out of"),
		CC->quickroom.QRname);
	return;
}


/*
 * Forget (Zap) the current room (API call)
 * Returns 0 on success
 */
int CtdlForgetThisRoom(void) {
	struct visit vbuf;

	/* On some systems, Aides are not allowed to forget rooms */
	if (is_aide() && (config.c_aide_zap == 0)
	   && ((CC->quickroom.QRflags & QR_MAILBOX) == 0)  ) {
		return(1);
	}

	lgetuser(&CC->usersupp, CC->curr_user);
	CtdlGetRelationship(&vbuf, &CC->usersupp, &CC->quickroom);

	vbuf.v_flags = vbuf.v_flags | V_FORGET;
	vbuf.v_flags = vbuf.v_flags & ~V_ACCESS;

	CtdlSetRelationship(&vbuf, &CC->usersupp, &CC->quickroom);
	lputuser(&CC->usersupp);

	/* Return to the Lobby, so we don't end up in an undefined room */
	usergoto(config.c_baseroom, 0, 0, NULL, NULL);
	return(0);

}


/*
 * forget (Zap) the current room
 */
void cmd_forg(void)
{

	if (CtdlAccessCheck(ac_logged_in)) {
		return;
	}

	if (CtdlForgetThisRoom() == 0) {
		cprintf("%d Ok\n", CIT_OK);
	}
	else {
		cprintf("%d You may not forget this room.\n", ERROR);
	}
}

/*
 * Get Next Unregistered User
 */
void cmd_gnur(void)
{
	struct cdbdata *cdbus;
	struct usersupp usbuf;

	if (CtdlAccessCheck(ac_aide)) {
		return;
	}

	if ((CitControl.MMflags & MM_VALID) == 0) {
		cprintf("%d There are no unvalidated users.\n", CIT_OK);
		return;
	}

	/* There are unvalidated users.  Traverse the usersupp database,
	 * and return the first user we find that needs validation.
	 */
	cdb_rewind(CDB_USERSUPP);
	while (cdbus = cdb_next_item(CDB_USERSUPP), cdbus != NULL) {
		memset(&usbuf, 0, sizeof(struct usersupp));
		memcpy(&usbuf, cdbus->ptr,
		       ((cdbus->len > sizeof(struct usersupp)) ?
			sizeof(struct usersupp) : cdbus->len));
		cdb_free(cdbus);
		if ((usbuf.flags & US_NEEDVALID)
		    && (usbuf.axlevel > 0)) {
			cprintf("%d %s\n", MORE_DATA, usbuf.fullname);
			cdb_close_cursor(CDB_USERSUPP);
			return;
		}
	}

	/* If we get to this point, there are no more unvalidated users.
	 * Therefore we clear the "users need validation" flag.
	 */

	begin_critical_section(S_CONTROL);
	get_control();
	CitControl.MMflags = CitControl.MMflags & (~MM_VALID);
	put_control();
	end_critical_section(S_CONTROL);
	cprintf("%d *** End of registration.\n", CIT_OK);


}


/*
 * validate a user
 */
void cmd_vali(char *v_args)
{
	char user[SIZ];
	int newax;
	struct usersupp userbuf;

	extract(user, v_args, 0);
	newax = extract_int(v_args, 1);

	if (CtdlAccessCheck(ac_aide)) {
		return;
	}

	if (lgetuser(&userbuf, user) != 0) {
		cprintf("%d '%s' not found.\n", ERROR + NO_SUCH_USER, user);
		return;
	}

	userbuf.axlevel = newax;
	userbuf.flags = (userbuf.flags & ~US_NEEDVALID);

	lputuser(&userbuf);

	/* If the access level was set to zero, delete the user */
	if (newax == 0) {
		if (purge_user(user) == 0) {
			cprintf("%d %s Deleted.\n", CIT_OK, userbuf.fullname);
			return;
		}
	}
	cprintf("%d User '%s' validated.\n", CIT_OK, userbuf.fullname);
}



/* 
 *  Traverse the user file...
 */
void ForEachUser(void (*CallBack) (struct usersupp * EachUser, void *out_data),
		 void *in_data)
{
	struct usersupp usbuf;
	struct cdbdata *cdbus;

	cdb_rewind(CDB_USERSUPP);

	while (cdbus = cdb_next_item(CDB_USERSUPP), cdbus != NULL) {
		memset(&usbuf, 0, sizeof(struct usersupp));
		memcpy(&usbuf, cdbus->ptr,
		       ((cdbus->len > sizeof(struct usersupp)) ?
			sizeof(struct usersupp) : cdbus->len));
		cdb_free(cdbus);
		(*CallBack) (&usbuf, in_data);
	}
}


/*
 * List one user (this works with cmd_list)
 */
void ListThisUser(struct usersupp *usbuf, void *data)
{
	if (usbuf->axlevel > 0) {
		if ((CC->usersupp.axlevel >= 6)
		    || ((usbuf->flags & US_UNLISTED) == 0)
		    || ((CC->internal_pgm))) {
			cprintf("%s|%d|%ld|%ld|%ld|%ld|",
				usbuf->fullname,
				usbuf->axlevel,
				usbuf->usernum,
				(long)usbuf->lastcall,
				usbuf->timescalled,
				usbuf->posted);
			if (CC->usersupp.axlevel >= 6)
				cprintf("%s", usbuf->password);
			cprintf("\n");
		}
	}
}

/* 
 *  List users
 */
void cmd_list(void)
{
	cprintf("%d \n", LISTING_FOLLOWS);
	ForEachUser(ListThisUser, NULL);
	cprintf("000\n");
}




/*
 * assorted info we need to check at login
 */
void cmd_chek(void)
{
	int mail = 0;
	int regis = 0;
	int vali = 0;

	if (CtdlAccessCheck(ac_logged_in)) {
		return;
	}

	getuser(&CC->usersupp, CC->curr_user);	/* no lock is needed here */
	if ((REGISCALL != 0) && ((CC->usersupp.flags & US_REGIS) == 0))
		regis = 1;

	if (CC->usersupp.axlevel >= 6) {
		get_control();
		if (CitControl.MMflags & MM_VALID)
			vali = 1;
	}

	/* check for mail */
	mail = InitialMailCheck();

	cprintf("%d %d|%d|%d\n", CIT_OK, mail, regis, vali);
}


/*
 * check to see if a user exists
 */
void cmd_qusr(char *who)
{
	struct usersupp usbuf;

	if (getuser(&usbuf, who) == 0) {
		cprintf("%d %s\n", CIT_OK, usbuf.fullname);
	} else {
		cprintf("%d No such user.\n", ERROR + NO_SUCH_USER);
	}
}


/*
 * Administrative Get User Parameters
 */
void cmd_agup(char *cmdbuf)
{
	struct usersupp usbuf;
	char requested_user[SIZ];

	if (CtdlAccessCheck(ac_aide)) {
		return;
	}

	extract(requested_user, cmdbuf, 0);
	if (getuser(&usbuf, requested_user) != 0) {
		cprintf("%d No such user.\n", ERROR + NO_SUCH_USER);
		return;
	}
	cprintf("%d %s|%s|%u|%ld|%ld|%d|%ld|%ld|%d\n",
		CIT_OK,
		usbuf.fullname,
		usbuf.password,
		usbuf.flags,
		usbuf.timescalled,
		usbuf.posted,
		(int) usbuf.axlevel,
		usbuf.usernum,
		(long)usbuf.lastcall,
		usbuf.USuserpurge);
}



/*
 * Administrative Set User Parameters
 */
void cmd_asup(char *cmdbuf)
{
	struct usersupp usbuf;
	char requested_user[SIZ];
	int np;
	int newax;
	int deleted = 0;

	if (CtdlAccessCheck(ac_aide))
		return;

	extract(requested_user, cmdbuf, 0);
	if (lgetuser(&usbuf, requested_user) != 0) {
		cprintf("%d No such user.\n", ERROR + NO_SUCH_USER);
		return;
	}
	np = num_parms(cmdbuf);
	if (np > 1)
		extract(usbuf.password, cmdbuf, 1);
	if (np > 2)
		usbuf.flags = extract_int(cmdbuf, 2);
	if (np > 3)
		usbuf.timescalled = extract_int(cmdbuf, 3);
	if (np > 4)
		usbuf.posted = extract_int(cmdbuf, 4);
	if (np > 5) {
		newax = extract_int(cmdbuf, 5);
		if ((newax >= 0) && (newax <= 6)) {
			usbuf.axlevel = extract_int(cmdbuf, 5);
		}
	}
	if (np > 7) {
		usbuf.lastcall = extract_long(cmdbuf, 7);
	}
	if (np > 8) {
		usbuf.USuserpurge = extract_int(cmdbuf, 8);
	}
	lputuser(&usbuf);
	if (usbuf.axlevel == 0) {
		if (purge_user(requested_user) == 0) {
			deleted = 1;
		}
	}
	cprintf("%d Ok", CIT_OK);
	if (deleted)
		cprintf(" (%s deleted)", requested_user);
	cprintf("\n");
}



/*
 * Check to see if the user who we just sent mail to is logged in.  If yes,
 * bump the 'new mail' counter for their session.  That enables them to
 * receive a new mail notification without having to hit the database.
 */
void BumpNewMailCounter(long which_user) {
	struct CitContext *ptr;

	begin_critical_section(S_SESSION_TABLE);

	for (ptr = ContextList; ptr != NULL; ptr = ptr->next) {
		if (ptr->usersupp.usernum == which_user) {
			ptr->newmail += 1;
		}
	}

	end_critical_section(S_SESSION_TABLE);
}


/*
 * Count the number of new mail messages the user has
 */
int NewMailCount()
{
	int num_newmsgs = 0;

	num_newmsgs = CC->newmail;
	CC->newmail = 0;

	return (num_newmsgs);
}


/*
 * Count the number of new mail messages the user has
 */
int InitialMailCheck()
{
        int num_newmsgs = 0;
        int a;
        char mailboxname[ROOMNAMELEN];
        struct quickroom mailbox;
        struct visit vbuf;
        struct cdbdata *cdbfr;
        long *msglist = NULL;
        int num_msgs = 0;

        MailboxName(mailboxname, sizeof mailboxname, &CC->usersupp, MAILROOM);
        if (getroom(&mailbox, mailboxname) != 0)
                return (0);
        CtdlGetRelationship(&vbuf, &CC->usersupp, &mailbox);

        cdbfr = cdb_fetch(CDB_MSGLISTS, &mailbox.QRnumber, sizeof(long));

        if (cdbfr != NULL) {
                msglist = mallok(cdbfr->len);
                memcpy(msglist, cdbfr->ptr, cdbfr->len);
                num_msgs = cdbfr->len / sizeof(long);
                cdb_free(cdbfr);
        }
        if (num_msgs > 0)
                for (a = 0; a < num_msgs; ++a) {
                        if (msglist[a] > 0L) {
                                if (msglist[a] > vbuf.v_lastseen) {
                                        ++num_newmsgs;
                                }
                        }
                }
        if (msglist != NULL)
                phree(msglist);

        return (num_newmsgs);
}



/*
 * Set the preferred view for the current user/room combination
 */
void cmd_view(char *cmdbuf) {
	int requested_view;
	struct visit vbuf;

	if (CtdlAccessCheck(ac_logged_in)) {
		return;
	}

	requested_view = extract_int(cmdbuf, 0);

	CtdlGetRelationship(&vbuf, &CC->usersupp, &CC->quickroom);
	vbuf.v_view = requested_view;
	CtdlSetRelationship(&vbuf, &CC->usersupp, &CC->quickroom);
	
	cprintf("%d ok\n", CIT_OK);
}
