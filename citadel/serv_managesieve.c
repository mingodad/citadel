/**
 * $Id: $
 *
 * This module is an managesieve implementation for the Citadel system.
 * It is compliant with all of the following:
 *
 * http://tools.ietf.org/html/draft-martin-managesieve-06
 * as this draft expires with this writing, you might need to search for
 * the new one.
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <pwd.h>
#include <errno.h>
#include <sys/types.h>
#include <syslog.h>

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

#include <sys/wait.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "citadel.h"
#include "server.h"
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "control.h"
#include "serv_extensions.h"
#include "room_ops.h"
#include "user_ops.h"
#include "policy.h"
#include "database.h"
#include "msgbase.h"
#include "tools.h"
#include "internet_addressing.h"
#include "imap_tools.h"
#include "genstamp.h"
#include "domain.h"
#include "clientsocket.h"
#include "locate_host.h"
#include "citadel_dirs.h"

#ifdef HAVE_OPENSSL
#include "serv_crypto.h"
#endif

#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif



/**
 * http://tools.ietf.org/html/draft-martin-managesieve-06
 *
 * this is the draft this code tries to implement.
 */


struct citmgsve {		
	int command_state;             /**< Information about the current session */
	char *transmitted_message;
	size_t transmitted_length;
};

enum { 	/** Command states for login authentication */
	mgsve_command,
	mgsve_tls,
	mgsve_user,
	mgsve_password,
	mgsve_plain
};

#define MGSVE          CC->MGSVE

/*****************************************************************************/
/*                      MANAGESIEVE Server                                   */
/*****************************************************************************/

void goto_sieverules_room(void)
{// TODO: check if we're authenticated.
	struct ctdlroom QRscratch;
	int c;
	char augmented_roomname[ROOMNAMELEN];
	int transiently = 0;

	MailboxName(augmented_roomname, sizeof augmented_roomname,
		    &CC->user, SIEVERULES);
	c = getroom(&QRscratch, augmented_roomname);
	if (c != 0)/* something went wrong. hit it! */
	{
		cprintf("BYE\r\n");
		CC->kill_me = 1;
		return;
	}
	/* move to the sieve room. */
	memcpy(&CC->room, &QRscratch,
	       sizeof(struct ctdlroom));
	usergoto(NULL, 1, transiently, NULL, NULL);
}

/**
 * Capability listing. Printed as greeting or on "CAPABILITIES" 
 * see Section 1.8 ; 2.4
 */
void cmd_mgsve_caps(void)
{ 
	cprintf("\"IMPLEMENTATION\" \"CITADEL Sieve v6.84\"\r\n" /* TODO: put citversion here. */
		"\"SASL\" \"PLAIN\"\r\n" /*DIGEST-MD5 GSSAPI  SASL sucks.*/
#ifdef HAVE_OPENSSL
/* if TLS is already there, should we say that again? */
		"\"STARTTLS\"\r\n"
#endif
		"\"SIEVE\" \"FILEINTO VACATION\"\r\n" /* TODO: print sieve extensions here. */
		"OK\r\n");
}


/*
 * Here's where our managesieve session begins its happy day.
 */
void managesieve_greeting(void) {

	strcpy(CC->cs_clientname, "Managesieve session");

	CC->internal_pgm = 1;
	CC->cs_flags |= CS_STEALTH;
	MGSVE = malloc(sizeof(struct citmgsve));
	memset(MGSVE, 0, sizeof(struct citmgsve));
	cmd_mgsve_caps();
}

