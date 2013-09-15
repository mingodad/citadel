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

#include "support.h"
#include "control.h"
#include "ctdl_module.h"

#include "citserver.h"

#include "user_ops.h"
#include "internet_addressing.h"



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
 * Citadel protocol command to do the same
 */
void cmd_isme(char *argbuf) {
	char addr[256];

	if (CtdlAccessCheck(ac_logged_in)) return;
	extract_token(addr, argbuf, 0, '|', sizeof addr);

	if (CtdlIsMe(addr, sizeof addr)) {
		cprintf("%d %s\n", CIT_OK, addr);
	}
	else {
		cprintf("%d Not you.\n", ERROR + ILLEGAL_VALUE);
	}

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



void cmd_quit(char *argbuf)
{
	cprintf("%d Goodbye.\n", CIT_OK);
	CC->kill_me = KILLME_CLIENT_LOGGED_OUT;
}


void cmd_lout(char *argbuf)
{
	if (CC->logged_in) 
		CtdlUserLogout();
	cprintf("%d logged out.\n", CIT_OK);
}


/*****************************************************************************/
/*                      MODULE INITIALIZATION STUFF                          */
/*****************************************************************************/


CTDL_MODULE_INIT(serv_user)
{
	if (!threading) {
		CtdlRegisterProtoHook(cmd_user, "USER", "Submit username for login");
		CtdlRegisterProtoHook(cmd_pass, "PASS", "Complete login by submitting a password");
		CtdlRegisterProtoHook(cmd_quit, "QUIT", "log out and disconnect from server");
		CtdlRegisterProtoHook(cmd_lout, "LOUT", "log out but do not disconnect from server");
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
		CtdlRegisterProtoHook(cmd_isme, "ISME", "Determine whether an email address belongs to a user");
	}
	/* return our Subversion id for the Log */
	return "user";
}
