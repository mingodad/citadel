/*
 * $Id$
 *
 * WebcitAuth; Handles authentication of users to a Citadel server.
 */

#include "webcit.h"

/*
 * user states
 * the plain text states of a user. filled in at \ function TODO initialize_ax_defs()
 * due to NLS
 */
char *axdefs[7]; 

void initialize_axdefs(void) {
	axdefs[0] = _("Deleted");       /*!0: an erased user */
	axdefs[1] = _("New User");      /*!1: a new user */
	axdefs[2] = _("Problem User");  /*!2: a trouble maker */
	axdefs[3] = _("Local User");    /*!3: user with normal privileges */
	axdefs[4] = _("Network User");  /*!4: a user that may access network resources */
	axdefs[5] = _("Preferred User");/*!5: a moderator */
	axdefs[6] = _("Aide");          /*!6: chief */
}




/* 
 * Display the login screen
 * mesg = the error message if last attempt failed.
 */
void display_login(char *mesg)
{
	char buf[SIZ];

	output_headers(1, 1, 2, 0, 0, 0);
	wprintf("<div id=\"login_screen\">\n");

	if (mesg != NULL) if (!IsEmptyStr(mesg)) {
			stresc(buf, SIZ,  mesg, 0, 0);
			svprintf(HKEY("MESG"), WCS_STRING, "%s", buf);
	}

	svprintf(HKEY("LOGIN_INSTRUCTIONS"), WCS_STRING,
		_("<ul>"
		"<li><b>If you already have an account on %s</b>, "
		"enter your user name and password and click &quot;Login.&quot; "
		"<li><b>If you are a new user</b>, enter the name and password "
		"you wish to use, "
		"and click &quot;New User.&quot; "
		"<li>Please log off properly when finished. "
		"<li>You must use a browser that supports <i>frames</i> and "
		"<i>cookies</i>. "
		"<li>Also keep in mind that if your browser is "
		"configured to block pop-up windows, you will not be able "
		"to receive any instant messages.<br />"
		"</ul>"),
		serv_info.serv_humannode
	);

	svput("USERNAME_BOX", WCS_STRING, _("User name:"));
	svput("PASSWORD_BOX", WCS_STRING, _("Password:"));
	svput("LANGUAGE_BOX", WCS_STRING, _("Language:"));
	svput("LOGIN_BUTTON", WCS_STRING, _("Login"));
	svput("NEWUSER_BUTTON", WCS_STRING, _("New User"));
	svput("EXIT_BUTTON", WCS_STRING, _("Exit"));
	svput("HELLO", WCS_SERVCMD, "MESG hello");
	svprintf(HKEY("BOXTITLE"), WCS_STRING, _("%s - powered by <a href=\"http://www.citadel.org\">Citadel</a>"),
		serv_info.serv_humannode);
	svcallback("DO_LANGUAGE_BOX", offer_languages);
	if (serv_info.serv_newuser_disabled) {
		svput("NEWUSER_BUTTON_PRE", WCS_STRING, "<div style=\"display:none;\">");
		svput("NEWUSER_BUTTON_POST", WCS_STRING, "</div>");
	}
	else {
		svput("NEWUSER_BUTTON_PRE", WCS_STRING, "");
		svput("NEWUSER_BUTTON_POST", WCS_STRING, "");
	}

#ifdef TECH_PREVIEW
		svprintf(HKEY("OFFER_OPENID_LOGIN"), WCS_STRING, "<a href=\"display_openid_login\">%s</a>",
			"Click here to login with OpenID"	// FIXME localize when ready
		);
#else
		svput("OFFER_OPENID_LOGIN", WCS_STRING, "");
#endif

	do_template("login");

	wDumpContent(2);
}




/* 
 * Display the openid-enabled login screen
 * mesg = the error message if last attempt failed.
 */
