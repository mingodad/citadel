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

#ifdef HAVE_LIBSIEVE

#include "serv_sieve.h"


/**
 * http://tools.ietf.org/html/draft-martin-managesieve-06
 *
 * this is the draft this code tries to implement.
 */


struct citmgsve {		
	int command_state;             /**< Information about the current session */
	char *transmitted_message;
	size_t transmitted_length;
	char *imap_format_outstring;
	int imap_outstring_length;
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

void sieve_outbuf_append(char *str)
{
        size_t newlen = strlen(str)+1;
        size_t oldlen = (MGSVE->imap_format_outstring==NULL)? 0 : strlen(MGSVE->imap_format_outstring)+2;
        char *buf = malloc ( newlen + oldlen + 10 );
        buf[0]='\0';

        if (oldlen!=0)
                sprintf(buf,"%s%s",MGSVE->imap_format_outstring, str);
        else
                memcpy(buf, str, newlen);
        
        if (oldlen != 0) free (MGSVE->imap_format_outstring);
        MGSVE->imap_format_outstring = buf;
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


long GetSizeToken(char * token)
{
	char *cursor = token;
	char *number;

	while ((*cursor != '\0') && 
	       (*cursor != '{'))
	{
		cursor++;
	}
	if (*cursor == '\0') 
		return -1;
	number = cursor + 1;
	while ((*cursor != '\0') && 
	       (*cursor != '}'))
	{
		cursor++;
	}

	if (cursor[-1] == '+')
		cursor--;

	if (*cursor == '\0') 
		return -1;
	
	return atol(number);
}

char *ReadString(long size, char *command)
{
	long ret;
	if (size < 1) {
		cprintf("NO %s: %ld BAD Message length must be at least 1.\r\n",
			command, size);
		CC->kill_me = 1;
		return NULL;
	}
	MGSVE->transmitted_message = malloc(size + 2);
	if (MGSVE->transmitted_message == NULL) {
		cprintf("NO %s Cannot allocate memory.\r\n", command);
		CC->kill_me = 1;
		return NULL;
	}
	MGSVE->transmitted_length = size;
	
	ret = client_read(MGSVE->transmitted_message, size);
	MGSVE->transmitted_message[size] = '\0';
	
	if (ret != 1) {
		cprintf("%s NO Read failed.\r\n", command);
		return NULL;
	} 
	return MGSVE->transmitted_message;

}
/* AUTHENTICATE command; 2.1 */
void cmd_mgsve_auth(int num_parms, char **parms, struct sdm_userdata *u)
{
/* TODO: compare "digest-md5" or "gssapi" and answer with "NO" */
	if ((num_parms == 3) && !strncasecmp(parms[1], "PLAIN", 5))
		/* todo, check length*/
	{
		char auth[SIZ];
		int retval;
		
		/* todo: how to do plain auth? */
		char *message = ReadString(GetSizeToken(parms[2]), parms[0]);
		
		if (message != NULL) {/* do we have tokenized login? */
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
				cprintf("OK\r\n");
				return;
			}
		}
	}
	
	cprintf("NO\r\n");/* we just support auth plain. */
	CC->kill_me = 1;
}


#ifdef HAVE_OPENSSL
/* STARTTLS command chapter 2.2 */
void cmd_mgsve_starttls(void)
{ /* answer with OK, and fire off tls session. */
	cprintf("OK\r\n");
	CtdlStartTLS(NULL, NULL, NULL);
	cmd_mgsve_caps();
}
#endif



/* LOGOUT command, see chapter 2.3 */
void cmd_mgsve_logout(struct sdm_userdata *u)
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
		cprintf("NO\r\n");
		CC->kill_me = 1;
	}
	else
	{
		cprintf("OK"); 
/* citadel doesn't have quotas. in case of change, please add code here. */

	}
}

/* PUTSCRIPT command, see chapter 2.6 */
void cmd_mgsve_putscript(int num_parms, char **parms, struct sdm_userdata *u)
{
/* "scriptname" {nnn+} */
/* TODO: answer with "NO" instant, if we're unauthorized. */
/* AFTER we have the whole script overwrite existing scripts */
/* spellcheck the script before overwrite old ones, and reply with "no" */
	if (num_parms == 3)
	{
		char *ScriptName;
		char *Script;
		long slength;

		if (parms[1][0]=='"')
			ScriptName = &parms[1][1];
		else
			ScriptName = parms[1];
		
		slength = strlen (ScriptName);
		
		if (ScriptName[slength] == '"')
			ScriptName[slength] = '\0';

		Script = ReadString(GetSizeToken(parms[2]),parms[0]);

		if (Script == NULL) return;
		
		// TODO: do we spellcheck?
		msiv_putscript(u, ScriptName, Script);
		cprintf("OK\r\n");
	}
	else {
		cprintf("%s NO Read failed.\r\n", parms[0]);
		CC->kill_me = 1;
		return;
	} 



}




/* LISTSCRIPT command. see chapter 2.7 */
void cmd_mgsve_listscript(int num_parms, char **parms, struct sdm_userdata *u)
{

	struct sdm_script *s;
	long nScripts = 0;

	MGSVE->imap_format_outstring = NULL;
	for (s=u->first_script; s!=NULL; s=s->next) {
		if (s->script_content != NULL) {
			cprintf("\"%s\"%s\r\n", 
				s->script_name, 
				(s->script_active)?" ACTIVE":"");
			nScripts++;
		}
	}

	//	if (nScripts > 0)
		cprintf("OK\r\n");
		//	else
		//cprintf("NO \"No scripts found.\"\r\n");
}


