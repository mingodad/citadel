/* 
 * $Id$
 *
 * Server functions which perform operations on user objects.
 *
 */

#include "sysdep.h"
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <pwd.h>
#include <ctype.h>
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
#include <limits.h>
#ifndef ENABLE_CHKPWD
#include "auth.h"
#endif
#include "citadel.h"
#include "server.h"
#include "database.h"
#include "user_ops.h"
#include "serv_extensions.h"
#include "sysdep_decls.h"
#include "support.h"
#include "room_ops.h"
#include "file_ops.h"
#include "control.h"
#include "msgbase.h"
#include "config.h"
#include "tools.h"
#include "citserver.h"


/*
 * makeuserkey() - convert a username into the format used as a database key
 *                 (it's just the username converted into lower case)
 */
static INLINE void makeuserkey(char *key, char *username) {
	int i, len;

	len = strlen(username);
	for (i=0; i<=len; ++i) {
		key[i] = tolower(username[i]);
	}
}


/*
 * getuser()  -  retrieve named user into supplied buffer.
 *               returns 0 on success
 */
int getuser(struct ctdluser *usbuf, char name[])
{

	char usernamekey[USERNAME_SIZE];
	char sysuser_name[USERNAME_SIZE];
	struct cdbdata *cdbus;
	int using_sysuser = 0;

	if (usbuf != NULL) {
		memset(usbuf, 0, sizeof(struct ctdluser));
	}

#ifdef ENABLE_AUTOLOGIN
	if (CtdlAssociateSystemUser(sysuser_name, name) == 0) {
		++using_sysuser;
	}
#endif

	if (using_sysuser) {
		makeuserkey(usernamekey, sysuser_name);
	}
	else {
		makeuserkey(usernamekey, name);
	}

	cdbus = cdb_fetch(CDB_USERS, usernamekey, strlen(usernamekey));
	if (cdbus == NULL) {	/* user not found */
		return(1);
	}
	if (usbuf != NULL) {
		memcpy(usbuf, cdbus->ptr,
			((cdbus->len > sizeof(struct ctdluser)) ?
			 sizeof(struct ctdluser) : cdbus->len));
	}
	cdb_free(cdbus);

	return (0);
}


/*
 * lgetuser()  -  same as getuser() but locks the record
 */
int lgetuser(struct ctdluser *usbuf, char *name)
{
	int retcode;

	retcode = getuser(usbuf, name);
	if (retcode == 0) {
		begin_critical_section(S_USERS);
	}
	return (retcode);
}


/*
 * putuser()  -  write user buffer into the correct place on disk
 */
void putuser(struct ctdluser *usbuf)
{
	char usernamekey[USERNAME_SIZE];

	makeuserkey(usernamekey, usbuf->fullname);

	usbuf->version = REV_LEVEL;
	cdb_store(CDB_USERS,
		  usernamekey, strlen(usernamekey),
		  usbuf, sizeof(struct ctdluser));

}


/*
 * lputuser()  -  same as putuser() but locks the record
 */
void lputuser(struct ctdluser *usbuf)
{
	putuser(usbuf);
	end_critical_section(S_USERS);
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
			 struct ctdluser *rel_user,
			 struct ctdlroom *rel_room)
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
			 struct ctdluser *rel_user,
			 struct ctdlroom *rel_room)
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


void MailboxName(char *buf, size_t n, const struct ctdluser *who, const char *prefix)
{
	snprintf(buf, n, "%010ld.%s", who->usernum, prefix);
}


/*
 * Is the user currently logged in an Aide?
 */