void display_openid_login(char *mesg)
{
	char buf[SIZ];

	output_headers(1, 1, 2, 0, 0, 0);
	wprintf("<div id=\"login_screen\">\n");

	if (mesg != NULL) if (!IsEmptyStr(mesg)) {
			stresc(buf, SIZ,  mesg, 0, 0);
			svprintf(HKEY("MESG"), WCS_STRING, "%s", buf);
	}

	svprintf(HKEY("LOGIN_INSTRUCTIONS"), WCS_STRING,
		_("<ul>"
		"<li>Enter your OpenID URL and click &quot;Login&quot;."
		"<li>Please log off properly when finished. "
		"<li>You must use a browser that supports <i>frames</i> and "
		"<i>cookies</i>. "
		"<li>Also keep in mind that if your browser is "
		"configured to block pop-up windows, you will not be able "
		"to receive any instant messages.<br />"
		"</ul>")
	);

	svput("OPENID_BOX", WCS_STRING, _("OpenID URL:"));
	svput("LANGUAGE_BOX", WCS_STRING, _("Language:"));
	svput("LOGIN_BUTTON", WCS_STRING, _("Login"));
	svput("EXIT_BUTTON", WCS_STRING, _("Exit"));
	svput("HELLO", WCS_SERVCMD, "MESG hello");
	svprintf(HKEY("BOXTITLE"), WCS_STRING, _("%s - powered by <a href=\"http://www.citadel.org\">Citadel</a>"),
		serv_info.serv_humannode);
	svcallback("DO_LANGUAGE_BOX", offer_languages);

	do_template("openid_login");
	wDumpContent(2);
}




/* Initialize the session
 *
 * This function needs to get called whenever the session changes from
 * not-logged-in to logged-in, either by an explicit login by the user or
 * by a timed-out session automatically re-establishing with a little help
 * from the browser cookie.  Either way, we need to load access controls and
 * preferences from the server.
 *
 * user			the username
 * pass			his password
 * serv_response	The parameters returned from a Citadel USER or NEWU command
 */
void become_logged_in(char *user, char *pass, char *serv_response)
{
	char buf[SIZ];

	WC->logged_in = 1;
	extract_token(WC->wc_fullname, &serv_response[4], 0, '|', sizeof WC->wc_fullname);
	safestrncpy(WC->wc_username, user, sizeof WC->wc_username);
	safestrncpy(WC->wc_password, pass, sizeof WC->wc_password);
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

	get_preference("current_iconbar", buf, sizeof buf);
	WC->current_iconbar = atoi(buf);

	get_preference("floordiv_expanded", WC->floordiv_expanded, sizeof WC->floordiv_expanded);
}


/* 
 * Login Checks
 * the logic to detect invalid passwords not to get on citservers nerves
 */