/* SETACTIVE command. see chapter 2.8 */
void cmd_mgsve_setactive(int num_parms, char **parms, struct sdm_userdata *u)
{
/* TODO: check auth, if not, answer with "no" */
/* search our room for subjects with that scriptname, 
 * if the scriptname is empty, use the default flag.
 * if the script is not there answer "No "there is no script by that name "
 */
	if (num_parms == 2)
	{
		if (msiv_setactive(u, parms[1]) == 0) {
			cprintf("OK\r\n");
		}
		else
			cprintf("No \"there is no script by that name %s \"\r\n", parms[1]);
	}
	else 
		cprintf("NO \"unexpected parameters.\"\r\n");

}


/* GETSCRIPT command. see chapter 2.9 */
void cmd_mgsve_getscript(int num_parms, char **parms, struct sdm_userdata *u)
{
/* check first param, this is the name. look up that in the folder.
 * answer with the size {nnn+}and spill it out, one blank line and OK
 */

	if (num_parms == 2){
		char *script_content;
		long  slen;

		script_content = msiv_getscript(u, parms[1]);
		if (script_content != NULL){
			char *outbuf;

			slen = strlen(script_content);
			outbuf = malloc (slen + 64);
			snprintf(outbuf, slen + 64, "{%ld+}\r\n%s\r\nOK\r\n",slen, script_content);
			cprintf(outbuf);
		}
		else
			cprintf("No \"there is no script by that name %s \"\r\n", parms[1]);
	}
	else 
		cprintf("NO \"unexpected parameters.\"\r\n");
}


/* DELETESCRIPT command. see chapter 2.10 */
void cmd_mgsve_deletescript(int num_parms, char **parms, struct sdm_userdata *u)
{
/* TODO: check auth, if not, answer with "no" */
	int i=-1;

	if (num_parms == 2)
		i = msiv_deletescript(u, parms[1]);
	switch (i){		
	case 0:
		cprintf("OK\r\n");
		break;
	case 1:
		cprintf("NO \"no script by that name: %s\"\r\n", parms[1]);
		break;
	case 2:
		cprintf("NO \"can't delete active Script: %s\"\r\n", parms[1]);
		break;
	default:
	case -1:
		cprintf("NO \"unexpected parameters.\"\r\n");
		break;
	}
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
 * Attempt to perform authenticated managesieve
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
 * Main command loop for managesieve sessions.
 */
void managesieve_command_loop(void) {
	char cmdbuf[SIZ];
	char *parms[SIZ];
	int length;
	int num_parms;
	struct sdm_userdata u;


	memset(&u, 0, sizeof(struct sdm_userdata));

	time(&CC->lastcmd);
	memset(cmdbuf, 0, sizeof cmdbuf); /* Clear it, just in case */
	length = client_getln(cmdbuf, sizeof cmdbuf);
	if (length >= 1) {
		num_parms = imap_parameterize(parms, cmdbuf);
		///		length = client_getln(parms[0], sizeof parms[0]);
		if (num_parms == 0) return;
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
		cmd_mgsve_auth(num_parms, parms, &u);
	}

#ifdef HAVE_OPENSSL
	else if ((length>= 8) && (!strncasecmp(parms[0], "STARTTLS", 8))){
		cmd_mgsve_starttls();
	}
#endif
	else if ((length>= 6) && (!strncasecmp(parms[0], "LOGOUT", 6))){
		cmd_mgsve_logout(&u);
	}
	else if ((length>= 6) && (!strncasecmp(parms[0], "CAPABILITY", 10))){
		cmd_mgsve_caps();
	} /* these commands need to be authenticated. throw it out if it tries. */
	else if (!CtdlAccessCheck(ac_logged_in))
	{
		msiv_load(&u);
		if ((length>= 9) && (!strncasecmp(parms[0], "HAVESPACE", 9))){
			cmd_mgsve_havespace();
		}
		else if ((length>= 6) && (!strncasecmp(parms[0], "PUTSCRIPT", 9))){
			cmd_mgsve_putscript(num_parms, parms, &u);
		}
		else if ((length>= 6) && (!strncasecmp(parms[0], "LISTSCRIPT", 10))){
			cmd_mgsve_listscript(num_parms, parms,&u);
		}
		else if ((length>= 6) && (!strncasecmp(parms[0], "SETACTIVE", 9))){
			cmd_mgsve_setactive(num_parms, parms,&u);
		}
		else if ((length>= 6) && (!strncasecmp(parms[0], "GETSCRIPT", 9))){
			cmd_mgsve_getscript(num_parms, parms, &u);
		}
		else if ((length>= 6) && (!strncasecmp(parms[0], "DELETESCRIPT", 11))){
			cmd_mgsve_deletescript(num_parms, parms, &u);
		}
		msiv_store(&u);
	}
	else {
		/// todo: log this.
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

	return "$Id: serv_managesieve.c 4570 2006-08-27 02:07:18Z dothebart $";
}

#else	/* HAVE_LIBSIEVE */

char *serv_managesieve_init(void)
{
	lprintf(CTDL_INFO, "This server is missing libsieve.  Managesieve protocol is disabled..\n");
	return "$Id:  $";
}

#endif	/* HAVE_LIBSIEVE */
