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
	char buf[256];

	printf("HTTP/1.0 200 OK\n");
	output_headers(1);

	/* Da banner */
	wprintf("<CENTER><TABLE border=0 width=100%><TR><TD>\n");
	wprintf("<IMG SRC=\"/image&name=hello\">");
	wprintf("</TD><TD><CENTER>\n");

	if (mesg != NULL) {
		wprintf("<font size=+1><b>%s</b></font>", mesg);
	} else {
		serv_puts("MESG hello");
		serv_gets(buf);
		if (buf[0] == '1')
			fmout(NULL);
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

	/* Only offer the "check to disable frames" selection if frames haven't
	 * already been disabled by the browser braindamage check.
	 */
	if (noframes == 0) {
		wprintf("<BR><INPUT TYPE=\"checkbox\" NAME=\"noframes\">");
		wprintf("<FONT SIZE=-1>&nbsp;Check here to disable frames</FONT>\n");
		wprintf("</FORM></CENTER>\n");
	}

	/* Da instructions */
	wprintf("<LI><EM>If you already have an account on %s,",
		serv_info.serv_humannode);
	wprintf("</EM> enter your user name\n");
	wprintf("and password and click \"<TT>Login</TT>.\"<BR>\n");
	wprintf("<LI><EM>If you are a new user,</EM>\n");
	wprintf("enter the name and password you wish to use, and click\n");
	wprintf("\"New User.\"<BR><LI>");
	wprintf("<EM>Please log off properly when finished.</EM>");
	wprintf("<LI>You must use a browser that supports <i>cookies</i>.<BR>\n");
	wprintf("</EM></UL>\n");

	wDumpContent(0);	/* No menu here; not logged in yet! */
}




/*
 * This function needs to get called whenever a PASS or NEWU succeeds.
 */
void become_logged_in(char *user, char *pass, char *serv_response)
{
	logged_in = 1;
	extract(wc_username, &serv_response[4], 0);
	strcpy(wc_password, pass);
	axlevel = extract_int(&serv_response[4], 1);
	if (axlevel >= 6)
		is_aide = 1;
}


