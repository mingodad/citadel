/*
 * $Id$
 *
 * Handles authentication of users to a Citadel server.
 *
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

	output_headers(1, 1, 2, 0, 0, 0, 0);
	wprintf("<div style=\"position:absolute; top:20px; left:20px; right:20px\">\n");

	if (mesg != NULL) if (strlen(mesg) > 0) {
		stresc(buf, mesg, 0, 0);
		svprintf("mesg", WCS_STRING, "%s", buf);
	}

	svprintf("hello", WCS_SERVCMD, "MESG hello");
	svprintf("BOXTITLE", WCS_STRING, "%s - powered by Citadel",
		serv_info.serv_humannode);

	do_template("login");

	wDumpContent(2);
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
	extract_token(WC->wc_username, &serv_response[4], 0, '|', sizeof WC->wc_username);
	strcpy(WC->wc_password, pass);
	WC->axlevel = extract_int(&serv_response[4], 1);
	if (WC->axlevel >= 6) {
		WC->is_aide = 1;
	}

	load_preferences();

	serv_puts("CHEK");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '2') {
		WC->new_mail = extract_int(&buf[4], 0);
		WC->need_regi = extract_int(&buf[4], 1);
		WC->need_vali = extract_int(&buf[4], 2);
		extract_token(WC->cs_inet_email, &buf[4], 3, '|', sizeof WC->cs_inet_email);
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
		serv_getln(buf, sizeof buf);
		if (buf[0] == '3') {
			serv_printf("PASS %s", bstr("pass"));
			serv_getln(buf, sizeof buf);
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
		if (strlen(bstr("pass")) == 0) {
			display_login("Blank passwords are not allowed.");
			return;
		}
		serv_printf("NEWU %s", bstr("name"));
		serv_getln(buf, sizeof buf);
		if (buf[0] == '2') {
			become_logged_in(bstr("name"), bstr("pass"), buf);
			serv_printf("SETP %s", bstr("pass"));
			serv_getln(buf, sizeof buf);
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
	char buf[SIZ];
#ifdef XXX_NOT_FINISHED_YET_XXX
	FILE *fp;
	int i;

	/*
	 * See if we have to run the first-time setup wizard
	 */
	if (WC->is_aide) {
		if (!setup_wizard) {
			sprintf(wizard_filename, "setupwiz.%s.%s",
				ctdlhost, ctdlport);
			for (i=0; i<strlen(wizard_filename); ++i) {
				if (	(wizard_filename[i]==' ')
					|| (wizard_filename[i] == '/')
				) {
					wizard_filename[i] = '_';
				}
			}
	
			fp = fopen(wizard_filename, "r");
			if (fp != NULL) {
				fgets(buf, sizeof buf, fp);
				buf[strlen(buf)-1] = 0;
				fclose(fp);
				if (atoi(buf) == serv_info.serv_rev_level) {
					setup_wizard = 1; /* already run */
				}
			}
		}

		if (!setup_wizard) {
			http_redirect("/setup_wizard");
		}
	}
