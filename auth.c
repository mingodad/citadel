/*
 * auth.c
 *
 * This file contains code which relates to authentication of users to Citadel.
 *
 * $Id$
 */


#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <limits.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <stdarg.h>
#include <pthread.h>
#include <signal.h>
#include "webcit.h"

char *axdefs[] =
{
	"Deleted",
	"New User",
	"Problem User",
	"Local User",
	"Network User",
	"Preferred User",
	"Aide"
};

/*
 * Display the login screen
 */
void display_login(char *mesg)
{
	char buf[SIZ];

	output_headers(3);

	if (mesg != NULL) if (strlen(mesg) > 0) {
		stresc(buf, mesg, 0);
		svprintf("mesg", WCS_STRING, "%s", buf);
	}

	svprintf("hello", WCS_SERVCMD, "MESG hello");

	do_template("login");

	clear_local_substs();
	wDumpContent(0);	/* No menu here; not logged in yet! */
}




/*
 * This function needs to get called whenever the session changes from
 * not-logged-in to logged-in, either by an explicit login by the user or
 * by a timed-out session automatically re-establishing with a little help
 * from the browser cookie.  Either way, we need to load access controls and
 * preferences from the server.
 */
void become_logged_in(char *user, char *pass, char *serv_response)
{
	char buf[SIZ];

	WC->logged_in = 1;
	extract(WC->wc_username, &serv_response[4], 0);
	strcpy(WC->wc_password, pass);
	WC->axlevel = extract_int(&serv_response[4], 1);
	if (WC->axlevel >= 6) {
		WC->is_aide = 1;
	}

	load_preferences();

	serv_puts("CHEK");
	serv_gets(buf);
	if (buf[0] == '2') {
		WC->new_mail = extract_int(&buf[4], 0);
		WC->need_regi = extract_int(&buf[4], 1);
		WC->need_vali = extract_int(&buf[4], 2);
		extract(WC->cs_inet_email, &buf[4], 3);
	}
}


void do_login(void)
{
	char buf[SIZ];

	if (!strcasecmp(bstr("action"), "Exit")) {
		do_logout();
		return;
	}
	if (!strcasecmp(bstr("action"), "Login")) {
		serv_printf("USER %s", bstr("name"));
		serv_gets(buf);
		if (buf[0] == '3') {
			serv_printf("PASS %s", bstr("pass"));
			serv_gets(buf);
			if (buf[0] == '2') {
				become_logged_in(bstr("name"),
						 bstr("pass"), buf);
			} else {
				display_login(&buf[4]);
				return;
			}
		} else {
			display_login(&buf[4]);
			return;
		}
	}
	if (!strcasecmp(bstr("action"), "New User")) {
		serv_printf("NEWU %s", bstr("name"));
		serv_gets(buf);
		if (buf[0] == '2') {
			become_logged_in(bstr("name"), bstr("pass"), buf);
			serv_printf("SETP %s", bstr("pass"));
			serv_gets(buf);
		} else {
			display_login(&buf[4]);
			return;
		}
	}
	if (WC->logged_in) {
		if (WC->need_regi) {
			display_reg(1);
		} else {
			do_welcome();
		}
	} else {
		display_login("Your password was not accepted.");
	}

}

void do_welcome(void)
{
	char startpage[SIZ];

	get_preference("startpage", startpage);
	if (strlen(startpage)==0) {
		strcpy(startpage, "/dotskip&room=_BASEROOM_");
		set_preference("startpage", startpage);
	}

        svprintf("STARTPAGE", WCS_STRING, startpage);

        do_template("mainframeset");
        clear_local_substs();
}


/*
 * Disconnect from the Citadel server, and end this WebCit session
 */
void end_webcit_session(void) {
	serv_puts("QUIT");
	WC->killthis = 1;
	/* close() of citadel socket will be done by do_housekeeping() */
}


void do_logout(void)
{
	char buf[SIZ];

	strcpy(WC->wc_username, "");
	strcpy(WC->wc_password, "");
	strcpy(WC->wc_roomname, "");

	output_headers(2);	/* note "2" causes cookies to be unset */

	wprintf("<CENTER>");
	serv_puts("MESG goodbye");
	serv_gets(buf);

	if (WC->serv_sock >= 0) {
		if (buf[0] == '1') {
			fmout(NULL);
		} else {
			wprintf("Goodbye\n");
		}
	}
	else {
		wprintf("This program was unable to connect or stay "
			"connected to the Citadel server.  Please report "
			"this problem to your system administrator."
		);
	}

	wprintf("<HR><A HREF=\"/\">Log in again</A>&nbsp;&nbsp;&nbsp;"
		"<A HREF=\"javascript:window.close();\">Close window</A>"
		"</CENTER>\n");
	wDumpContent(2);
	end_webcit_session();
}



/* 
 * validate new users
 */