/* AUTHENTICATE command; 2.1 */
void cmd_mgsve_auth(int num_parms, char **parms)
{
/* TODO: compare "digest-md5" or "gssapi" and answer with "NO" */
	if ((num_parms == 3) && !strncasecmp(parms[1], "PLAIN", 5))
		/* todo, check length*/
	{
		char auth[SIZ];
		int retval;
		
		/* todo: how to do plain auth? */
		
		if (parms[2][0] == '{')	
		{
			long literal_length;
			long ret;
			
			literal_length = atol(&parms[2][1]);
			if (literal_length < 1) {
				cprintf("NO %s BAD Message length must be at least 1.\n",
					parms[0]);
				CC->kill_me = 1;
				return;
			}
			MGSVE->transmitted_message = malloc(literal_length + 2);
			if (MGSVE->transmitted_message == NULL) {
				cprintf("NO %s Cannot allocate memory.\r\n", parms[0]);
				CC->kill_me = 1;
				return;
			}
			MGSVE->transmitted_length = literal_length;
			
			ret = client_read(MGSVE->transmitted_message, literal_length);
			MGSVE->transmitted_message[literal_length] = 0;
			
			if (ret != 1) {
				cprintf("%s NO Read failed.\r\n", parms[0]);
				return;
			} 
			
			retval = CtdlDecodeBase64(auth, MGSVE->transmitted_message, SIZ);
			
		}
		else 
			retval = CtdlDecodeBase64(auth, parms[2], SIZ);
		if (login_ok == CtdlLoginExistingUser(auth))
		{
			char *pass;
			pass = &(auth[strlen(auth)+1]);
			/* for some reason the php script sends us the username twice. y? */
			pass = &(pass[strlen(pass)+1]);
			
			if (pass_ok == CtdlTryPassword(pass))
			{
				MGSVE->command_state = mgsve_password;
				cprintf("OK\n");
				return;
			}
		}
	}
	
	cprintf("NO\n");/* we just support auth plain. */
	CC->kill_me = 1;
	
}


#ifdef HAVE_OPENSSL
/* STARTTLS command chapter 2.2 */
void cmd_mgsve_starttls(void)
{ /* answer with OK, and fire off tls session. */
	cprintf("OK\n");
	CtdlStartTLS(NULL, NULL, NULL);
	cmd_mgsve_caps();
}
#endif



/* LOGOUT command, see chapter 2.3 */
void cmd_mgsve_logout(void)
{/* send "OK" and terminate the connection. */
	cprintf("OK\r\n");
	lprintf(CTDL_NOTICE, "MgSve bye.");
	CC->kill_me = 1;
}


/* HAVESPACE command. see chapter 2.5 */
void cmd_mgsve_havespace(void)
{
/* TODO answer NO in any case if auth is missing. */
/* as we don't have quotas in citadel we should always answer with OK; 
 * pherhaps we should have a max-scriptsize. 
 */
	if (MGSVE->command_state != mgsve_password)
	{
		cprintf("NO\n");
		CC->kill_me = 1;
	}
	else
	{
		cprintf("OK"); 
/* citadel doesn't have quotas. in case of change, please add code here. */

	}
}

/* PUTSCRIPT command, see chapter 2.6 */
void cmd_mgsve_putscript(void)
{
/* "scriptname" {nnn+} */
/* TODO: answer with "NO" instant, if we're unauthorized. */
/* AFTER we have the whole script overwrite existing scripts */
/* spellcheck the script before overwrite old ones, and reply with "no" */

}


/** forward declaration for function in msgbase.c */
void headers_listing(long msgnum, void *userdata);


/* LISTSCRIPT command. see chapter 2.7 */
void cmd_mgsve_listscript(void)
{
	goto_sieverules_room();/* TODO: do we need a template? */
	CtdlForEachMessage(MSGS_ALL, 0, NULL, NULL, NULL,
			   headers_listing, NULL);

	
/* TODO: check auth, if not, answer with "no" */
/* do something like the sieve room indexlisting, one per row, in quotes. ACTIVE behind the active one.*/ 

///	ra = getroom(sieveroom, SIEVERULES);
///	/* Only list rooms to which the user has access!! */
///	CtdlRoomAccess(SIEVERULES, &CC->user, &ra, NULL);
///	if ((ra & UA_KNOWN)
///	    || ((ra & UA_GOTOALLOWED) && (ra & UA_ZAPPED))) {
///		imap_mailboxname(buf, sizeof buf, qrbuf);
///		if (imap_mailbox_matches_pattern(pattern, buf)) {
///			cprintf("* LIST () \"/\" ");
///			imap_strout(buf);
///			cprintf("\r\n");
///		}
///	}
///
	cprintf("OK\r\n");
}