#endif

	/*
	 * Go to the user's preferred start page
	 */
	get_preference("startpage", buf, sizeof buf);
	if (strlen(buf)==0) {
		strcpy(buf, "/dotskip&room=_BASEROOM_");
		set_preference("startpage", buf);
	}
	http_redirect(buf);
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

	/* Calling output_headers() this way causes the cookies to be un-set */
	output_headers(1, 1, 0, 1, 0, 0, 0);

	wprintf("<center>");
	serv_puts("MESG goodbye");
	serv_getln(buf, sizeof buf);

	if (WC->serv_sock >= 0) {
		if (buf[0] == '1') {
			fmout(NULL, "CENTER");
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

	wprintf("<hr /><a href=\"/\">Log in again</A>&nbsp;&nbsp;&nbsp;"
		"<a href=\"javascript:window.close();\">Close window</A>"
		"</center>\n");
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

	output_headers(1, 1, 2, 0, 0, 0, 0);
	wprintf("<div id=\"banner\">\n"
		"<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>"
		"<SPAN CLASS=\"titlebar\">Validate new users</SPAN>"
		"</TD></TR></TABLE>\n"
		"</div>\n<div id=\"content\">\n"
	);
															     
	strcpy(buf, bstr("user"));
	if (strlen(buf) > 0)
		if (strlen(bstr("axlevel")) > 0) {
			serv_printf("VALI %s|%s", buf, bstr("axlevel"));
			serv_getln(buf, sizeof buf);
			if (buf[0] != '2') {
				wprintf("<b>%s</b><br />\n", &buf[4]);
			}
		}
	serv_puts("GNUR");
	serv_getln(buf, sizeof buf);

	if (buf[0] != '3') {
		wprintf("<b>%s</b><br />\n", &buf[4]);
		wDumpContent(1);
		return;
	}

	wprintf("<div id=\"fix_scrollbar_bug\">"
		"<table border=0 width=100%% bgcolor=\"#ffffff\"><tr><td>\n");
	wprintf("<center>");

	strcpy(user, &buf[4]);
	serv_printf("GREG %s", user);
	serv_getln(cmd, sizeof cmd);
	if (cmd[0] == '1') {
		a = 0;
		do {
			serv_getln(buf, sizeof buf);
			++a;
			if (a == 1)
				wprintf("User #%s<br /><H1>%s</H1>",
					buf, &cmd[4]);
			if (a == 2)
				wprintf("PW: %s<br />\n", buf);
			if (a == 3)
				wprintf("%s<br />\n", buf);
			if (a == 4)
				wprintf("%s<br />\n", buf);
			if (a == 5)
				wprintf("%s, ", buf);
			if (a == 6)
				wprintf("%s ", buf);
			if (a == 7)
				wprintf("%s<br />\n", buf);
			if (a == 8)
				wprintf("%s<br />\n", buf);
			if (a == 9)
				wprintf("Current access level: %d (%s)\n",
					atoi(buf), axdefs[atoi(buf)]);
		} while (strcmp(buf, "000"));
	} else {
		wprintf("<H1>%s</H1>%s<br />\n", user, &cmd[4]);
	}

	wprintf("<hr />Select access level for this user:<br />\n");
	for (a = 0; a <= 6; ++a) {
		wprintf("<A HREF=\"/validate&user=");
		urlescputs(user);
		wprintf("&axlevel=%d\">%s</A>&nbsp;&nbsp;&nbsp;\n",
			a, axdefs[a]);
	}
	wprintf("<br />\n");

	wprintf("</CENTER>\n");
	wprintf("</td></tr></table></div>\n");
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

	output_headers(1, 1, 2, 0, 0, 0, 0);
	wprintf("<div id=\"banner\">\n"
		"<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>"
		"<SPAN CLASS=\"titlebar\">Change your password</SPAN>"
		"</TD></TR></TABLE>\n"
		"</div>\n<div id=\"content\">\n"
	);

	if (strlen(WC->ImportantMessage) > 0) {
		do_template("beginbox_nt");
		wprintf("<SPAN CLASS=\"errormsg\">"
			"%s</SPAN><br />\n", WC->ImportantMessage);
		do_template("endbox");
		strcpy(WC->ImportantMessage, "");
	}

	wprintf("<div id=\"fix_scrollbar_bug\">"
		"<table border=0 width=100%% bgcolor=\"#ffffff\"><tr><td>\n");

	wprintf("<CENTER><br />");
	serv_puts("MESG changepw");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') {
		fmout(NULL, "CENTER");
	}

	wprintf("<form name=\"changepwform\" action=\"changepw\" method=\"post\">\n");
	wprintf("<CENTER>"
		"<table border=\"0\" cellspacing=\"5\" cellpadding=\"5\" "
		"BGCOLOR=\"#EEEEEE\">"
		"<TR><TD>Enter new password:</TD>\n");
	wprintf("<TD><INPUT TYPE=\"password\" NAME=\"newpass1\" VALUE=\"\" MAXLENGTH=\"20\"></TD></TR>\n");
	wprintf("<TR><TD>Enter it again to confirm:</TD>\n");
	wprintf("<TD><INPUT TYPE=\"password\" NAME=\"newpass2\" VALUE=\"\" MAXLENGTH=\"20\"></TD></TR>\n");

	wprintf("</TABLE><br />\n");
	wprintf("<INPUT type=\"submit\" name=\"action\" value=\"Change\">"
		"&nbsp;"
		"<INPUT type=\"submit\" name=\"action\" value=\"Cancel\">\n");
	wprintf("</form></center>\n");
	wprintf("</td></tr></table></div>\n");
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
		strcpy(WC->ImportantMessage, 
			"Cancelled.  Password was not changed.");
		display_main_menu();
		return;
	}

	strcpy(newpass1, bstr("newpass1"));
	strcpy(newpass2, bstr("newpass2"));

	if (strcasecmp(newpass1, newpass2)) {
		strcpy(WC->ImportantMessage, 
			"They don't match.  Password was not changed.");
		display_changepw();
		return;
	}

	if (strlen(newpass1) == 0) {
		strcpy(WC->ImportantMessage, 
			"Blank passwords are not allowed.");
		display_changepw();
		return;
	}

	serv_printf("SETP %s", newpass1);
	serv_getln(buf, sizeof buf);
	sprintf(WC->ImportantMessage, "%s", &buf[4]);
	if (buf[0] == '2') {
		safestrncpy(WC->wc_password, buf, sizeof WC->wc_password);
		display_main_menu();
	}
	else {
		display_changepw();
	}
}
