/*
 * $Id$
 *
 * This file contains functions which handle the mapping of Internet addresses
 * to users on the Citadel system.
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
#include <sys/time.h>
#include <sys/wait.h>
#include <string.h>
#include <limits.h>
#include "citadel.h"
#include "server.h"
#include <time.h>
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "tools.h"
#include "internet_addressing.h"
#include "user_ops.h"


/*
 * Return 0 if a given string fuzzy-matches a Citadel user account
 *
 * FIX ... this needs to be updated to match any and all address syntaxes.
 */
int fuzzy_match(struct usersupp *us, char *matchstring) {
	int a;

	for (a=0; a<strlen(us->fullname); ++a) {
		if (!strncasecmp(&us->fullname[a],
		   matchstring, strlen(matchstring))) {
			return 0;
		}
	}
	return -1;
}




/*
 * Split an RFC822-style address into userid, host, and full name
 * (Originally from citmail.c, and unchanged so far)
 *
 */
void process_rfc822_addr(char *rfc822, char *user, char *node, char *name)
{
	int a;

	/* extract full name - first, it's From minus <userid> */
	strcpy(name, rfc822);
	for (a = 0; a < strlen(name); ++a) {
		if (name[a] == '<') {
			do {
				strcpy(&name[a], &name[a + 1]);
			} while ((strlen(name) > 0) && (name[a] != '>'));
			strcpy(&name[a], &name[a + 1]);
		}
	}
	/* strip anything to the left of a bang */
	while ((strlen(name) > 0) && (haschar(name, '!') > 0))
		strcpy(name, &name[1]);

	/* and anything to the right of a @ or % */
	for (a = 0; a < strlen(name); ++a) {
		if (name[a] == '@')
			name[a] = 0;
		if (name[a] == '%')
			name[a] = 0;
	}

	/* but if there are parentheses, that changes the rules... */
	if ((haschar(rfc822, '(') == 1) && (haschar(rfc822, ')') == 1)) {
		strcpy(name, rfc822);
		while ((strlen(name) > 0) && (name[0] != '(')) {
			strcpy(&name[0], &name[1]);
		}
		strcpy(&name[0], &name[1]);
		for (a = 0; a < strlen(name); ++a) {
			if (name[a] == ')') {
				name[a] = 0;
			}
		}
	}
	/* but if there are a set of quotes, that supersedes everything */
	if (haschar(rfc822, 34) == 2) {
		strcpy(name, rfc822);
		while ((strlen(name) > 0) && (name[0] != 34)) {
			strcpy(&name[0], &name[1]);
		}
		strcpy(&name[0], &name[1]);
		for (a = 0; a < strlen(name); ++a)
			if (name[a] == 34)
				name[a] = 0;
	}
	/* extract user id */
	strcpy(user, rfc822);

	/* first get rid of anything in parens */
	for (a = 0; a < strlen(user); ++a)
		if (user[a] == '(') {
			do {
				strcpy(&user[a], &user[a + 1]);
			} while ((strlen(user) > 0) && (user[a] != ')'));
			strcpy(&user[a], &user[a + 1]);
		}
	/* if there's a set of angle brackets, strip it down to that */
	if ((haschar(user, '<') == 1) && (haschar(user, '>') == 1)) {
		while ((strlen(user) > 0) && (user[0] != '<')) {
			strcpy(&user[0], &user[1]);
		}
		strcpy(&user[0], &user[1]);
		for (a = 0; a < strlen(user); ++a)
			if (user[a] == '>')
				user[a] = 0;
	}
	/* strip anything to the left of a bang */
	while ((strlen(user) > 0) && (haschar(user, '!') > 0))
		strcpy(user, &user[1]);

	/* and anything to the right of a @ or % */
	for (a = 0; a < strlen(user); ++a) {
		if (user[a] == '@')
			user[a] = 0;
		if (user[a] == '%')
			user[a] = 0;
	}


	/* extract node name */
	strcpy(node, rfc822);

	/* first get rid of anything in parens */
	for (a = 0; a < strlen(node); ++a)
		if (node[a] == '(') {
			do {
				strcpy(&node[a], &node[a + 1]);
			} while ((strlen(node) > 0) && (node[a] != ')'));
			strcpy(&node[a], &node[a + 1]);
		}
	/* if there's a set of angle brackets, strip it down to that */
	if ((haschar(node, '<') == 1) && (haschar(node, '>') == 1)) {
		while ((strlen(node) > 0) && (node[0] != '<')) {
			strcpy(&node[0], &node[1]);
		}
		strcpy(&node[0], &node[1]);
		for (a = 0; a < strlen(node); ++a)
			if (node[a] == '>')
				node[a] = 0;
	}
	/* strip anything to the left of a @ */
	while ((strlen(node) > 0) && (haschar(node, '@') > 0))
		strcpy(node, &node[1]);

	/* strip anything to the left of a % */
	while ((strlen(node) > 0) && (haschar(node, '%') > 0))
		strcpy(node, &node[1]);

	/* reduce multiple system bang paths to node!user */
	while ((strlen(node) > 0) && (haschar(node, '!') > 1))
		strcpy(node, &node[1]);

	/* now get rid of the user portion of a node!user string */
	for (a = 0; a < strlen(node); ++a)
		if (node[a] == '!')
			node[a] = 0;

	/* strip leading and trailing spaces in all strings */
	striplt(user);
	striplt(node);
	striplt(name);
}



/*
 * Back end for convert_internet_address()
 * (Compares an internet name [buffer1] and stores in [buffer2] if found)
 */
void try_name(struct usersupp *us) {
	
	if (!strncasecmp(CC->buffer1, "cit", 3))
		if (atol(&CC->buffer1[3]) == us->usernum)
			strcpy(CC->buffer2, us->fullname);

	if (!collapsed_strcmp(CC->buffer1, us->fullname)) 
			strcpy(CC->buffer2, us->fullname);

	if (us->uid != BBSUID)
		if (!strcasecmp(CC->buffer1, getpwuid(us->uid)->pw_name))
			strcpy(CC->buffer2, us->fullname);
}


/*
 * Convert an Internet email address to a Citadel user/host combination
 */
int convert_internet_address(char *destuser, char *desthost, char *source)
{
	char user[256];
	char node[256];
	char name[256];

	/* Split it up */
	process_rfc822_addr(source, user, node, name);

	/* Map the FQDN to a Citadel node name
	 * FIX ... we have to check for all known aliases for the local
	 *         system, and also handle gateway domains, etc. etc.
	 */
	if (!strcasecmp(node, config.c_fqdn)) {
		strcpy(node, config.c_nodename);
	}

	/* Return an error condition if the node is not known.
	 * FIX ... make this work for non-local systems
	 */
	if (strcasecmp(node, config.c_nodename)) {
		return(1);
	}
	
	/* Now try to resolve the name
	 * FIX ... do the multiple-addresses thing
	 */
	if (!strcasecmp(node, config.c_nodename)) {
		strcpy(destuser, user);
		strcpy(desthost, config.c_nodename);
		strcpy(CC->buffer1, user);
		strcpy(CC->buffer2, "");
		ForEachUser(try_name);
		if (strlen(CC->buffer2) == 0) return(2);
		strcpy(destuser, CC->buffer2);
		return(0);
	}

	return(3);	/* unknown error */
}