void validate(void)
{
	char cmd[SIZ];
	char user[SIZ];
	char buf[SIZ];
	int a;

	output_headers(3);

	strcpy(buf, bstr("user"));
	if (strlen(buf) > 0)
		if (strlen(bstr("WC->axlevel")) > 0) {
			serv_printf("VALI %s|%s", buf, bstr("WC->axlevel"));
			serv_gets(buf);
			if (buf[0] != '2') {
				wprintf("<EM>%s</EM><BR>\n", &buf[4]);
			}
		}
	serv_puts("GNUR");
	serv_gets(buf);

	if (buf[0] != '3') {
		wprintf("<EM>%s</EM><BR>\n", &buf[4]);
		wDumpContent(1);
		return;
	}
	strcpy(user, &buf[4]);
	serv_printf("GREG %s", user);
	serv_gets(cmd);
	if (cmd[0] == '1') {
		a = 0;
		do {
			serv_gets(buf);
			++a;
			if (a == 1)
				wprintf("User #%s<BR><H1>%s</H1>",
					buf, &cmd[4]);
			if (a == 2)
				wprintf("PW: %s<BR>\n", buf);
			if (a == 3)
				wprintf("%s<BR>\n", buf);
			if (a == 4)
				wprintf("%s<BR>\n", buf);
			if (a == 5)
				wprintf("%s, ", buf);
			if (a == 6)
				wprintf("%s ", buf);
			if (a == 7)
				wprintf("%s<BR>\n", buf);
			if (a == 8)
				wprintf("%s<BR>\n", buf);
			if (a == 9)
				wprintf("Current access level: %d (%s)\n",
					atoi(buf), axdefs[atoi(buf)]);
		} while (strcmp(buf, "000"));
	} else {
		wprintf("<H1>%s</H1>%s<BR>\n", user, &cmd[4]);
	}

	wprintf("<CENTER><TABLE border><CAPTION>Select access level:");
	wprintf("</CAPTION><TR>");
	for (a = 0; a <= 6; ++a) {
		wprintf("<TD><A HREF=\"/validate&user=");
		urlescputs(user);
		wprintf("&WC->axlevel=%d\">%s</A></TD>\n",
			a, axdefs[a]);
	}
	wprintf("</TR></TABLE><CENTER><BR>\n");
	wDumpContent(1);
}



/* 
 * Display form for registration.
 * (Set during_login to 1 if this registration is being performed during
 * new user login and will require chaining to the proper screen.)
 */
void display_reg(int during_login)
{
	long vcard_msgnum;

	if (goto_config_room() != 0) {
		if (during_login) do_welcome();
		else display_main_menu();
		return;
	}

	vcard_msgnum = locate_user_vcard(WC->wc_username, -1);
	if (vcard_msgnum < 0L) {
		if (during_login) do_welcome();
		else display_main_menu();
		return;
	}

	if (during_login) {
		do_edit_vcard(vcard_msgnum, "1", "/do_welcome");
	}
	else {
		do_edit_vcard(vcard_msgnum, "1", "/display_main_menu");
	}

}




/* 
 * display form for changing your password
 */
void display_changepw(void)
{
	char buf[SIZ];

	output_headers(3);

	svprintf("BOXTITLE", WCS_STRING, "Change your password");
	do_template("beginbox");
	wprintf("<CENTER>");
	serv_puts("MESG changepw");
	serv_gets(buf);
	if (buf[0] == '1') {
		fmout(NULL);
	}

	wprintf("<FORM ACTION=\"changepw\" METHOD=\"POST\">\n");
	wprintf("<CENTER><TABLE border><TR><TD>Enter new password:</TD>\n");
	wprintf("<TD><INPUT TYPE=\"password\" NAME=\"newpass1\" VALUE=\"\" MAXLENGTH=\"20\"></TD></TR>\n");
	wprintf("<TR><TD>Enter it again to confirm:</TD>\n");
	wprintf("<TD><INPUT TYPE=\"password\" NAME=\"newpass2\" VALUE=\"\" MAXLENGTH=\"20\"></TD></TR>\n");
	wprintf("</TABLE>\n");
	wprintf("<INPUT type=\"submit\" NAME=\"action\" VALUE=\"Change\">\n");
	wprintf("<INPUT type=\"submit\" NAME=\"action\" VALUE=\"Cancel\">\n");
	wprintf("</CENTER>\n");
	do_template("endbox");
	wDumpContent(1);
}

/*
 * change password
 */
void changepw(void)
{
	char buf[SIZ];
	char newpass1[32], newpass2[32];

	if (strcmp(bstr("action"), "Change")) {
		display_error("Cancelled.  Password was not changed.");
		return;
	}
	strcpy(newpass1, bstr("newpass1"));
	strcpy(newpass2, bstr("newpass2"));

	if (strcasecmp(newpass1, newpass2)) {
		display_error("They don't match.  Password was not changed.");
		return;
	}
	serv_printf("SETP %s", newpass1);
	serv_gets(buf);
	if (buf[0] == '2')
		display_success(&buf[4]);
	else
		display_error(&buf[4]);
}