/* SETACTIVE command. see chapter 2.8 */
void cmd_mgsve_setactive(void)
{
/* TODO: check auth, if not, answer with "no" */
/* search our room for subjects with that scriptname, 
 * if the scriptname is empty, use the default flag.
 * if the script is not there answer "No "there is no script by that name "
 */



}


/* GETSCRIPT command. see chapter 2.9 */
void cmd_mgsve_getscript(void)
{
/* TODO: check auth, if not, answer with "no" */
/* check first param, this is the name. look up that in the folder.
 * answer with the size {nnn+}and spill it out, one blank line and OK
 */

}


/* DELETESCRIPT command. see chapter 2.10 */
void cmd_mgsve_deletescript(void)
{
/* TODO: check auth, if not, answer with "no" */


}



/*
 *
void mgsve_get_user(char *argbuf) {
	char buf[SIZ];
	char username[SIZ];

	CtdlDecodeBase64(username, argbuf, SIZ);
	/ * lprintf(CTDL_DEBUG, "Trying <%s>\n", username); * /
	if (CtdlLoginExistingUser(username) == login_ok) {
		CtdlEncodeBase64(buf, "Password:", 9);
		cprintf("334 %s\r\n", buf);
		MGSVE->command_state = mgsve_password;
	}
	else {
		cprintf("500 5.7.0 No such user.\r\n");
		MGSVE->command_state = mgsve_command;
	}
}
 */


/*
 *
void mgsve_get_pass(char *argbuf) {
	char password[SIZ];

	CtdlDecodeBase64(password, argbuf, SIZ);
	/ * lprintf(CTDL_DEBUG, "Trying <%s>\n", password); * /
	if (CtdlTryPassword(password) == pass_ok) {
		mgsve_auth_greeting();
	}
	else {
		cprintf("535 5.7.0 Authentication failed.\r\n");
	}
	MGSVE->command_state = mgsve_command;
}
 */


/*
 * Back end for PLAIN auth method (either inline or multistate)
 */
void mgsve_try_plain(char *encoded_authstring) {
	char decoded_authstring[1024];
	char ident[256];
	char user[256];
	char pass[256];

	CtdlDecodeBase64(decoded_authstring,
			encoded_authstring,
			strlen(encoded_authstring) );
	safestrncpy(ident, decoded_authstring, sizeof ident);
	safestrncpy(user, &decoded_authstring[strlen(ident) + 1], sizeof user);
	safestrncpy(pass, &decoded_authstring[strlen(ident) + strlen(user) + 2], sizeof pass);

//	MGSVE->command_state = mgsve_command;
/*
	if (CtdlLoginExistingUser(user) == login_ok) {
		if (CtdlTryPassword(pass) == pass_ok) {
			mgsve_auth_greeting();
			return;
		}
	}
*/
	cprintf("504 5.7.4 Authentication failed.\r\n");
}


/*
 * Attempt to perform authenticated magagesieve
 */
void mgsve_auth(char *argbuf) {
	char username_prompt[64];
	char method[64];
	char encoded_authstring[1024];

	if (CC->logged_in) {
		cprintf("504 5.7.4 Already logged in.\r\n");
		return;
	}

	extract_token(method, argbuf, 0, ' ', sizeof method);

	if (!strncasecmp(method, "login", 5) ) {
		if (strlen(argbuf) >= 7) {
//			mgsve_get_user(&argbuf[6]);
		}
		else {
			CtdlEncodeBase64(username_prompt, "Username:", 9);
			cprintf("334 %s\r\n", username_prompt);
//			MGSVE->command_state = mgsve_user;
		}
		return;
	}

	if (!strncasecmp(method, "plain", 5) ) {
		if (num_tokens(argbuf, ' ') < 2) {
			cprintf("334 \r\n");
//			MGSVE->command_state = mgsve_plain;
			return;
		}

		extract_token(encoded_authstring, argbuf, 1, ' ', sizeof encoded_authstring);

///		mgsve_try_plain(encoded_authstring);
		return;
	}

	if (strncasecmp(method, "login", 5) ) {
		cprintf("504 5.7.4 Unknown authentication method.\r\n");
		return;
	}

}



