/*
 * auth.c
 *
 * This file contains code which relates to authentication of users to Citadel.
 *
 * $Id$
 */

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include "webcit.h"
#include "child.h"



/*
 * Display the login screen
 */
void display_login(char *mesg) {
	char buf[256];

	printf("HTTP/1.0 200 OK\n");
	output_headers(1);

	/* Da banner */
	wprintf("<CENTER><TABLE border=0 width=100%><TR><TD>\n");
	wprintf("<IMG SRC=\"/image&name=hello\">");
	wprintf("</TD><TD><CENTER>\n");

	if (mesg != NULL) {
		wprintf("<font size=+1><b>%s</b></font>", mesg);
		}
	else {
		serv_puts("MESG hello");
		serv_gets(buf);
		if (buf[0]=='1') fmout(NULL);
		}

	wprintf("</CENTER></TD></TR></TABLE></CENTER>\n");
	wprintf("<HR>\n");

	/* Da login box */
	wprintf("<CENTER><FORM ACTION=\"/login\" METHOD=\"POST\">\n");
	wprintf("<TABLE border><TR>\n");
	wprintf("<TD>User Name:</TD>\n");
	wprintf("<TD><INPUT TYPE=\"text\" NAME=\"name\" MAXLENGTH=\"25\">\n");
	wprintf("</TD></TR><TR>\n");
	wprintf("<TD>Password:</TD>\n");
	wprintf("<TD><INPUT TYPE=\"password\" NAME=\"pass\" MAXLENGTH=\"20\"></TD>\n");
	wprintf("</TR></TABLE>\n");
	wprintf("<INPUT type=\"submit\" NAME=\"action\" VALUE=\"Login\">\n");
        wprintf("<INPUT type=\"submit\" NAME=\"action\" VALUE=\"New User\">\n");
        wprintf("<INPUT type=\"submit\" NAME=\"action\" VALUE=\"Exit\">\n");
        wprintf("</FORM></CENTER>\n");

	/* Da instructions */
	wprintf("<LI><EM>If you already have an account on %s,",
		serv_info.serv_humannode);
	wprintf("</EM> enter your user name\n");
	wprintf("and password and click \"<TT>Login</TT>.\"<BR>\n");
	wprintf("<LI><EM>If you are a new user,</EM>\n");
	wprintf("enter the name and password you wish to use, and click\n");
	wprintf("\"New User.\"<BR><LI>");
	wprintf("<EM>Please log off properly when finished.</EM>");
	wprintf("<LI>You must use a browser that supports <i>frames</i> ");
	wprintf("and <i>cookies</i>.\n");
	wprintf("</EM></UL>\n");

	wprintf("</BODY></HTML>\n");
	wDumpContent();
	}




/*
 * This function needs to get called whenever a PASS or NEWU succeeds
 */
void become_logged_in(char *user, char *pass, char *serv_response) {
	logged_in = 1;
	extract(wc_username, &serv_response[4], 0);
	strcpy(wc_password, pass);
	axlevel = extract_int(&serv_response[4], 1);
	if (axlevel >=6) is_aide = 1;
	}


void do_login(void) {
	char buf[256];

	if (!strcasecmp(bstr("action"), "Exit")) {
		do_logout();
		}

	if (!strcasecmp(bstr("action"), "Login")) {
		serv_printf("USER %s", bstr("name"));
		serv_gets(buf);
		if (buf[0]=='3') {
			serv_printf("PASS %s", bstr("pass"));
			serv_gets(buf);
			if (buf[0]=='2') {
				become_logged_in(bstr("name"),
					bstr("pass"), buf);
				}
			else {
				display_login(&buf[4]);
				return;
				}
			}
		else {
			display_login(&buf[4]);
			return;
			}
		}

	if (!strcasecmp(bstr("action"), "New User")) {
		serv_printf("NEWU %s", bstr("name"));
		serv_gets(buf);
		if (buf[0]=='2') {
			become_logged_in(bstr("name"), bstr("pass"), buf);
			serv_printf("SETP %s", bstr("pass"));
			serv_gets(buf);
			}
		else {
			display_login(&buf[4]);
			return;
			}
		}

	if (logged_in) {
		output_static("frameset.html");
		}
	else {
		display_login("Your password was not accepted.");
		}

	}

void do_welcome(void) {
	printf("HTTP/1.0 200 OK\n");
	output_headers(1);
	wprintf("<CENTER><H1>");
	escputs(wc_username);
	wprintf("</H1>\n");
	/* FIX add user stats here */

	wprintf("<HR>");
	/* FIX  ---  what should we put here?  the main menu,
	 * or new messages in the lobby?
	 */
	embed_main_menu();

	wprintf("</BODY></HTML>\n");
	wDumpContent();
	}


void do_logout(void) {
	char buf[256];

	strcpy(wc_username, "");
	strcpy(wc_password, "");
	strcpy(wc_roomname, "");
	strcpy(wc_host, "");
	strcpy(wc_port, "");

	printf("HTTP/1.0 200 OK\n");
	printf("X-WebCit-Session: close\n");
	output_headers(1);

	wprintf("<CENTER>");	
	serv_puts("MESG goodbye");
	serv_gets(buf);

	if (buf[0]=='1') fmout(NULL);
	else wprintf("Goodbye\n");

	wprintf("<HR><A HREF=\"/\">Log in again</A>\n");

	wprintf("</CENTER></BODY></HTML>\n");
	wDumpContent();
	serv_puts("QUIT");
	exit(0);
	}