int is_aide(void)
{
	if (CC->user.axlevel >= 6)
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

	if ((CC->user.axlevel >= 6)
	    || (CC->room.QRroomaide == CC->user.usernum)) {
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
int getuserbynumber(struct ctdluser *usbuf, long int number)
{
	struct cdbdata *cdbus;

	cdb_rewind(CDB_USERS);

	while (cdbus = cdb_next_item(CDB_USERS), cdbus != NULL) {
		memset(usbuf, 0, sizeof(struct ctdluser));
		memcpy(usbuf, cdbus->ptr,
		       ((cdbus->len > sizeof(struct ctdluser)) ?
			sizeof(struct ctdluser) : cdbus->len));
		cdb_free(cdbus);
		if (usbuf->usernum == number) {
			cdb_close_cursor(CDB_USERS);
			return (0);
		}
	}
	return (-1);
}


/*
 * See if we can translate a system login name (i.e. from /etc/passwd)
 * to a Citadel screen name.  Returns 0 if one is found.
 */
int CtdlAssociateSystemUser(char *screenname, char *loginname) {
	struct passwd *p;
	int a;

	p = (struct passwd *) getpwnam(loginname);
	if (p != NULL) {
		strcpy(screenname, p->pw_gecos);
		for (a = 0; a < strlen(screenname); ++a) {
			if (screenname[a] == ',') {
				screenname[a] = 0;
			}
		}
		return(0);
	}
	return(1);
}



/*
 * Back end for cmd_user() and its ilk
 */
int CtdlLoginExistingUser(char *trythisname)
{
	char username[SIZ];
	int found_user;
	struct recptypes *valid = NULL;

	if (trythisname == NULL) return login_not_found;
	safestrncpy(username, trythisname, USERNAME_SIZE);
	striplt(username);

	if ((CC->logged_in)) {
		return login_already_logged_in;
	}

	/* First, try to log in as if the supplied name is a display name */
	found_user = getuser(&CC->user, username);

	/* If that didn't work, try to log in as if the supplied name
	 * is an e-mail address
	 */
	if (found_user != 0) {
		valid = validate_recipients(username);
		if (valid != NULL) {
			if (valid->num_local == 1) {
				found_user = getuser(&CC->user,
						valid->recp_local);
			}
			free(valid);
		}
	}

#ifdef ENABLE_AUTOLOGIN
	/* If we haven't found the account yet, and the supplied name
	 * is a login name on the underlying host system, create the
	 * account.
	 */
	if (found_user != 0) {
		struct passwd *p = (struct passwd *) getpwnam(username);

		if (p != NULL) {
			create_user(username, 0);
			found_user = getuser(&CC->user, username);
		}
	}
#endif /* ENABLE_AUTOLOGIN */

	/* Did we find something? */
	if (found_user == 0) {
		if (((CC->nologin)) && (CC->user.axlevel < 6)) {
			return login_too_many_users;
		} else {
			safestrncpy(CC->curr_user, CC->user.fullname,
					sizeof CC->curr_user);
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
	char username[256];
	int a;

	extract_token(username, cmdbuf, 0, '|', sizeof username);
	striplt(username);

	a = CtdlLoginExistingUser(username);
	switch (a) {
	case login_already_logged_in:
		cprintf("%d Already logged in.\n", ERROR + ALREADY_LOGGED_IN);
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
		cprintf("%d %s not found.\n", ERROR + NO_SUCH_USER, username);
		return;
	default:
		cprintf("%d Internal error\n", ERROR + INTERNAL_ERROR);
	}
}



/*
 * session startup code which is common to both cmd_pass() and cmd_newu()
 */
void session_startup(void)
{
	int i;

	lprintf(CTDL_NOTICE, "<%s> logged in\n", CC->curr_user);

	lgetuser(&CC->user, CC->curr_user);
	++(CC->user.timescalled);
	CC->previous_login = CC->user.lastcall;
	time(&CC->user.lastcall);

	/* If this user's name is the name of the system administrator
	 * (as specified in setup), automatically assign access level 6.
	 */
	if (!strcasecmp(CC->user.fullname, config.c_sysadm)) {
		CC->user.axlevel = 6;
	}
	lputuser(&CC->user);

	/*
	 * Populate CC->cs_inet_email with a default address.  This will be
	 * overwritten with the user's directory address, if one exists, when
	 * the vCard module's login hook runs.
	 */
	snprintf(CC->cs_inet_email, sizeof CC->cs_inet_email, "%s@%s",
		CC->user.fullname, config.c_fqdn);
	for (i=0; i<strlen(CC->cs_inet_email); ++i) {
		if (isspace(CC->cs_inet_email[i])) {
			CC->cs_inet_email[i] = '_';
		}
	}

	/* Create any personal rooms required by the system.
	 * (Technically, MAILROOM should be there already, but just in case...)
	 */
	create_room(MAILROOM, 4, "", 0, 1, 0, VIEW_MAILBOX);
	create_room(SENTITEMS, 4, "", 0, 1, 0, VIEW_MAILBOX);
	create_room(USERTRASHROOM, 4, "", 0, 1, 0, VIEW_MAILBOX);

	/* Run any startup routines registered by loadable modules */
	PerformSessionHooks(EVT_LOGIN);

	/* Enter the lobby */
	usergoto(config.c_baseroom, 0, 0, NULL, NULL);
}


void logged_in_response(void)
{
	cprintf("%d %s|%d|%ld|%ld|%u|%ld|%ld\n",
		CIT_OK, CC->user.fullname, CC->user.axlevel,
		CC->user.timescalled, CC->user.posted,
		CC->user.flags, CC->user.usernum,
		CC->previous_login);
}



/* 
 * misc things to be taken care of when a user is logged out
 */
void logout(struct CitContext *who)
{
	/*
	 * Clear out some session data.  Most likely, the CitContext for this
	 * session is about to get nuked when the session disconnects, but
	 * since it's possible to log in again without reconnecting, we cannot
	 * make that assumption.
	 */
	strcpy(who->fake_username, "");
	strcpy(who->fake_postname, "");
	strcpy(who->fake_hostname, "");
	strcpy(who->fake_roomname, "");
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

	/* Do modular stuff... */
	PerformSessionHooks(EVT_LOGOUT);

	/* Free any output buffers */
	if (who->output_buffer != NULL) {
		unbuffer_output();
	}
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
		lprintf(CTDL_ERR, "pipe failed (%s): denying autologin access for "
			"uid %ld\n", strerror(errno), (long)uid);
		return 0;
	}
	switch (pid = fork()) {
	case -1:
		lprintf(CTDL_ERR, "fork failed (%s): denying autologin access for "
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

		execl(CTDLDIR "/chkpwd", CTDLDIR "/chkpwd", NULL);
		perror(CTDLDIR "/chkpwd");
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
			lprintf(CTDL_ERR, "waitpid failed (%s): denying autologin "
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
		lprintf(CTDL_WARNING, "CtdlTryPassword: already logged in\n");
		return pass_already_logged_in;
	}
	if (!strcmp(CC->curr_user, NLI)) {
		lprintf(CTDL_WARNING, "CtdlTryPassword: no user selected\n");
		return pass_no_user;
	}
	if (getuser(&CC->user, CC->curr_user)) {
		lprintf(CTDL_ERR, "CtdlTryPassword: internal error\n");
		return pass_internal_error;
	}
	if (password == NULL) {
		lprintf(CTDL_INFO, "CtdlTryPassword: NULL password string supplied\n");
		return pass_wrong_password;
	}
	code = (-1);


#ifdef ENABLE_AUTOLOGIN
	/* A uid of CTDLUID or -1 indicates that this user exists only in
	 * Citadel, not in the underlying operating system.
	 */
	if ( (CC->user.uid == CTDLUID) || (CC->user.uid == (-1)) ) {
		strproc(password);
		strproc(CC->user.password);
		code = strcasecmp(CC->user.password, password);
	}
	/* Any other uid means we have to check the system password database */
	else {
		if (validpw(CC->user.uid, password)) {
			code = 0;
			lgetuser(&CC->user, CC->curr_user);
			safestrncpy(CC->user.password, password,
				    sizeof CC->user.password);
			lputuser(&CC->user);
		}
	}

#else /* ENABLE_AUTOLOGIN */
	strproc(password);
	strproc(CC->user.password);
	code = strcasecmp(CC->user.password, password);

#endif /* ENABLE_AUTOLOGIN */

	if (!code) {
		do_login();
		return pass_ok;
	} else {
		lprintf(CTDL_WARNING, "Bad password specified for <%s>\n", CC->curr_user);
		return pass_wrong_password;
	}
}


void cmd_pass(char *buf)
{
	char password[256];
	int a;

	extract_token(password, buf, 0, '|', sizeof password);
	a = CtdlTryPassword(password);

	switch (a) {
	case pass_already_logged_in:
		cprintf("%d Already logged in.\n", ERROR + ALREADY_LOGGED_IN);
		return;
	case pass_no_user:
		cprintf("%d You must send a name with USER first.\n",
			ERROR + USERNAME_REQUIRED);
		return;
	case pass_wrong_password:
		cprintf("%d Wrong password.\n", ERROR + PASSWORD_REQUIRED);
		return;
	case pass_ok:
		logged_in_response();
		return;
	}
}



/*
 * Delete a user record *and* all of its related resources.
 */
int purge_user(char pname[])
{
	char filename[64];
	struct ctdluser usbuf;
	char usernamekey[USERNAME_SIZE];
	struct CitContext *ccptr;
	int user_is_logged_in = 0;

	makeuserkey(usernamekey, pname);

	if (getuser(&usbuf, pname) != 0) {
		lprintf(CTDL_ERR, "Cannot purge user <%s> - not found\n", pname);
		return (ERROR + NO_SUCH_USER);
	}
	/* Don't delete a user who is currently logged in.  Instead, just
	 * set the access level to 0, and let the account get swept up
	 * during the next purge.
	 */
	user_is_logged_in = 0;
	begin_critical_section(S_SESSION_TABLE);
	for (ccptr = ContextList; ccptr != NULL; ccptr = ccptr->next) {
		if (ccptr->user.usernum == usbuf.usernum) {
			user_is_logged_in = 1;
		}
	}
	end_critical_section(S_SESSION_TABLE);
	if (user_is_logged_in == 1) {
		lprintf(CTDL_WARNING, "User <%s> is logged in; not deleting.\n", pname);
		usbuf.axlevel = 0;
		putuser(&usbuf);
		return (1);
	}
	lprintf(CTDL_NOTICE, "Deleting user <%s>\n", pname);

	/* Perform any purge functions registered by server extensions */
	PerformUserHooks(&usbuf, EVT_PURGEUSER);

	/* delete any existing user/room relationships */
	cdb_delete(CDB_VISIT, &usbuf.usernum, sizeof(long));

	/* delete the userlog entry */
	cdb_delete(CDB_USERS, usernamekey, strlen(usernamekey));

	/* remove the user's bio file */
	snprintf(filename, sizeof filename, 
#ifndef HAVE_DATA_DIR
			 "."
#else
			 DATA_DIR
#endif
			 "/bio/%ld", usbuf.usernum);
	unlink(filename);

	/* remove the user's picture */
	snprintf(filename, sizeof filename, 
#ifndef HAVE_DATA_DIR
			 "."
#else
			 DATA_DIR
#endif
			 "/userpics/%ld.gif", usbuf.usernum);
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
	struct ctdluser usbuf;
	struct ctdlroom qrbuf;
	char username[256];
	char mailboxname[ROOMNAMELEN];
	uid_t uid;

	safestrncpy(username, newusername, sizeof username);
	strproc(username);

#ifdef ENABLE_AUTOLOGIN
	{
		struct passwd *p = (struct passwd *) getpwnam(username);

		if (p != NULL) {
			extract_token(username, p->pw_gecos, 0, ',', sizeof username);
			uid = p->pw_uid;
		} else {
			uid = (-1);
		}
	}
#else
	uid = (-1);
#endif

	if (!getuser(&usbuf, username)) {
		return (ERROR + ALREADY_EXISTS);
	}

	/* Go ahead and initialize a new user record */
	memset(&usbuf, 0, sizeof(struct ctdluser));
	safestrncpy(usbuf.fullname, username, sizeof usbuf.fullname);
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
	create_room(mailboxname, 5, "", 0, 1, 1, VIEW_MAILBOX);

	MailboxName(mailboxname, sizeof mailboxname, &usbuf, USERCONFIGROOM);
	create_room(mailboxname, 5, "", 0, 1, 1, VIEW_BBS);
        if (lgetroom(&qrbuf, mailboxname) == 0) {
                qrbuf.QRflags2 |= QR2_SYSTEM;
                lputroom(&qrbuf);
        }

	/* Perform any create functions registered by server extensions */
	PerformUserHooks(&usbuf, EVT_NEWUSER);

	/* Everything below this line can be bypassed if administratively
	 * creating a user, instead of doing self-service account creation
	 */

	if (become_user) {
		/* Now become the user we just created */
		memcpy(&CC->user, &usbuf, sizeof(struct ctdluser));
		safestrncpy(CC->curr_user, username, sizeof CC->curr_user);
		CC->logged_in = 1;
	
		/* Check to make sure we're still who we think we are */
		if (getuser(&CC->user, CC->curr_user)) {
			return (ERROR + INTERNAL_ERROR);
		}
	}

	lprintf(CTDL_NOTICE, "New user <%s> created\n", username);
	return (0);
}




/*
 * cmd_newu()  -  create a new user account and log in as that user
 */
void cmd_newu(char *cmdbuf)
{
	int a;
	char username[26];

	if (config.c_disable_newu) {
		cprintf("%d Self-service user account creation "
			"is disabled on this system.\n", ERROR + NOT_HERE);
		return;
	}

	if (CC->logged_in) {
		cprintf("%d Already logged in.\n", ERROR + ALREADY_LOGGED_IN);
		return;
	}
	if (CC->nologin) {
		cprintf("%d %s: Too many users are already online (maximum is %d)\n",
			ERROR + MAX_SESSIONS_EXCEEDED,
			config.c_nodename, config.c_maxsessions);
	}
	extract_token(username, cmdbuf, 0, '|', sizeof username);
	username[25] = 0;
	strproc(username);

	if (strlen(username) == 0) {
		cprintf("%d You must supply a user name.\n", ERROR + USERNAME_REQUIRED);
		return;
	}

	if ((!strcasecmp(username, "bbs")) ||
	    (!strcasecmp(username, "new")) ||
	    (!strcasecmp(username, "."))) {
		cprintf("%d '%s' is an invalid login name.\n", ERROR + ILLEGAL_VALUE, username);
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
		cprintf("%d unknown error\n", ERROR + INTERNAL_ERROR);
	}
}



/*
 * set password
 */
void cmd_setp(char *new_pw)
{
	if (CtdlAccessCheck(ac_logged_in)) {
		return;
	}
	if ( (CC->user.uid != CTDLUID) && (CC->user.uid != (-1)) ) {
		cprintf("%d Not allowed.  Use the 'passwd' command.\n", ERROR + NOT_HERE);
		return;
	}
	strproc(new_pw);
	if (strlen(new_pw) == 0) {
		cprintf("%d Password unchanged.\n", CIT_OK);
		return;
	}
	lgetuser(&CC->user, CC->curr_user);
	safestrncpy(CC->user.password, new_pw, sizeof(CC->user.password));
	lputuser(&CC->user);
	cprintf("%d Password changed.\n", CIT_OK);
	lprintf(CTDL_INFO, "Password changed for user <%s>\n", CC->curr_user);
	PerformSessionHooks(EVT_SETPASS);
}


/*
 * cmd_creu() - administratively create a new user account (do not log in to it)
 */
void cmd_creu(char *cmdbuf)
{
	int a;
	char username[26];
	char password[32];
	struct ctdluser tmp;

	if (CtdlAccessCheck(ac_aide)) {
		return;
	}

	extract_token(username, cmdbuf, 0, '|', sizeof username);
	extract_token(password, cmdbuf, 1, '|', sizeof password);
	username[25] = 0;
	password[31] = 0;
	strproc(username);
	strproc(password);

	if (strlen(username) == 0) {
		cprintf("%d You must supply a user name.\n", ERROR + USERNAME_REQUIRED);
		return;
	}

	a = create_user(username, 0);

	if (a == 0) {
		if (strlen(password) > 0) {
			lgetuser(&tmp, username);
			safestrncpy(tmp.password, password, sizeof(tmp.password));
			lputuser(&tmp);
		}
		cprintf("%d User '%s' created %s.\n", CIT_OK, username,
				(strlen(password) > 0) ? "and password set" :
				"with no password");
		return;
	} else if (a == ERROR + ALREADY_EXISTS) {
		cprintf("%d '%s' already exists.\n",
			ERROR + ALREADY_EXISTS, username);
		return;
	} else {
		cprintf("%d An error occured creating the user account.\n", ERROR + INTERNAL_ERROR);
	}
}



/*
 * get user parameters
 */
void cmd_getu(void)
{

	if (CtdlAccessCheck(ac_logged_in))
		return;

	getuser(&CC->user, CC->curr_user);
	cprintf("%d %d|%d|%d|\n",
		CIT_OK,
		CC->user.USscreenwidth,
		CC->user.USscreenheight,
		(CC->user.flags & US_USER_SET)
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
		cprintf("%d Usage error.\n", ERROR + ILLEGAL_VALUE);
		return;
	}
	lgetuser(&CC->user, CC->curr_user);
	CC->user.USscreenwidth = extract_int(new_parms, 0);
	CC->user.USscreenheight = extract_int(new_parms, 1);
	CC->user.flags = CC->user.flags & (~US_USER_SET);
	CC->user.flags = CC->user.flags |
	    (extract_int(new_parms, 2) & US_USER_SET);

	lputuser(&CC->user);
	cprintf("%d Ok\n", CIT_OK);
}

/*
 * set last read pointer
 */
void cmd_slrp(char *new_ptr)
{
	long newlr;
	struct visit vbuf;
	struct visit original_vbuf;

	if (CtdlAccessCheck(ac_logged_in)) {
		return;
	}

	if (!strncasecmp(new_ptr, "highest", 7)) {
		newlr = CC->room.QRhighest;
	} else {
		newlr = atol(new_ptr);
	}

	lgetuser(&CC->user, CC->curr_user);

	CtdlGetRelationship(&vbuf, &CC->user, &CC->room);
	memcpy(&original_vbuf, &vbuf, sizeof(struct visit));
	vbuf.v_lastseen = newlr;
	snprintf(vbuf.v_seen, sizeof vbuf.v_seen, "*:%ld", newlr);

	/* Only rewrite the record if it changed */
	if ( (vbuf.v_lastseen != original_vbuf.v_lastseen)
	   || (strcmp(vbuf.v_seen, original_vbuf.v_seen)) ) {
		CtdlSetRelationship(&vbuf, &CC->user, &CC->room);
	}

	lputuser(&CC->user);
	cprintf("%d %ld\n", CIT_OK, newlr);
}


void cmd_seen(char *argbuf) {
	long target_msgnum = 0L;
	int target_setting = 0;

	if (CtdlAccessCheck(ac_logged_in)) {
		return;
	}

	if (num_parms(argbuf) != 2) {
		cprintf("%d Invalid parameters\n", ERROR + ILLEGAL_VALUE);
		return;
	}

	target_msgnum = extract_long(argbuf, 0);
	target_setting = extract_int(argbuf, 1);

	CtdlSetSeen(&target_msgnum, 1, target_setting,
			ctdlsetseen_seen, NULL, NULL);
	cprintf("%d OK\n", CIT_OK);
}


void cmd_gtsn(char *argbuf) {
	char buf[SIZ];

	if (CtdlAccessCheck(ac_logged_in)) {
		return;
	}

	CtdlGetSeen(buf, ctdlsetseen_seen);
	cprintf("%d %s\n", CIT_OK, buf);
}


/*
 * API function for cmd_invt_kick() and anything else that needs to
 * invite or kick out a user to/from a room.
 * 
 * Set iuser to the name of the user, and op to 1=invite or 0=kick
 */
int CtdlInvtKick(char *iuser, int op) {
	struct ctdluser USscratch;
	struct visit vbuf;
	char bbb[SIZ];

	if (getuser(&USscratch, iuser) != 0) {
		return(1);
	}

	CtdlGetRelationship(&vbuf, &USscratch, &CC->room);
	if (op == 1) {
		vbuf.v_flags = vbuf.v_flags & ~V_FORGET & ~V_LOCKOUT;
		vbuf.v_flags = vbuf.v_flags | V_ACCESS;
	}
	if (op == 0) {
		vbuf.v_flags = vbuf.v_flags & ~V_ACCESS;
		vbuf.v_flags = vbuf.v_flags | V_FORGET | V_LOCKOUT;
	}
	CtdlSetRelationship(&vbuf, &USscratch, &CC->room);

	/* post a message in Aide> saying what we just did */
	snprintf(bbb, sizeof bbb, "%s has been %s \"%s\" by %s.\n",
		iuser,
		((op == 1) ? "invited to" : "kicked out of"),
		CC->room.QRname,
		CC->user.fullname);
	aide_message(bbb);

	return(0);
}


/*
 * INVT and KICK commands
 */
void cmd_invt_kick(char *iuser, int op) {

	/*
	 * These commands are only allowed by aides, room aides,
	 * and room namespace owners
	 */
	if (is_room_aide()
	   || (atol(CC->room.QRname) == CC->user.usernum) ) {
		/* access granted */
	} else {
		/* access denied */
                cprintf("%d Higher access or room ownership required.\n",
                        ERROR + HIGHER_ACCESS_REQUIRED);
                return;
        }

	if (!strncasecmp(CC->room.QRname, config.c_baseroom,
			 ROOMNAMELEN)) {
		cprintf("%d Can't add/remove users from this room.\n",
			ERROR + NOT_HERE);
		return;
	}

	if (CtdlInvtKick(iuser, op) != 0) {
		cprintf("%d No such user.\n", ERROR + NO_SUCH_USER);
		return;
	}

	cprintf("%d %s %s %s.\n",
		CIT_OK, iuser,
		((op == 1) ? "invited to" : "kicked out of"),
		CC->room.QRname);
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
	   && ((CC->room.QRflags & QR_MAILBOX) == 0)  ) {
		return(1);
	}

	lgetuser(&CC->user, CC->curr_user);
	CtdlGetRelationship(&vbuf, &CC->user, &CC->room);

	vbuf.v_flags = vbuf.v_flags | V_FORGET;
	vbuf.v_flags = vbuf.v_flags & ~V_ACCESS;

	CtdlSetRelationship(&vbuf, &CC->user, &CC->room);
	lputuser(&CC->user);

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
		cprintf("%d You may not forget this room.\n", ERROR + NOT_HERE);
	}
}

/*
 * Get Next Unregistered User
 */
void cmd_gnur(void)
{
	struct cdbdata *cdbus;
	struct ctdluser usbuf;

	if (CtdlAccessCheck(ac_aide)) {
		return;
	}

	if ((CitControl.MMflags & MM_VALID) == 0) {
		cprintf("%d There are no unvalidated users.\n", CIT_OK);
		return;
	}

	/* There are unvalidated users.  Traverse the user database,
	 * and return the first user we find that needs validation.
	 */
	cdb_rewind(CDB_USERS);
	while (cdbus = cdb_next_item(CDB_USERS), cdbus != NULL) {
		memset(&usbuf, 0, sizeof(struct ctdluser));
		memcpy(&usbuf, cdbus->ptr,
		       ((cdbus->len > sizeof(struct ctdluser)) ?
			sizeof(struct ctdluser) : cdbus->len));
		cdb_free(cdbus);
		if ((usbuf.flags & US_NEEDVALID)
		    && (usbuf.axlevel > 0)) {
			cprintf("%d %s\n", MORE_DATA, usbuf.fullname);
			cdb_close_cursor(CDB_USERS);
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
	char user[128];
	int newax;
	struct ctdluser userbuf;

	extract_token(user, v_args, 0, '|', sizeof user);
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
void ForEachUser(void (*CallBack) (struct ctdluser * EachUser, void *out_data),
		 void *in_data)
{
	struct ctdluser usbuf;
	struct cdbdata *cdbus;

	cdb_rewind(CDB_USERS);

	while (cdbus = cdb_next_item(CDB_USERS), cdbus != NULL) {
		memset(&usbuf, 0, sizeof(struct ctdluser));
		memcpy(&usbuf, cdbus->ptr,
		       ((cdbus->len > sizeof(struct ctdluser)) ?
			sizeof(struct ctdluser) : cdbus->len));
		cdb_free(cdbus);
		(*CallBack) (&usbuf, in_data);
	}
}


/*
 * List one user (this works with cmd_list)
 */
void ListThisUser(struct ctdluser *usbuf, void *data)
{
	char *searchstring;

	searchstring = (char *)data;
	if (bmstrcasestr(usbuf->fullname, searchstring) == NULL) {
		return;
	}

	if (usbuf->axlevel > 0) {
		if ((CC->user.axlevel >= 6)
		    || ((usbuf->flags & US_UNLISTED) == 0)
		    || ((CC->internal_pgm))) {
			cprintf("%s|%d|%ld|%ld|%ld|%ld|",
				usbuf->fullname,
				usbuf->axlevel,
				usbuf->usernum,
				(long)usbuf->lastcall,
				usbuf->timescalled,
				usbuf->posted);
			if (CC->user.axlevel >= 6)
				cprintf("%s", usbuf->password);
			cprintf("\n");
		}
	}
}

/* 
 *  List users (searchstring may be empty to list all users)
 */
void cmd_list(char *cmdbuf)
{
	char searchstring[256];
	extract_token(searchstring, cmdbuf, 0, '|', sizeof searchstring);
	striplt(searchstring);
	cprintf("%d \n", LISTING_FOLLOWS);
	ForEachUser(ListThisUser, (void *)searchstring );
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

	getuser(&CC->user, CC->curr_user);	/* no lock is needed here */
	if ((REGISCALL != 0) && ((CC->user.flags & US_REGIS) == 0))
		regis = 1;

	if (CC->user.axlevel >= 6) {
		get_control();
		if (CitControl.MMflags & MM_VALID)
			vali = 1;
	}

	/* check for mail */
	mail = InitialMailCheck();

	cprintf("%d %d|%d|%d|%s|\n", CIT_OK, mail, regis, vali, CC->cs_inet_email);
}


/*
 * check to see if a user exists
 */
void cmd_qusr(char *who)
{
	struct ctdluser usbuf;

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
	struct ctdluser usbuf;
	char requested_user[128];

	if (CtdlAccessCheck(ac_aide)) {
		return;
	}

	extract_token(requested_user, cmdbuf, 0, '|', sizeof requested_user);
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
	struct ctdluser usbuf;
	char requested_user[128];
	char notify[SIZ];
	int np;
	int newax;
	int deleted = 0;

	if (CtdlAccessCheck(ac_aide))
		return;

	extract_token(requested_user, cmdbuf, 0, '|', sizeof requested_user);
	if (lgetuser(&usbuf, requested_user) != 0) {
		cprintf("%d No such user.\n", ERROR + NO_SUCH_USER);
		return;
	}
	np = num_parms(cmdbuf);
	if (np > 1)
		extract_token(usbuf.password, cmdbuf, 1, '|', sizeof usbuf.password);
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

	if (deleted) {
		sprintf(notify, "User \"%s\" has been deleted by %s.\n",
			usbuf.fullname, CC->user.fullname);
		aide_message(notify);
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
		if (ptr->user.usernum == which_user) {
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
        struct ctdlroom mailbox;
        struct visit vbuf;
        struct cdbdata *cdbfr;
        long *msglist = NULL;
        int num_msgs = 0;

        MailboxName(mailboxname, sizeof mailboxname, &CC->user, MAILROOM);
        if (getroom(&mailbox, mailboxname) != 0)
                return (0);
        CtdlGetRelationship(&vbuf, &CC->user, &mailbox);

        cdbfr = cdb_fetch(CDB_MSGLISTS, &mailbox.QRnumber, sizeof(long));

        if (cdbfr != NULL) {
                msglist = malloc(cdbfr->len);
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
                free(msglist);

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

	CtdlGetRelationship(&vbuf, &CC->user, &CC->room);
	vbuf.v_view = requested_view;
	CtdlSetRelationship(&vbuf, &CC->user, &CC->room);
	
	cprintf("%d ok\n", CIT_OK);
}