/*
 * implements the STARTTLS command (Citadel API version)
 */
#ifdef HAVE_OPENSSL
void _mgsve_starttls(void)
{
	char ok_response[SIZ];
	char nosup_response[SIZ];
	char error_response[SIZ];

	sprintf(ok_response,
		"200 2.0.0 Begin TLS negotiation now\r\n");
	sprintf(nosup_response,
		"554 5.7.3 TLS not supported here\r\n");
	sprintf(error_response,
		"554 5.7.3 Internal error\r\n");
	CtdlStartTLS(ok_response, nosup_response, error_response);
}
#endif


/*
 * Create the Sieve script room if it doesn't already exist
 */
void mgsve_create_room(void)
{
	create_room(SIEVERULES, 4, "", 0, 1, 0, VIEW_SIEVE);
}


/* 
 * Main command loop for managesieve sessions.
 */
void managesieve_command_loop(void) {
	char cmdbuf[SIZ];
	char *parms[SIZ];
	int length;
	int num_parms;

	time(&CC->lastcmd);
	memset(cmdbuf, 0, sizeof cmdbuf); /* Clear it, just in case */
	length = client_getln(cmdbuf, sizeof cmdbuf);
	if (length >= 1) {
		num_parms = imap_parameterize(parms, cmdbuf);
		///		length = client_getln(parms[0], sizeof parms[0]);
		length = strlen(parms[0]);
	}
	if (length < 1) {
		lprintf(CTDL_CRIT, "Client disconnected: ending session.\n");
		CC->kill_me = 1;
		return;
	}
	lprintf(CTDL_INFO, "MANAGESIEVE: %s\n", cmdbuf);
//// we have different lengths	while (strlen(cmdbuf) < 5) strcat(cmdbuf, " ");
	if ((length>= 12) && (!strncasecmp(parms[0], "AUTHENTICATE", 12))){
		cmd_mgsve_auth(num_parms, parms);
	}

#ifdef HAVE_OPENSSL
	else if ((length>= 8) && (!strncasecmp(parms[0], "STARTTLS", 8))){
		cmd_mgsve_starttls();
	}
#endif
	else if ((length>= 6) && (!strncasecmp(parms[0], "LOGOUT", 6))){
		cmd_mgsve_logout();
	}
	else if ((length>= 6) && (!strncasecmp(parms[0], "CAPABILITY", 10))){
		cmd_mgsve_caps();
	} /* these commands need to be authenticated. throw it out if it tries. */
	else if (MGSVE->command_state == mgsve_password)
	{
		if ((length>= 9) && (!strncasecmp(parms[0], "HAVESPACE", 9))){
			cmd_mgsve_havespace();
		}
		else if ((length>= 6) && (!strncasecmp(parms[0], "PUTSCRIPT", 9))){
			cmd_mgsve_putscript();
		}
		else if ((length>= 6) && (!strncasecmp(parms[0], "LISTSCRIPT", 10))){
			cmd_mgsve_listscript();
		}
		else if ((length>= 6) && (!strncasecmp(parms[0], "SETACTIVE", 9))){
			cmd_mgsve_setactive();
		}
		else if ((length>= 6) && (!strncasecmp(parms[0], "GETSCRIPT", 9))){
			cmd_mgsve_getscript();
		}
		else if ((length>= 6) && (!strncasecmp(parms[0], "DELETESCRIPT", 11))){
			cmd_mgsve_deletescript();
		}
	}
	else {
		cprintf("No\r\n");
		CC->kill_me = 1;
	}


}



char *serv_managesieve_init(void)
{

	CtdlRegisterServiceHook(config.c_managesieve_port,	/* MGSVE */
				NULL,
				managesieve_greeting,
				managesieve_command_loop,
				NULL);

	CtdlRegisterSessionHook(mgsve_create_room, EVT_LOGIN);
	return "$Id: serv_managesieve.c 4570 2006-08-27 02:07:18Z dothebart $";
}
