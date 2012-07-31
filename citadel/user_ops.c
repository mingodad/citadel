/* 
 * Server functions which perform operations on user objects.
 *
 * Copyright (c) 1987-2011 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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
#include <syslog.h>
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

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
#include <libcitadel.h>
#include "auth.h"
#include "citadel.h"
#include "server.h"
#include "database.h"
#include "sysdep_decls.h"
#include "support.h"
#include "room_ops.h"
#include "file_ops.h"
#include "control.h"
#include "msgbase.h"
#include "config.h"
#include "citserver.h"
#include "citadel_dirs.h"
#include "genstamp.h"
#include "threads.h"
#include "citadel_ldap.h"
#include "context.h"
#include "ctdl_module.h"
#include "user_ops.h"

/* These pipes are used to talk to the chkpwd daemon, which is forked during startup */
int chkpwd_write_pipe[2];
int chkpwd_read_pipe[2];



/*
 * getuser()  -  retrieve named user into supplied buffer.
 *	       returns 0 on success
 */
int getuser(struct ctdluser *usbuf, char name[])
{
	return CtdlGetUser(usbuf, name);
}


/*
 * CtdlGetUser()  -  retrieve named user into supplied buffer.
 *	       returns 0 on success
 */
int CtdlGetUserLen(struct ctdluser *usbuf, const char *name, long len)
{

	char usernamekey[USERNAME_SIZE];
	struct cdbdata *cdbus;

	if (usbuf != NULL) {
		memset(usbuf, 0, sizeof(struct ctdluser));
	}

	makeuserkey(usernamekey, name, len);
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


int CtdlGetUser(struct ctdluser *usbuf, char *name)
{
	return CtdlGetUserLen(usbuf, name, cutuserkey(name));
}


/*
 * CtdlGetUserLock()  -  same as getuser() but locks the record
 */
int CtdlGetUserLock(struct ctdluser *usbuf, char *name)
{
	int retcode;

	retcode = CtdlGetUser(usbuf, name);
	if (retcode == 0) {
		begin_critical_section(S_USERS);
	}
	return (retcode);
}


/*
 * lgetuser()  -  same as getuser() but locks the record
 */
int lgetuser(struct ctdluser *usbuf, char *name)
{
	return CtdlGetUserLock(usbuf, name);
}


/*
 * CtdlPutUser()  -  write user buffer into the correct place on disk
 */
void CtdlPutUser(struct ctdluser *usbuf)
{
	char usernamekey[USERNAME_SIZE];

	makeuserkey(usernamekey, 
		    usbuf->fullname, 
		    cutuserkey(usbuf->fullname));

	usbuf->version = REV_LEVEL;
	cdb_store(CDB_USERS,
		  usernamekey, strlen(usernamekey),
		  usbuf, sizeof(struct ctdluser));

}


/*
 * putuser()  -  write user buffer into the correct place on disk
 */
void putuser(struct ctdluser *usbuf)
{
	CtdlPutUser(usbuf);
}


/*
 * CtdlPutUserLock()  -  same as putuser() but locks the record
 */
void CtdlPutUserLock(struct ctdluser *usbuf)
{
	CtdlPutUser(usbuf);
	end_critical_section(S_USERS);
}


/*
 * lputuser()  -  same as putuser() but locks the record
 */
void lputuser(struct ctdluser *usbuf)
{
	CtdlPutUserLock(usbuf);
}


/*
 * rename_user()  -  this is tricky because the user's display name is the database key
 *
 * Returns 0 on success or nonzero if there was an error...
 *
 */
int rename_user(char *oldname, char *newname) {
	int retcode = RENAMEUSER_OK;
	struct ctdluser usbuf;

	char oldnamekey[USERNAME_SIZE];
	char newnamekey[USERNAME_SIZE];

	/* Create the database keys... */
	makeuserkey(oldnamekey, oldname, cutuserkey(oldname));
	makeuserkey(newnamekey, newname, cutuserkey(newname));

	/* Lock up and get going */
	begin_critical_section(S_USERS);

	/* We cannot rename a user who is currently logged in */
	if (CtdlIsUserLoggedIn(oldname)) {
		end_critical_section(S_USERS);
		return RENAMEUSER_LOGGED_IN;
	}

	if (CtdlGetUser(&usbuf, newname) == 0) {
		retcode = RENAMEUSER_ALREADY_EXISTS;
	}
	else {

		if (CtdlGetUser(&usbuf, oldname) != 0) {
			retcode = RENAMEUSER_NOT_FOUND;
		}

		else {		/* Sanity checks succeeded.  Now rename the user. */
			if (usbuf.usernum == 0)
			{
				CONM_syslog(LOG_DEBUG, "Can not rename user \"Citadel\".\n");
				retcode = RENAMEUSER_NOT_FOUND;
			} else {
				CON_syslog(LOG_DEBUG, "Renaming <%s> to <%s>\n", oldname, newname);
				cdb_delete(CDB_USERS, oldnamekey, strlen(oldnamekey));
				safestrncpy(usbuf.fullname, newname, sizeof usbuf.fullname);
				CtdlPutUser(&usbuf);
				cdb_store(CDB_USERSBYNUMBER, &usbuf.usernum, sizeof(long),
					usbuf.fullname, strlen(usbuf.fullname)+1 );

				retcode = RENAMEUSER_OK;
			}
		}
	
	}

	end_critical_section(S_USERS);
	return(retcode);
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
void put_visit(visit *newvisit)
{
	char IndexBuf[32];
	int IndexLen = 0;

	memset (IndexBuf, 0, sizeof (IndexBuf));
	/* Generate an index */
	IndexLen = GenerateRelationshipIndex(IndexBuf,
					     newvisit->v_roomnum,
					     newvisit->v_roomgen,
					     newvisit->v_usernum);

	/* Store the record */
	cdb_store(CDB_VISIT, IndexBuf, IndexLen,
		  newvisit, sizeof(visit)
	);
}




/*
 * Define a relationship between a user and a room
 */
void CtdlSetRelationship(visit *newvisit,
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
void CtdlGetRelationship(visit *vbuf,
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
	memset(vbuf, 0, sizeof(visit));

	cdbvisit = cdb_fetch(CDB_VISIT, IndexBuf, IndexLen);
	if (cdbvisit != NULL) {
		memcpy(vbuf, cdbvisit->ptr,
		       ((cdbvisit->len > sizeof(visit)) ?
			sizeof(visit) : cdbvisit->len));
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


void CtdlMailboxName(char *buf, size_t n, const struct ctdluser *who, const char *prefix)
{
	snprintf(buf, n, "%010ld.%s", who->usernum, prefix);
}


void MailboxName(char *buf, size_t n, const struct ctdluser *who, const char *prefix)
{
	snprintf(buf, n, "%010ld.%s", who->usernum, prefix);
}


/*
 * Is the user currently logged in an Admin?
 */
int is_aide(void)
{
	if (CC->user.axlevel >= AxAideU)
		return (1);
	else
		return (0);
}


/*
 * Is the user currently logged in an Admin *or* the room Admin for this room?
 */
int is_room_aide(void)
{

	if (!CC->logged_in) {
		return (0);
	}

	if ((CC->user.axlevel >= AxAideU)
	    || (CC->room.QRroomaide == CC->user.usernum)) {
		return (1);
	} else {
		return (0);
	}
}

/*
 * CtdlGetUserByNumber() -	get user by number
 * 			returns 0 if user was found
 *
 * Note: fetching a user this way requires one additional database operation.
 */
int CtdlGetUserByNumber(struct ctdluser *usbuf, long number)
{
	struct cdbdata *cdbun;
	int r;

	cdbun = cdb_fetch(CDB_USERSBYNUMBER, &number, sizeof(long));
	if (cdbun == NULL) {
		CON_syslog(LOG_INFO, "User %ld not found\n", number);
		return(-1);
	}

	CON_syslog(LOG_INFO, "User %ld maps to %s\n", number, cdbun->ptr);
	r = CtdlGetUser(usbuf, cdbun->ptr);
	cdb_free(cdbun);
	return(r);
}

/*
 * getuserbynumber() -	get user by number
 * 			returns 0 if user was found
 *
 * Note: fetching a user this way requires one additional database operation.
 */
int getuserbynumber(struct ctdluser *usbuf, long number)
{
	return CtdlGetUserByNumber(usbuf, number);
}



/*
 * Helper function for rebuild_usersbynumber()
 */
void rebuild_ubn_for_user(struct ctdluser *usbuf, void *data) {

	struct ubnlist {
		struct ubnlist *next;
		char username[USERNAME_SIZE];
		long usernum;
	};

	static struct ubnlist *u = NULL;
	struct ubnlist *ptr = NULL;

	/* Lazy programming here.  Call this function as a ForEachUser backend
	 * in order to queue up the room names, or call it with a null user
	 * to make it do the processing.
	 */
	if (usbuf != NULL) {
		ptr = (struct ubnlist *) malloc(sizeof (struct ubnlist));
		if (ptr == NULL) return;

		ptr->usernum = usbuf->usernum;
		safestrncpy(ptr->username, usbuf->fullname, sizeof ptr->username);
		ptr->next = u;
		u = ptr;
		return;
	}

	while (u != NULL) {
		CON_syslog(LOG_DEBUG, "Rebuilding usersbynumber index %10ld : %s\n",
			u->usernum, u->username);
		cdb_store(CDB_USERSBYNUMBER, &u->usernum, sizeof(long), u->username, strlen(u->username)+1);

		ptr = u;
		u = u->next;
		free(ptr);
	}
}



/*
 * Rebuild the users-by-number index
 */
void rebuild_usersbynumber(void) {
	cdb_trunc(CDB_USERSBYNUMBER);			/* delete the old indices */
	ForEachUser(rebuild_ubn_for_user, NULL);	/* enumerate the users */
	rebuild_ubn_for_user(NULL, NULL);		/* and index them */
}



/*
 * getuserbyuid()  -     get user by system uid (for PAM mode authentication)
 *		       returns 0 if user was found
 *
 * WARNING: don't use this function unless you absolutely have to.  It does
 *	  a sequential search and therefore is computationally expensive.
 */
int getuserbyuid(struct ctdluser *usbuf, uid_t number)
{
	struct cdbdata *cdbus;

	cdb_rewind(CDB_USERS);

	while (cdbus = cdb_next_item(CDB_USERS), cdbus != NULL) {
		memset(usbuf, 0, sizeof(struct ctdluser));
		memcpy(usbuf, cdbus->ptr,
		       ((cdbus->len > sizeof(struct ctdluser)) ?
			sizeof(struct ctdluser) : cdbus->len));
		cdb_free(cdbus);
		if (usbuf->uid == number) {
			cdb_close_cursor(CDB_USERS);
			return (0);
		}
	}
	return (-1);
}

/*
 * Back end for cmd_user() and its ilk
 *
 * NOTE: "authname" should only be used if we are attempting to use the "master user" feature
 */
int CtdlLoginExistingUser(char *authname, const char *trythisname)
{
	char username[SIZ];
	int found_user;
	long len;

	CON_syslog(LOG_DEBUG, "CtdlLoginExistingUser(%s, %s)\n", authname, trythisname);

	if ((CC->logged_in)) {
		return login_already_logged_in;
	}

	if (trythisname == NULL) return login_not_found;
	
	if (!strncasecmp(trythisname, "SYS_", 4))
	{
		CON_syslog(LOG_DEBUG, "System user \"%s\" is not allowed to log in.\n", trythisname);
		return login_not_found;
	}

	/* If a "master user" is defined, handle its authentication if specified */
	CC->is_master = 0;
	if (strlen(config.c_master_user) > 0) if (strlen(config.c_master_pass) > 0) if (authname) {
		if (!strcasecmp(authname, config.c_master_user)) {
			CC->is_master = 1;
		}
	}

	/* Continue attempting user validation... */
	safestrncpy(username, trythisname, sizeof (username));
	striplt(username);
	len = cutuserkey(username);

	if (IsEmptyStr(username)) {
		return login_not_found;
	}

	if (config.c_auth_mode == AUTHMODE_HOST) {

		/* host auth mode */

		struct passwd pd;
		struct passwd *tempPwdPtr;
		char pwdbuffer[256];
	
		CON_syslog(LOG_DEBUG, "asking host about <%s>\n", username);
#ifdef HAVE_GETPWNAM_R
#ifdef SOLARIS_GETPWUID
		CON_syslog(LOG_DEBUG, "Calling getpwnam_r()\n");
		tempPwdPtr = getpwnam_r(username, &pd, pwdbuffer, sizeof pwdbuffer);
#else // SOLARIS_GETPWUID
		CONM_syslog(LOG_DEBUG, "Calling getpwnam_r()\n");
		getpwnam_r(username, &pd, pwdbuffer, sizeof pwdbuffer, &tempPwdPtr);
#endif // SOLARIS_GETPWUID
#else // HAVE_GETPWNAM_R
		CON_syslog(LOG_DEBUG, "SHOULD NEVER GET HERE!!!\n");
		tempPwdPtr = NULL;
#endif // HAVE_GETPWNAM_R
		if (tempPwdPtr == NULL) {
			CON_syslog(LOG_DEBUG, "no such user <%s>\n", username);
			return login_not_found;
		}
	
		/* Locate the associated Citadel account.
		 * If not found, make one attempt to create it.
		 */
		found_user = getuserbyuid(&CC->user, pd.pw_uid);
		CON_syslog(LOG_DEBUG, "found it: uid=%ld, gecos=%s here: %d\n",
			(long)pd.pw_uid, pd.pw_gecos, found_user);
		if (found_user != 0) {
			len = cutuserkey(username);
			create_user(username, len, 0);
			found_user = getuserbyuid(&CC->user, pd.pw_uid);
		}

	}

#ifdef HAVE_LDAP
	else if ((config.c_auth_mode == AUTHMODE_LDAP) || (config.c_auth_mode == AUTHMODE_LDAP_AD)) {
	
		/* LDAP auth mode */

		uid_t ldap_uid;
		char ldap_cn[256];
		char ldap_dn[256];

		found_user = CtdlTryUserLDAP(username, ldap_dn, sizeof ldap_dn, ldap_cn, sizeof ldap_cn, &ldap_uid);
		if (found_user != 0) {
			return login_not_found;
		}

		found_user = getuserbyuid(&CC->user, ldap_uid);
		if (found_user != 0) {
			create_user(username, len, 0);
			found_user = getuserbyuid(&CC->user, ldap_uid);
		}

		if (found_user == 0) {
			if (CC->ldap_dn != NULL) free(CC->ldap_dn);
			CC->ldap_dn = strdup(ldap_dn);
		}

	}
#endif

	else {
		/* native auth mode */

		struct recptypes *valid = NULL;
	
		/* First, try to log in as if the supplied name is a display name */
		found_user = CtdlGetUser(&CC->user, username);
	
		/* If that didn't work, try to log in as if the supplied name
	 	* is an e-mail address
	 	*/
		if (found_user != 0) {
			valid = validate_recipients(username, NULL, 0);
			if (valid != NULL) {
				if (valid->num_local == 1) {
					found_user = CtdlGetUser(&CC->user, valid->recp_local);
				}
				free_recipients(valid);
			}
		}
	}

	/* Did we find something? */
	if (found_user == 0) {
		if (((CC->nologin)) && (CC->user.axlevel < AxAideU)) {
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

	CON_syslog(LOG_DEBUG, "cmd_user(%s)\n", cmdbuf);
	extract_token(username, cmdbuf, 0, '|', sizeof username);
	CON_syslog(LOG_DEBUG, "username: %s\n", username);
	striplt(username);
	CON_syslog(LOG_DEBUG, "username: %s\n", username);

	a = CtdlLoginExistingUser(NULL, username);
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
void do_login(void)
{
	struct CitContext *CCC = CC;

	CCC->logged_in = 1;
	CON_syslog(LOG_NOTICE, "<%s> logged in\n", CCC->curr_user);

	CtdlGetUserLock(&CCC->user, CCC->curr_user);
	++(CCC->user.timescalled);
	CCC->previous_login = CCC->user.lastcall;
	time(&CCC->user.lastcall);

	/* If this user's name is the name of the system administrator
	 * (as specified in setup), automatically assign access level 6.
	 */
	if (!strcasecmp(CCC->user.fullname, config.c_sysadm)) {
		CCC->user.axlevel = AxAideU;
	}

	/* If we're authenticating off the host system, automatically give
	 * root the highest level of access.
	 */
	if (config.c_auth_mode == AUTHMODE_HOST) {
		if (CCC->user.uid == 0) {
			CCC->user.axlevel = AxAideU;
		}
	}

	CtdlPutUserLock(&CCC->user);

	/*
	 * Populate CCC->cs_inet_email with a default address.  This will be
	 * overwritten with the user's directory address, if one exists, when
	 * the vCard module's login hook runs.
	 */
	snprintf(CCC->cs_inet_email, sizeof CCC->cs_inet_email, "%s@%s",
		CCC->user.fullname, config.c_fqdn);
	convert_spaces_to_underscores(CCC->cs_inet_email);

	/* Create any personal rooms required by the system.
	 * (Technically, MAILROOM should be there already, but just in case...)
	 */
	CtdlCreateRoom(MAILROOM, 4, "", 0, 1, 0, VIEW_MAILBOX);
	CtdlCreateRoom(SENTITEMS, 4, "", 0, 1, 0, VIEW_MAILBOX);
	CtdlCreateRoom(USERTRASHROOM, 4, "", 0, 1, 0, VIEW_MAILBOX);
	CtdlCreateRoom(USERDRAFTROOM, 4, "", 0, 1, 0, VIEW_MAILBOX);

	/* Run any startup routines registered by loadable modules */
	PerformSessionHooks(EVT_LOGIN);

	/* Enter the lobby */
	CtdlUserGoto(config.c_baseroom, 0, 0, NULL, NULL);
}


void logged_in_response(void)
{
	cprintf("%d %s|%d|%ld|%ld|%u|%ld|%ld\n",
		CIT_OK, CC->user.fullname, CC->user.axlevel,
		CC->user.timescalled, CC->user.posted,
		CC->user.flags, CC->user.usernum,
		CC->previous_login);
}



void CtdlUserLogout(void)
{
	CitContext *CCC = MyContext();

	CON_syslog(LOG_DEBUG, "CtdlUserLogout() logging out <%s> from session %d",
		   CCC->curr_user, CCC->cs_pid
	);

	/*
	 * If there is a download in progress, abort it.
	 */
	if (CCC->download_fp != NULL) {
		fclose(CCC->download_fp);
		CCC->download_fp = NULL;
	}

	/*
	 * If there is an upload in progress, abort it.
	 */
	if (CCC->upload_fp != NULL) {
		abort_upl(CCC);
	}

	/* Run any hooks registered by modules... */
	PerformSessionHooks(EVT_LOGOUT);
	
	/*
	 * Clear out some session data.  Most likely, the CitContext for this
	 * session is about to get nuked when the session disconnects, but
	 * since it's possible to log in again without reconnecting, we cannot
	 * make that assumption.
	 */
	strcpy(CCC->fake_username, "");
	strcpy(CCC->fake_hostname, "");
	strcpy(CCC->fake_roomname, "");
	CCC->logged_in = 0;

	/* Check to see if the user was deleted whilst logged in and purge them if necessary */
	if ((CCC->user.axlevel == AxDeleted) && (CCC->user.usernum))
		purge_user(CCC->user.fullname);

	/* Clear out the user record in memory so we don't behave like a ghost */
	memset(&CCC->user, 0, sizeof(struct ctdluser));
	CCC->curr_user[0] = 0;
	CCC->is_master = 0;
	CCC->cs_inet_email[0] = 0;
	CCC->cs_inet_other_emails[0] = 0;
	CCC->cs_inet_fn[0] = 0;
	CCC->fake_username[0] = 0;
	CCC->fake_hostname[0] = 0;
	CCC->fake_roomname[0] = 0;
	

	/* Free any output buffers */
	unbuffer_output();
}


/*
 * Validate a password on the host unix system by talking to the chkpwd daemon
 */
static int validpw(uid_t uid, const char *pass)
{
	char buf[256];
	int rv = 0;

	if (IsEmptyStr(pass)) {
		CON_syslog(LOG_DEBUG, "Refusing to chkpwd for uid=%d with empty password.\n", uid);
		return 0;
	}

	CON_syslog(LOG_DEBUG, "Validating password for uid=%d using chkpwd...\n", uid);

	begin_critical_section(S_CHKPWD);
	rv = write(chkpwd_write_pipe[1], &uid, sizeof(uid_t));
	if (rv == -1) {
		CON_syslog(LOG_EMERG, "Communicatino with chkpwd broken: %s\n", strerror(errno));
		end_critical_section(S_CHKPWD);
		return 0;
	}
	rv = write(chkpwd_write_pipe[1], pass, 256);
	if (rv == -1) {
		CON_syslog(LOG_EMERG, "Communicatino with chkpwd broken: %s\n", strerror(errno));
		end_critical_section(S_CHKPWD);
		return 0;
	}
	rv = read(chkpwd_read_pipe[0], buf, 4);
	if (rv == -1) {
		CON_syslog(LOG_EMERG, "Communicatino with chkpwd broken: %s\n", strerror(errno));
		end_critical_section(S_CHKPWD);
		return 0;
	}
	end_critical_section(S_CHKPWD);

	if (!strncmp(buf, "PASS", 4)) {
		CONM_syslog(LOG_DEBUG, "...pass\n");
		return(1);
	}

	CONM_syslog(LOG_DEBUG, "...fail\n");
	return 0;
}

/* 
 * Start up the chkpwd daemon so validpw() has something to talk to
 */
void start_chkpwd_daemon(void) {
	pid_t chkpwd_pid;
	struct stat filestats;
	int i;

	CONM_syslog(LOG_DEBUG, "Starting chkpwd daemon for host authentication mode\n");

	if ((stat(file_chkpwd, &filestats)==-1) ||
	    (filestats.st_size==0)){
		printf("didn't find chkpwd daemon in %s: %s\n", file_chkpwd, strerror(errno));
		abort();
	}
	if (pipe(chkpwd_write_pipe) != 0) {
		CON_syslog(LOG_EMERG, "Unable to create pipe for chkpwd daemon: %s\n", strerror(errno));
		abort();
	}
	if (pipe(chkpwd_read_pipe) != 0) {
		CON_syslog(LOG_EMERG, "Unable to create pipe for chkpwd daemon: %s\n", strerror(errno));
		abort();
	}

	chkpwd_pid = fork();
	if (chkpwd_pid < 0) {
		CON_syslog(LOG_EMERG, "Unable to fork chkpwd daemon: %s\n", strerror(errno));
		abort();
	}
	if (chkpwd_pid == 0) {
		CONM_syslog(LOG_DEBUG, "Now calling dup2() write\n");
		dup2(chkpwd_write_pipe[0], 0);
		CONM_syslog(LOG_DEBUG, "Now calling dup2() write\n");
		dup2(chkpwd_read_pipe[1], 1);
		CONM_syslog(LOG_DEBUG, "Now closing stuff\n");
		for (i=2; i<256; ++i) close(i);
		CON_syslog(LOG_DEBUG, "Now calling execl(%s)\n", file_chkpwd);
		execl(file_chkpwd, file_chkpwd, NULL);
		CON_syslog(LOG_EMERG, "Unable to exec chkpwd daemon: %s\n", strerror(errno));
		abort();
		exit(errno);
	}
}


int CtdlTryPassword(const char *password, long len)
{
	int code;
	CitContext *CCC = CC;

	if ((CCC->logged_in)) {
		CONM_syslog(LOG_WARNING, "CtdlTryPassword: already logged in\n");
		return pass_already_logged_in;
	}
	if (!strcmp(CCC->curr_user, NLI)) {
		CONM_syslog(LOG_WARNING, "CtdlTryPassword: no user selected\n");
		return pass_no_user;
	}
	if (CtdlGetUser(&CCC->user, CCC->curr_user)) {
		CONM_syslog(LOG_ERR, "CtdlTryPassword: internal error\n");
		return pass_internal_error;
	}
	if (password == NULL) {
		CONM_syslog(LOG_INFO, "CtdlTryPassword: NULL password string supplied\n");
		return pass_wrong_password;
	}

	if (CCC->is_master) {
		code = strcmp(password, config.c_master_pass);
	}

	else if (config.c_auth_mode == AUTHMODE_HOST) {

		/* host auth mode */

		if (validpw(CCC->user.uid, password)) {
			code = 0;

			/*
			 * sooper-seekrit hack: populate the password field in the
			 * citadel database with the password that the user typed,
			 * if it's correct.  This allows most sites to convert from
			 * host auth to native auth if they want to.  If you think
			 * this is a security hazard, comment it out.
			 */

			CtdlGetUserLock(&CCC->user, CCC->curr_user);
			safestrncpy(CCC->user.password, password, sizeof CCC->user.password);
			CtdlPutUserLock(&CCC->user);

			/*
			 * (sooper-seekrit hack ends here)
			 */

		}
		else {
			code = (-1);
		}
	}

#ifdef HAVE_LDAP
	else if ((config.c_auth_mode == AUTHMODE_LDAP) || (config.c_auth_mode == AUTHMODE_LDAP_AD)) {

		/* LDAP auth mode */

		if ((CCC->ldap_dn) && (!CtdlTryPasswordLDAP(CCC->ldap_dn, password))) {
			code = 0;
		}
		else {
			code = (-1);
		}
	}
#endif

	else {

		/* native auth mode */
		char *pw;

		pw = (char*) malloc(len + 1);
		memcpy(pw, password, len + 1);
		strproc(pw);
		strproc(CCC->user.password);
		code = strcasecmp(CCC->user.password, pw);
		if (code != 0) {
			strproc(pw);
			strproc(CCC->user.password);
			code = strcasecmp(CCC->user.password, pw);
		}
		free (pw);
	}

	if (!code) {
		do_login();
		return pass_ok;
	} else {
		CON_syslog(LOG_WARNING, "Bad password specified for <%s> Service <%s> Port <%ld> Remote <%s / %s>\n",
			   CCC->curr_user,
			   CCC->ServiceName,
			   CCC->tcp_port,
			   CCC->cs_host,
			   CCC->cs_addr);


//citserver[5610]: Bad password specified for <willi> Service <citadel-TCP> Remote <PotzBlitz / >

		return pass_wrong_password;
	}
}


void cmd_pass(char *buf)
{
	char password[SIZ];
	int a;
	long len;

	memset(password, 0, sizeof(password));
	len = extract_token(password, buf, 0, '|', sizeof password);
	a = CtdlTryPassword(password, len);

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

	makeuserkey(usernamekey, pname, cutuserkey(pname));

	/* If the name is empty we can't find them in the DB any way so just return */
	if (IsEmptyStr(pname))
		return (ERROR + NO_SUCH_USER);

	if (CtdlGetUser(&usbuf, pname) != 0) {
		CON_syslog(LOG_ERR, "Cannot purge user <%s> - not found\n", pname);
		return (ERROR + NO_SUCH_USER);
	}
	/* Don't delete a user who is currently logged in.  Instead, just
	 * set the access level to 0, and let the account get swept up
	 * during the next purge.
	 */
	if (CtdlIsUserLoggedInByNum(usbuf.usernum)) {
		CON_syslog(LOG_WARNING, "User <%s> is logged in; not deleting.\n", pname);
		usbuf.axlevel = AxDeleted;
		CtdlPutUser(&usbuf);
		return (1);
	}
	CON_syslog(LOG_NOTICE, "Deleting user <%s>\n", pname);

/*
 * FIXME:
 * This should all be wrapped in a S_USERS mutex.
 * Without the mutex the user could log in before we get to the next function
 * That would truly mess things up :-(
 * I would like to see the S_USERS start before the CtdlIsUserLoggedInByNum() above
 * and end after the user has been deleted from the database, below.
 * Question is should we enter the EVT_PURGEUSER whilst S_USERS is active?
 */

	/* Perform any purge functions registered by server extensions */
	PerformUserHooks(&usbuf, EVT_PURGEUSER);

	/* delete any existing user/room relationships */
	cdb_delete(CDB_VISIT, &usbuf.usernum, sizeof(long));

	/* delete the users-by-number index record */
	cdb_delete(CDB_USERSBYNUMBER, &usbuf.usernum, sizeof(long));

	/* delete the userlog entry */
	cdb_delete(CDB_USERS, usernamekey, strlen(usernamekey));

	/* remove the user's bio file */
	snprintf(filename, 
			 sizeof filename, 
			 "%s/%ld",
			 ctdl_bio_dir,
			 usbuf.usernum);
	unlink(filename);

	/* remove the user's picture */
	snprintf(filename, 
			 sizeof filename, 
			 "%s/%ld.gif",
			 ctdl_image_dir,
			 usbuf.usernum);
	unlink(filename);

	return (0);
}


int internal_create_user (const char *username, long len, struct ctdluser *usbuf, uid_t uid)
{
	if (!CtdlGetUserLen(usbuf, username, len)) {
		return (ERROR + ALREADY_EXISTS);
	}

	/* Go ahead and initialize a new user record */
	memset(usbuf, 0, sizeof(struct ctdluser));
	safestrncpy(usbuf->fullname, username, sizeof usbuf->fullname);
	strcpy(usbuf->password, "");
	usbuf->uid = uid;

	/* These are the default flags on new accounts */
	usbuf->flags = US_LASTOLD | US_DISAPPEAR | US_PAGINATOR | US_FLOORS;

	usbuf->timescalled = 0;
	usbuf->posted = 0;
	usbuf->axlevel = config.c_initax;
	usbuf->lastcall = time(NULL);

	/* fetch a new user number */
	usbuf->usernum = get_new_user_number();

	/* add user to the database */
	CtdlPutUser(usbuf);
	cdb_store(CDB_USERSBYNUMBER, &usbuf->usernum, sizeof(long), usbuf->fullname, strlen(usbuf->fullname)+1);

	return 0;
}



/*
 * create_user()  -  back end processing to create a new user
 *
 * Set 'newusername' to the desired account name.
 * Set 'become_user' to nonzero if this is self-service account creation and we want
 * to actually log in as the user we just created, otherwise set it to 0.
 */
int create_user(const char *newusername, long len, int become_user)
{
	struct ctdluser usbuf;
	struct ctdlroom qrbuf;
	char username[256];
	char mailboxname[ROOMNAMELEN];
	char buf[SIZ];
	int retval;
	uid_t uid = (-1);
	

	safestrncpy(username, newusername, sizeof username);
	strproc(username);

	
	if (config.c_auth_mode == AUTHMODE_HOST) {

		/* host auth mode */

		struct passwd pd;
		struct passwd *tempPwdPtr;
		char pwdbuffer[SIZ];
	
#ifdef HAVE_GETPWNAM_R
#ifdef SOLARIS_GETPWUID
		tempPwdPtr = getpwnam_r(username, &pd, pwdbuffer, sizeof(pwdbuffer));
#else // SOLARIS_GETPWUID
		getpwnam_r(username, &pd, pwdbuffer, sizeof pwdbuffer, &tempPwdPtr);
#endif // SOLARIS_GETPWUID
#else // HAVE_GETPWNAM_R
		tempPwdPtr = NULL;
#endif // HAVE_GETPWNAM_R
		if (tempPwdPtr != NULL) {
			extract_token(username, pd.pw_gecos, 0, ',', sizeof username);
			uid = pd.pw_uid;
			if (IsEmptyStr (username))
			{
				safestrncpy(username, pd.pw_name, sizeof username);
				len = cutuserkey(username);
			}
		}
		else {
			return (ERROR + NO_SUCH_USER);
		}
	}

#ifdef HAVE_LDAP
	if ((config.c_auth_mode == AUTHMODE_LDAP) || (config.c_auth_mode == AUTHMODE_LDAP_AD)) {
		if (CtdlTryUserLDAP(username, NULL, 0, username, sizeof username, &uid) != 0) {
			return(ERROR + NO_SUCH_USER);
		}
	}
#endif /* HAVE_LDAP */
	
	if ((retval = internal_create_user(username, len, &usbuf, uid)) != 0)
		return retval;
	
	/*
	 * Give the user a private mailbox and a configuration room.
	 * Make the latter an invisible system room.
	 */
	CtdlMailboxName(mailboxname, sizeof mailboxname, &usbuf, MAILROOM);
	CtdlCreateRoom(mailboxname, 5, "", 0, 1, 1, VIEW_MAILBOX);

	CtdlMailboxName(mailboxname, sizeof mailboxname, &usbuf, USERCONFIGROOM);
	CtdlCreateRoom(mailboxname, 5, "", 0, 1, 1, VIEW_BBS);
	if (CtdlGetRoomLock(&qrbuf, mailboxname) == 0) {
		qrbuf.QRflags2 |= QR2_SYSTEM;
		CtdlPutRoomLock(&qrbuf);
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
		do_login();
	
		/* Check to make sure we're still who we think we are */
		if (CtdlGetUser(&CC->user, CC->curr_user)) {
			return (ERROR + INTERNAL_ERROR);
		}
	}
	
	snprintf(buf, SIZ, 
		"New user account <%s> has been created, from host %s [%s].\n",
		username,
		CC->cs_host,
		CC->cs_addr
	);
	CtdlAideMessage(buf, "User Creation Notice");
	CON_syslog(LOG_NOTICE, "New user <%s> created\n", username);
	return (0);
}



/*
 * cmd_newu()  -  create a new user account and log in as that user
 */
void cmd_newu(char *cmdbuf)
{
	int a;
	long len;
	char username[SIZ];

	if (config.c_auth_mode != AUTHMODE_NATIVE) {
		cprintf("%d This system does not use native mode authentication.\n",
			ERROR + NOT_HERE);
		return;
	}

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
	strproc(username);
	len = cutuserkey(username);

	if (IsEmptyStr(username)) {
		cprintf("%d You must supply a user name.\n", ERROR + USERNAME_REQUIRED);
		return;
	}

	if ((!strcasecmp(username, "bbs")) ||
	    (!strcasecmp(username, "new")) ||
	    (!strcasecmp(username, "."))) {
		cprintf("%d '%s' is an invalid login name.\n", ERROR + ILLEGAL_VALUE, username);
		return;
	}

	a = create_user(username, len, 1);

	if (a == 0) {
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
 * set password - back end api code
 */
void CtdlSetPassword(char *new_pw)
{
	CtdlGetUserLock(&CC->user, CC->curr_user);
	safestrncpy(CC->user.password, new_pw, sizeof(CC->user.password));
	CtdlPutUserLock(&CC->user);
	CON_syslog(LOG_INFO, "Password changed for user <%s>\n", CC->curr_user);
	PerformSessionHooks(EVT_SETPASS);
}


/*
 * set password - citadel protocol implementation
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
	if (CC->is_master) {
		cprintf("%d The master prefix password cannot be changed with this command.\n",
			ERROR + NOT_HERE);
		return;
	}

	if (!strcasecmp(new_pw, "GENERATE_RANDOM_PASSWORD")) {
		char random_password[17];
		snprintf(random_password, sizeof random_password, "%08lx%08lx", random(), random());
		CtdlSetPassword(random_password);
		cprintf("%d %s\n", CIT_OK, random_password);
	}
	else {
		strproc(new_pw);
		if (IsEmptyStr(new_pw)) {
			cprintf("%d Password unchanged.\n", CIT_OK);
			return;
		}
		CtdlSetPassword(new_pw);
		cprintf("%d Password changed.\n", CIT_OK);
	}
}


/*
 * cmd_creu() - administratively create a new user account (do not log in to it)
 */
void cmd_creu(char *cmdbuf)
{
	int a;
	long len;
	char username[SIZ];
	char password[SIZ];
	struct ctdluser tmp;

	if (CtdlAccessCheck(ac_aide)) {
		return;
	}

	extract_token(username, cmdbuf, 0, '|', sizeof username);
	strproc(username);
	strproc(password);
	if (IsEmptyStr(username)) {
		cprintf("%d You must supply a user name.\n", ERROR + USERNAME_REQUIRED);
		return;
	}
	len = cutuserkey(username);


	extract_token(password, cmdbuf, 1, '|', sizeof password);

	a = create_user(username, len, 0);

	if (a == 0) {
		if (!IsEmptyStr(password)) {
			CtdlGetUserLock(&tmp, username);
			safestrncpy(tmp.password, password, sizeof(tmp.password));
			CtdlPutUserLock(&tmp);
		}
		cprintf("%d User '%s' created %s.\n", CIT_OK, username,
				(!IsEmptyStr(password)) ? "and password set" :
				"with no password");
		return;
	} else if (a == ERROR + ALREADY_EXISTS) {
		cprintf("%d '%s' already exists.\n", ERROR + ALREADY_EXISTS, username);
		return;
	} else if ( (config.c_auth_mode != AUTHMODE_NATIVE) && (a == ERROR + NO_SUCH_USER) ) {
		cprintf("%d User accounts are not created within Citadel in host authentication mode.\n",
			ERROR + NO_SUCH_USER);
		return;
	} else {
		cprintf("%d An error occurred creating the user account.\n", ERROR + INTERNAL_ERROR);
	}
}



/*
 * get user parameters
 */
void cmd_getu(char *cmdbuf)
{

	if (CtdlAccessCheck(ac_logged_in))
		return;

	CtdlGetUser(&CC->user, CC->curr_user);
	cprintf("%d 80|24|%d|\n",
		CIT_OK,
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
	CtdlGetUserLock(&CC->user, CC->curr_user);
	CC->user.flags = CC->user.flags & (~US_USER_SET);
	CC->user.flags = CC->user.flags | (extract_int(new_parms, 2) & US_USER_SET);
	CtdlPutUserLock(&CC->user);
	cprintf("%d Ok\n", CIT_OK);
}

/*
 * set last read pointer
 */
void cmd_slrp(char *new_ptr)
{
	long newlr;
	visit vbuf;
	visit original_vbuf;

	if (CtdlAccessCheck(ac_logged_in)) {
		return;
	}

	if (!strncasecmp(new_ptr, "highest", 7)) {
		newlr = CC->room.QRhighest;
	} else {
		newlr = atol(new_ptr);
	}

	CtdlGetUserLock(&CC->user, CC->curr_user);

	CtdlGetRelationship(&vbuf, &CC->user, &CC->room);
	memcpy(&original_vbuf, &vbuf, sizeof(visit));
	vbuf.v_lastseen = newlr;
	snprintf(vbuf.v_seen, sizeof vbuf.v_seen, "*:%ld", newlr);

	/* Only rewrite the record if it changed */
	if ( (vbuf.v_lastseen != original_vbuf.v_lastseen)
	   || (strcmp(vbuf.v_seen, original_vbuf.v_seen)) ) {
		CtdlSetRelationship(&vbuf, &CC->user, &CC->room);
	}

	CtdlPutUserLock(&CC->user);
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
	visit vbuf;

	if (CtdlAccessCheck(ac_logged_in)) {
		return;
	}

	/* Learn about the user and room in question */
	CtdlGetRelationship(&vbuf, &CC->user, &CC->room);

	cprintf("%d ", CIT_OK);
	client_write(vbuf.v_seen, strlen(vbuf.v_seen));
	client_write(HKEY("\n"));
}


/*
 * API function for cmd_invt_kick() and anything else that needs to
 * invite or kick out a user to/from a room.
 * 
 * Set iuser to the name of the user, and op to 1=invite or 0=kick
 */
int CtdlInvtKick(char *iuser, int op) {
	struct ctdluser USscratch;
	visit vbuf;
	char bbb[SIZ];

	if (CtdlGetUser(&USscratch, iuser) != 0) {
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
		(CC->logged_in ? CC->user.fullname : "an administrator")
	);
	CtdlAideMessage(bbb,"User Admin Message");

	return(0);
}


/*
 * INVT and KICK commands
 */
void cmd_invt_kick(char *iuser, int op) {

	/*
	 * These commands are only allowed by admins, room admins,
	 * and room namespace owners
	 */
	if (is_room_aide()) {
		/* access granted */
	} else if ( ((atol(CC->room.QRname) == CC->user.usernum) ) && (CC->user.usernum != 0) ) {
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

void cmd_invt(char *iuser) {cmd_invt_kick(iuser, 1);}
void cmd_kick(char *iuser) {cmd_invt_kick(iuser, 0);}

/*
 * Forget (Zap) the current room (API call)
 * Returns 0 on success
 */
int CtdlForgetThisRoom(void) {
	visit vbuf;

	/* On some systems, Admins are not allowed to forget rooms */
	if (is_aide() && (config.c_aide_zap == 0)
	   && ((CC->room.QRflags & QR_MAILBOX) == 0)  ) {
		return(1);
	}

	CtdlGetUserLock(&CC->user, CC->curr_user);
	CtdlGetRelationship(&vbuf, &CC->user, &CC->room);

	vbuf.v_flags = vbuf.v_flags | V_FORGET;
	vbuf.v_flags = vbuf.v_flags & ~V_ACCESS;

	CtdlSetRelationship(&vbuf, &CC->user, &CC->room);
	CtdlPutUserLock(&CC->user);

	/* Return to the Lobby, so we don't end up in an undefined room */
	CtdlUserGoto(config.c_baseroom, 0, 0, NULL, NULL);
	return(0);

}


/*
 * forget (Zap) the current room
 */
void cmd_forg(char *argbuf)
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
void cmd_gnur(char *argbuf)
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
		    && (usbuf.axlevel > AxDeleted)) {
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

	if (CtdlAccessCheck(ac_aide) || 
	    (newax > AxAideU) ||
	    (newax < AxDeleted)) {
		return;
	}

	if (CtdlGetUserLock(&userbuf, user) != 0) {
		cprintf("%d '%s' not found.\n", ERROR + NO_SUCH_USER, user);
		return;
	}

	userbuf.axlevel = newax;
	userbuf.flags = (userbuf.flags & ~US_NEEDVALID);

	CtdlPutUserLock(&userbuf);

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

	if (usbuf->axlevel > AxDeleted) {
		if ((CC->user.axlevel >= AxAideU)
		    || ((usbuf->flags & US_UNLISTED) == 0)
		    || ((CC->internal_pgm))) {
			cprintf("%s|%d|%ld|%ld|%ld|%ld||\n",
				usbuf->fullname,
				usbuf->axlevel,
				usbuf->usernum,
				(long)usbuf->lastcall,
				usbuf->timescalled,
				usbuf->posted);
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
void cmd_chek(char *argbuf)
{
	int mail = 0;
	int regis = 0;
	int vali = 0;

	if (CtdlAccessCheck(ac_logged_in)) {
		return;
	}

	CtdlGetUser(&CC->user, CC->curr_user);	/* no lock is needed here */
	if ((REGISCALL != 0) && ((CC->user.flags & US_REGIS) == 0))
		regis = 1;

	if (CC->user.axlevel >= AxAideU) {
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

	if (CtdlGetUser(&usbuf, who) == 0) {
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
	if (CtdlGetUser(&usbuf, requested_user) != 0) {
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
	if (CtdlGetUserLock(&usbuf, requested_user) != 0) {
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
		if ((newax >= AxDeleted) && (newax <= AxAideU)) {
			usbuf.axlevel = newax;
		}
	}
	if (np > 7) {
		usbuf.lastcall = extract_long(cmdbuf, 7);
	}
	if (np > 8) {
		usbuf.USuserpurge = extract_int(cmdbuf, 8);
	}
	CtdlPutUserLock(&usbuf);
	if (usbuf.axlevel == AxDeleted) {
		if (purge_user(requested_user) == 0) {
			deleted = 1;
		}
	}

	if (deleted) {
		snprintf(notify, SIZ, 
			 "User \"%s\" has been deleted by %s.\n",
			 usbuf.fullname,
			(CC->logged_in ? CC->user.fullname : "an administrator")
		);
		CtdlAideMessage(notify, "User Deletion Message");
	}

	cprintf("%d Ok", CIT_OK);
	if (deleted)
		cprintf(" (%s deleted)", requested_user);
	cprintf("\n");
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
	visit vbuf;
	struct cdbdata *cdbfr;
	long *msglist = NULL;
	int num_msgs = 0;

	CtdlMailboxName(mailboxname, sizeof mailboxname, &CC->user, MAILROOM);
	if (CtdlGetRoom(&mailbox, mailboxname) != 0)
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
	visit vbuf;

	if (CtdlAccessCheck(ac_logged_in)) {
		return;
	}

	requested_view = extract_int(cmdbuf, 0);

	CtdlGetRelationship(&vbuf, &CC->user, &CC->room);
	vbuf.v_view = requested_view;
	CtdlSetRelationship(&vbuf, &CC->user, &CC->room);
	
	cprintf("%d ok\n", CIT_OK);
}


/*
 * Rename a user
 */
void cmd_renu(char *cmdbuf)
{
	int retcode;
	char oldname[USERNAME_SIZE];
	char newname[USERNAME_SIZE];

	if (CtdlAccessCheck(ac_aide)) {
		return;
	}

	extract_token(oldname, cmdbuf, 0, '|', sizeof oldname);
	extract_token(newname, cmdbuf, 1, '|', sizeof newname);

	retcode = rename_user(oldname, newname);
	switch(retcode) {
		case RENAMEUSER_OK:
			cprintf("%d '%s' has been renamed to '%s'.\n", CIT_OK, oldname, newname);
			return;
		case RENAMEUSER_LOGGED_IN:
			cprintf("%d '%s' is currently logged in and cannot be renamed.\n",
				ERROR + ALREADY_LOGGED_IN , oldname);
			return;
		case RENAMEUSER_NOT_FOUND:
			cprintf("%d '%s' does not exist.\n", ERROR + NO_SUCH_USER, oldname);
			return;
		case RENAMEUSER_ALREADY_EXISTS:
			cprintf("%d A user named '%s' already exists.\n", ERROR + ALREADY_EXISTS, newname);
			return;
	}

	cprintf("%d An unknown error occurred.\n", ERROR);
}



/*****************************************************************************/
/*                      MODULE INITIALIZATION STUFF                          */
/*****************************************************************************/


CTDL_MODULE_INIT(user_ops)
{
	if (!threading) {
		CtdlRegisterProtoHook(cmd_user, "USER", "Submit username for login");
		CtdlRegisterProtoHook(cmd_pass, "PASS", "Complete login by submitting a password");
		CtdlRegisterProtoHook(cmd_creu, "CREU", "Create User");
		CtdlRegisterProtoHook(cmd_setp, "SETP", "Set the password for an account");
		CtdlRegisterProtoHook(cmd_getu, "GETU", "Get User parameters");
		CtdlRegisterProtoHook(cmd_setu, "SETU", "Set User parameters");
		CtdlRegisterProtoHook(cmd_slrp, "SLRP", "Set Last Read Pointer");
		CtdlRegisterProtoHook(cmd_invt, "INVT", "Invite a user to a room");
		CtdlRegisterProtoHook(cmd_kick, "KICK", "Kick a user out of a room");
		CtdlRegisterProtoHook(cmd_forg, "FORG", "Forget a room");
		CtdlRegisterProtoHook(cmd_gnur, "GNUR", "Get Next Unregistered User");
		CtdlRegisterProtoHook(cmd_vali, "VALI", "Validate new users");
		CtdlRegisterProtoHook(cmd_list, "LIST", "List users");
		CtdlRegisterProtoHook(cmd_chek, "CHEK", "assorted info we need to check at login");
		CtdlRegisterProtoHook(cmd_qusr, "QUSR", "check to see if a user exists");
		CtdlRegisterProtoHook(cmd_agup, "AGUP", "Administratively Get User Parameters");
		CtdlRegisterProtoHook(cmd_asup, "ASUP", "Administratively Set User Parameters");
		CtdlRegisterProtoHook(cmd_seen, "SEEN", "Manipulate seen/unread message flags");
		CtdlRegisterProtoHook(cmd_gtsn, "GTSN", "Fetch seen/unread message flags");
		CtdlRegisterProtoHook(cmd_view, "VIEW", "Set preferred view for user/room combination");
		CtdlRegisterProtoHook(cmd_renu, "RENU", "Rename a user");
		CtdlRegisterProtoHook(cmd_newu, "NEWU", "Log in as a new user");
	}
	/* return our Subversion id for the Log */
	return "user_ops";
}