void do_login(void)
{
	char buf[SIZ];

	if (havebstr("language")) {
		set_selected_language(bstr("language"));
		go_selected_language();
	}

	if (havebstr("exit_action")) {
		do_logout();
		return;
	}
	if (havebstr("login_action")) {
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
	if (havebstr("newuser_action")) {
		if (!havebstr("pass")) {
			display_login(_("Blank passwords are not allowed."));
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
		display_login(_("Your password was not accepted."));
	}

}

/*
 * display the user a welcome screen.
 *
 * if this is the first time login, and the web based setup is enabled, 
 * lead the user through the setup routines
 */
void do_welcome(void)
{
	char buf[SIZ];
#ifdef XXX_NOT_FINISHED_YET_XXX
	FILE *fp;
	int i;

	/**
	 * See if we have to run the first-time setup wizard
	 */
	if (WC->is_aide) {
		if (!setup_wizard) {
			int len;
			sprintf(wizard_filename, "setupwiz.%s.%s",
				ctdlhost, ctdlport);
			len = strlen(wizard_filename);
			for (i=0; i<len; ++i) {
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
					setup_wizard = 1; /**< already run */
				}
			}
		}

		if (!setup_wizard) {
			http_redirect("setup_wizard");
		}
	}
#endif

	/*
	 * Go to the user's preferred start page
	 */
	get_preference("startpage", buf, sizeof buf);
	if (IsEmptyStr(buf)) {
		safestrncpy(buf, "dotskip&room=_BASEROOM_", sizeof buf);
		set_preference("startpage", buf, 1);
	}
	if (buf[0] == '/') {
		strcpy(buf, &buf[1]);
	}
	http_redirect(buf);
}


/*
 * Disconnect from the Citadel server, and end this WebCit session
 */
void end_webcit_session(void) {
	char buf[256];

	if (WC->logged_in) {
		sprintf(buf, "%d", WC->current_iconbar);
		set_preference("current_iconbar", buf, 0);
		set_preference("floordiv_expanded", WC->floordiv_expanded, 1);
	}

	serv_puts("QUIT");
	WC->killthis = 1;
	/* close() of citadel socket will be done by do_housekeeping() */
}

/* 
 * execute the logout
 */
void do_logout(void)
{
	char buf[SIZ];

	safestrncpy(WC->wc_username, "", sizeof WC->wc_username);
	safestrncpy(WC->wc_password, "", sizeof WC->wc_password);
	safestrncpy(WC->wc_roomname, "", sizeof WC->wc_roomname);
	safestrncpy(WC->wc_fullname, "", sizeof WC->wc_fullname);

	/** Calling output_headers() this way causes the cookies to be un-set */
	output_headers(1, 1, 0, 1, 0, 0);

	wprintf("<div id=\"logout_screen\">");
        wprintf("<div class=\"box\">");
        wprintf("<div class=\"boxlabel\">");
	wprintf(_("Log off"));
        wprintf("</div><div class=\"boxcontent\">");	
	serv_puts("MESG goodbye");
	serv_getln(buf, sizeof buf);

	if (WC->serv_sock >= 0) {
		if (buf[0] == '1') {
			fmout("CENTER");
		} else {
			wprintf("Goodbye\n");
		}
	}
	else {
		wprintf(_("This program was unable to connect or stay "
			"connected to the Citadel server.  Please report "
			"this problem to your system administrator.")
		);
		wprintf("<a href=\"http://www.citadel.org/doku.php/"
			"faq:mastering_your_os:net#netstat\">%s</a>", 
			_("Read More..."));
	}

	wprintf("<hr /><div class=\"buttons\"> "
		"<span class=\"button_link\"><a href=\".\">");
	wprintf(_("Log in again"));
	wprintf("</a></span>&nbsp;&nbsp;&nbsp;<span class=\"button_link\">"
		"<a href=\"javascript:window.close();\">");
	wprintf(_("Close window"));
	wprintf("</a></span></div></div></div></div>\n");
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

	output_headers(1, 1, 2, 0, 0, 0);
	wprintf("<div id=\"banner\">\n");
	wprintf("<h1>");
	wprintf(_("Validate new users"));
	wprintf("</h1>");
	wprintf("</div>\n");

	wprintf("<div id=\"content\" class=\"service\">\n");

	/* If the user just submitted a validation, process it... */
	safestrncpy(buf, bstr("user"), sizeof buf);
	if (!IsEmptyStr(buf)) {
		if (havebstr("axlevel")) {
			serv_printf("VALI %s|%s", buf, bstr("axlevel"));
			serv_getln(buf, sizeof buf);
			if (buf[0] != '2') {
				wprintf("<b>%s</b><br />\n", &buf[4]);
			}
		}
	}

	/* Now see if any more users require validation. */
	serv_puts("GNUR");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '2') {
		wprintf("<b>");
		wprintf(_("No users require validation at this time."));
		wprintf("</b><br />\n");
		wDumpContent(1);
		return;
	}
	if (buf[0] != '3') {
		wprintf("<b>%s</b><br />\n", &buf[4]);
		wDumpContent(1);
		return;
	}

	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<table class=\"auth_validate\"><tr><td>\n");
	wprintf("<center>");

	safestrncpy(user, &buf[4], sizeof user);
	serv_printf("GREG %s", user);
	serv_getln(cmd, sizeof cmd);
	if (cmd[0] == '1') {
		a = 0;
		do {
			serv_getln(buf, sizeof buf);
			++a;
			if (a == 1)
				wprintf("#%s<br /><H1>%s</H1>",
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
				wprintf(_("Current access level: %d (%s)\n"),
					atoi(buf), axdefs[atoi(buf)]);
		} while (strcmp(buf, "000"));
	} else {
		wprintf("<H1>%s</H1>%s<br />\n", user, &cmd[4]);
	}

	wprintf("<hr />");
	wprintf(_("Select access level for this user:"));
	wprintf("<br />\n");
	for (a = 0; a <= 6; ++a) {
		wprintf("<a href=\"validate?nonce=%ld?user=", WC->nonce);
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
 *
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

	vcard_msgnum = locate_user_vcard(WC->wc_fullname, -1);
	if (vcard_msgnum < 0L) {
		if (during_login) do_welcome();
		else display_main_menu();
		return;
	}

	if (during_login) {
		do_edit_vcard(vcard_msgnum, "1", "do_welcome", USERCONFIGROOM);
	}
	else {
		do_edit_vcard(vcard_msgnum, "1", "display_main_menu", USERCONFIGROOM);
	}

}




/*
 * display form for changing your password
 */
void display_changepw(void)
{
	char buf[SIZ];

	output_headers(1, 1, 1, 0, 0, 0);

	svput("BOXTITLE", WCS_STRING, _("Change your password"));
	do_template("beginbox");

	if (!IsEmptyStr(WC->ImportantMessage)) {
		wprintf("<span class=\"errormsg\">"
			"%s</span><br />\n", WC->ImportantMessage);
		safestrncpy(WC->ImportantMessage, "", sizeof WC->ImportantMessage);
	}

	serv_puts("MESG changepw");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') {
		fmout("CENTER");
	}

	wprintf("<form name=\"changepwform\" action=\"changepw\" method=\"post\">\n");
	wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%ld\">\n", WC->nonce);
	wprintf("<table class=\"altern\" ");
	wprintf("<tr class=\"even\"><td>");
	wprintf(_("Enter new password:"));
	wprintf("</td><td>");
	wprintf("<input type=\"password\" name=\"newpass1\" value=\"\" maxlength=\"20\"></td></tr>\n");
	wprintf("<tr class=\"odd\"><td>");
	wprintf(_("Enter it again to confirm:"));
	wprintf("</td><td>");
	wprintf("<input type=\"password\" name=\"newpass2\" value=\"\" maxlength=\"20\"></td></tr>\n");
	wprintf("</table>\n");

	wprintf("<div class=\"buttons\">\n");
	wprintf("<input type=\"submit\" name=\"change_action\" value=\"%s\">", _("Change password"));
	wprintf("&nbsp;");
	wprintf("<input type=\"submit\" name=\"cancel_action\" value=\"%s\">\n", _("Cancel"));
	wprintf("</div>\n");
	wprintf("</form>\n");

	do_template("endbox");
	wDumpContent(1);
}

/*
 * change password
 * if passwords match, propagate it to citserver.
 */
void changepw(void)
{
	char buf[SIZ];
	char newpass1[32], newpass2[32];

	if (!havebstr("change_action")) {
		safestrncpy(WC->ImportantMessage, 
			_("Cancelled.  Password was not changed."),
			sizeof WC->ImportantMessage);
		display_main_menu();
		return;
	}

	safestrncpy(newpass1, bstr("newpass1"), sizeof newpass1);
	safestrncpy(newpass2, bstr("newpass2"), sizeof newpass2);

	if (strcasecmp(newpass1, newpass2)) {
		safestrncpy(WC->ImportantMessage, 
			_("They don't match.  Password was not changed."),
			sizeof WC->ImportantMessage);
		display_changepw();
		return;
	}

	if (IsEmptyStr(newpass1)) {
		safestrncpy(WC->ImportantMessage, 
			_("Blank passwords are not allowed."),
			sizeof WC->ImportantMessage);
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



void InitModule_AUTH(void)
{
	WebcitAddUrlHandler(HKEY("do_welcome"), do_welcome, 0);
	return ;
}