void do_login(void)
{
	char buf[256];
	int need_regi = 0;


	/* Note that the initial value of noframes is set by the browser braindamage
	 * check, so don't add an "else" clause here.
	 */
	if (!strcasecmp(bstr("noframes"), "on"))
		noframes = 1;

	if (!strcasecmp(bstr("action"), "Exit")) {
		do_logout();
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
	if (logged_in) {
		serv_puts("CHEK");
		serv_gets(buf);
		if (buf[0] == '2') {
			need_regi = extract_int(&buf[4], 1);
			/* FIX also check for new mail etc. here */
		}
		if (need_regi) {
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

	if (noframes) {
		smart_goto("_BASEROOM_");
	} else {
		output_static("frameset.html");
	}
}


void do_logout(void)
{
	char buf[256];

	strcpy(wc_username, "");
	strcpy(wc_password, "");
	strcpy(wc_roomname, "");

	printf("HTTP/1.0 200 OK\n");
	output_headers(2);	/* note "2" causes cookies to be unset */

	wprintf("<CENTER>");
	serv_puts("MESG goodbye");
	serv_gets(buf);

	if (buf[0] == '1')
		fmout(NULL);
	else
		wprintf("Goodbye\n");

	wprintf("<HR><A HREF=\"/\">Log in again</A></CENTER>\n");
	wDumpContent(2);
	serv_puts("QUIT");
	exit(0);
}





/* 
 * validate new users
 */
void validate(void)
{
	char cmd[256];
	char user[256];
	char buf[256];
	int a;

	printf("HTTP/1.0 200 OK\n");
	output_headers(1);

	strcpy(buf, bstr("user"));
	if (strlen(buf) > 0)
		if (strlen(bstr("axlevel")) > 0) {
			serv_printf("VALI %s|%s", buf, bstr("axlevel"));
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
		wprintf(
			       "<TD><A HREF=\"/validate&user=%s&axlevel=%d\">%s</A></TD>\n",
			       urlesc(user), a, axdefs[a]);
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
	char buf[256];
	int a;

	printf("HTTP/1.0 200 OK\n");
	output_headers(1);

	wprintf("<TABLE WIDTH=100% BORDER=0 BGCOLOR=007700><TR><TD>");
	wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"");
	wprintf("<B>Enter registration info</B>\n");
	wprintf("</FONT></TD></TR></TABLE>\n");

	wprintf("<CENTER>");
	serv_puts("MESG register");
	serv_gets(buf);
	if (buf[0] == '1')
		fmout(NULL);

	wprintf("<FORM ACTION=\"/register\" METHOD=\"POST\">\n");
	wprintf("<INPUT TYPE=\"hidden\" NAME=\"during_login\" VALUE=\"%d\">\n", during_login);

	serv_puts("GREG _SELF_");
	serv_gets(buf);
	if (buf[0] != '1') {
		wprintf("<EM>%s</EM><BR>\n", &buf[4]);
	} else {

		wprintf("<H1>%s</H1><TABLE border>\n", &buf[4]);
		a = 0;
		while (serv_gets(buf), strcmp(buf, "000")) {
			++a;
			wprintf("<TR><TD>");
			switch (a) {
			case 3:
				wprintf("Real Name:</TD><TD><INPUT TYPE=\"text\" NAME=\"realname\" VALUE=\"%s\" MAXLENGTH=\"29\"><BR>\n", buf);
				break;
			case 4:
				wprintf("Street Address:</TD><TD><INPUT TYPE=\"text\" NAME=\"address\" VALUE=\"%s\" MAXLENGTH=\"24\"><BR>\n", buf);
				break;
			case 5:
				wprintf("City/town:</TD><TD><INPUT TYPE=\"text\" NAME=\"city\" VALUE=\"%s\" MAXLENGTH=\"14\"><BR>\n", buf);
				break;
			case 6:
				wprintf("State/province:</TD><TD><INPUT TYPE=\"text\" NAME=\"state\" VALUE=\"%s\" MAXLENGTH=\"2\"><BR>\n", buf);
				break;
			case 7:
				wprintf("ZIP code:</TD><TD><INPUT TYPE=\"text\" NAME=\"zip\" VALUE=\"%s\" MAXLENGTH=\"10\"><BR>\n", buf);
				break;
			case 8:
				wprintf("Telephone:</TD><TD><INPUT TYPE=\"text\" NAME=\"phone\" VALUE=\"%s\" MAXLENGTH=\"14\"><BR>\n", buf);
				break;
			case 9:
				wprintf("E-Mail:</TD><TD><INPUT TYPE=\"text\" NAME=\"email\" VALUE=\"%s\" MAXLENGTH=\"31\"><BR>\n", buf);
				break;
			}
			wprintf("</TD></TR>\n");
		}
		wprintf("</TABLE><P>");
	}
	wprintf("<INPUT type=\"submit\" NAME=\"action\" VALUE=\"Register\">\n");
	wprintf("<INPUT type=\"submit\" NAME=\"action\" VALUE=\"Cancel\">\n");
	wprintf("</CENTER>\n");
	wDumpContent(1);
}

/*
 * register
 */
void register_user(void)
{
	char buf[256];

	if (strcmp(bstr("action"), "Register")) {
		display_error("Cancelled.  Registration was not saved.");
		return;
	}
	serv_puts("REGI");
	serv_gets(buf);
	if (buf[0] != '4') {
		display_error(&buf[4]);
	}
	serv_puts(bstr("realname"));
	serv_puts(bstr("address"));
	serv_puts(bstr("city"));
	serv_puts(bstr("state"));
	serv_puts(bstr("zip"));
	serv_puts(bstr("phone"));
	serv_puts(bstr("email"));
	serv_puts("000");

	if (atoi(bstr("during_login"))) {
		do_welcome();
	} else {
		display_error("Registration information has been saved.");
	}
}





/* 
 * display form for changing your password
 */
void display_changepw(void)
{
	char buf[256];

	printf("HTTP/1.0 200 OK\n");
	output_headers(1);

	wprintf("<TABLE WIDTH=100% BORDER=0 BGCOLOR=770000><TR><TD>");
	wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"");
	wprintf("<B>Change your password</B>\n");
	wprintf("</FONT></TD></TR></TABLE>\n");

	wprintf("<CENTER>");
	serv_puts("MESG changepw");
	serv_gets(buf);
	if (buf[0] == '1')
		fmout(NULL);

	wprintf("<FORM ACTION=\"changepw\" METHOD=\"POST\">\n");
	wprintf("<CENTER><TABLE border><TR><TD>Enter new password:</TD>\n");
	wprintf("<TD><INPUT TYPE=\"password\" NAME=\"newpass1\" VALUE=\"\" MAXLENGTH=\"20\"></TD></TR>\n");
	wprintf("<TR><TD>Enter it again to confirm:</TD>\n");
	wprintf("<TD><INPUT TYPE=\"password\" NAME=\"newpass2\" VALUE=\"\" MAXLENGTH=\"20\"></TD></TR>\n");
	wprintf("</TABLE>\n");
	wprintf("<INPUT type=\"submit\" NAME=\"action\" VALUE=\"Change\">\n");
	wprintf("<INPUT type=\"submit\" NAME=\"action\" VALUE=\"Cancel\">\n");
	wprintf("</CENTER>\n");
	wDumpContent(1);
}

/*
 * change password
 */
void changepw(void)
{
	char buf[256];
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
