/*
 * $Id$
 *
 * WebcitAuth; Handles authentication of users to a Citadel server.
 */

#include "webcit.h"
#include "webserver.h"
#include <ctype.h>


void display_reg(int during_login);

/*
 * Access level definitions.  This is initialized from a function rather than a
 * static array so that the strings may be localized.
 */
char *axdefs[7]; 

void initialize_axdefs(void) {
	axdefs[0] = _("Deleted");       /* an erased user */
	axdefs[1] = _("New User");      /* a new user */
	axdefs[2] = _("Problem User");  /* a trouble maker */
	axdefs[3] = _("Local User");    /* user with normal privileges */
	axdefs[4] = _("Network User");  /* a user that may access network resources */
	axdefs[5] = _("Preferred User");/* a moderator */
	axdefs[6] = _("Aide");          /* chief */
}




/* 
 * Display the login screen
 * mesg = the error message if last attempt failed.
 */
void display_login(void)
{
	begin_burst();
	output_headers(1, 0, 0, 0, 1, 0);
	do_template("login", NULL);
	end_burst();
}




/* 
 * Display the openid-enabled login screen
 * mesg = the error message if last attempt failed.
 */
void display_openid_login(char *mesg)
{
  begin_burst();
  output_headers(1, 0, 0, 0, 1, 0);
  do_template("openid_login", NULL);
  end_burst();
}


void display_openid_name_request(const StrBuf *claimed_id, const StrBuf *username) 
{
	StrBuf *Buf = NULL;

	output_headers(1, 1, 2, 0, 0, 0);
	wprintf("<div id=\"login_screen\">\n");

	Buf = NewStrBufPlain(NULL, StrLength(claimed_id));
	StrEscAppend(Buf, claimed_id, NULL, 0, 0);
	svprintf(HKEY("VERIFIED"), WCS_STRING, _("Your OpenID <tt>%s</tt> was successfully verified."),
		 ChrPtr(Buf));
	SVPutBuf("CLAIMED_ID", Buf, 0);


	if (StrLength(username) > 0) {
			Buf = NewStrBufPlain(NULL, StrLength(username));
			StrEscAppend(Buf, claimed_id, NULL, 0, 0);
			svprintf(HKEY("REASON"), WCS_STRING,
				 _("However, the user name '%s' conflicts with an existing user."), 
				 ChrPtr(Buf));
			FreeStrBuf(&Buf);
	}
	else {
		svput("REASON", WCS_STRING, "");
	}

	svput("ACTION_REQUESTED", WCS_STRING, _("Please specify the user name you would like to use."));

	svput("USERNAME_BOX", WCS_STRING, _("User name:"));
	svput("NEWUSER_BUTTON", WCS_STRING, _("New User"));
	svput("EXIT_BUTTON", WCS_STRING, _("Exit"));

	svprintf(HKEY("BOXTITLE"), WCS_STRING, _("%s - powered by <a href=\"http://www.citadel.org\">Citadel</a>"),
		 ChrPtr(serv_info.serv_humannode));

	do_template("openid_manual_create", NULL);
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
void become_logged_in(const StrBuf *user, const StrBuf *pass, StrBuf *serv_response)
{
	wcsession *WCC = WC;
	char buf[SIZ];
	StrBuf *FloorDiv;

	WC->logged_in = 1;

	if (WCC->wc_fullname == NULL)
		WCC->wc_fullname = NewStrBufPlain(NULL, StrLength(serv_response));
	StrBufExtract_token(WCC->wc_fullname, serv_response, 0, '|');
	StrBufCutLeft(WCC->wc_fullname, 4 );
	
	if (WCC->wc_username == NULL)
		WCC->wc_username = NewStrBufDup(user);
	else {
		FlushStrBuf(WCC->wc_username);
		StrBufAppendBuf(WCC->wc_username, user, 0);
	}

	if (WCC->wc_password == NULL)
		WCC->wc_password = NewStrBufDup(pass);
	else {
		FlushStrBuf(WCC->wc_password);
		StrBufAppendBuf(WCC->wc_password, pass, 0);
	}

	WCC->axlevel = StrBufExtract_int(serv_response, 1, '|');
	if (WCC->axlevel >= 6) { /* TODO: make this a define, else it might trick us later */
		WCC->is_aide = 1;
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

	get_preference("floordiv_expanded", &FloorDiv);
	WC->floordiv_expanded = FloorDiv;
}


/* 
 * Perform authentication using a user name and password
 */
void do_login(void)
{
	wcsession *WCC = WC;
	StrBuf *Buf;

	if (havebstr("language")) {
		set_selected_language(bstr("language"));
		go_selected_language();
	}

	if (havebstr("exit_action")) {
		do_logout();
		return;
	}
	Buf = NewStrBuf();
	if (havebstr("login_action")) {
		serv_printf("USER %s", bstr("name"));
		StrBuf_ServGetln(Buf);
		if (GetServerStatus(Buf, NULL) == 3) {
			serv_printf("PASS %s", bstr("pass"));
			StrBuf_ServGetln(Buf);
			if (GetServerStatus(Buf, NULL) == 2) {
				become_logged_in(sbstr("name"), sbstr("pass"), Buf);
			} else {
				snprintf(WCC->ImportantMessage, 
					 sizeof (WCC->ImportantMessage), 
					 "%s", 
					 &(ChrPtr(Buf))[4]);
				display_login();
				FreeStrBuf(&Buf);
				return;
			}
		} else {
			snprintf(WCC->ImportantMessage, 
				 sizeof (WCC->ImportantMessage), 
				 "%s", 
				 &(ChrPtr(Buf))[4]);
			display_login();
			FreeStrBuf(&Buf);
			return;
		}
	}
	if (havebstr("newuser_action")) {
		if (!havebstr("pass")) {
			snprintf(WCC->ImportantMessage, 
				 sizeof (WCC->ImportantMessage), 
				 "%s", 
				 _("Blank passwords are not allowed."));
			display_login();
			FreeStrBuf(&Buf);
			return;
		}
		serv_printf("NEWU %s", bstr("name"));
		StrBuf_ServGetln(Buf);
		if (GetServerStatus(Buf, NULL) == 2) {
			become_logged_in(sbstr("name"), sbstr("pass"), Buf);
			serv_printf("SETP %s", bstr("pass"));
			StrBuf_ServGetln(Buf); /* Don't care? */
		} else {
			snprintf(WCC->ImportantMessage, 
				 sizeof (WCC->ImportantMessage), 
				 "%s", 
				 &(ChrPtr(Buf))[4]);
			display_login();
			FreeStrBuf(&Buf);
			return;
		}
	}
	if (WCC->logged_in) {
		set_preference("language", NewStrBufPlain(bstr("language"), -1), 1);
		if (WCC->need_regi) {
			display_reg(1);
		} else if (WCC->need_vali) {
			validate();
		} else {
			do_welcome();
		}
	} else {
		snprintf(WCC->ImportantMessage, 
			 sizeof (WCC->ImportantMessage), 
			 "%s", 
			 _("Your password was not accepted."));
		display_login();
	}
	FreeStrBuf(&Buf);
}

/* 
 * Try to create an account manually after an OpenID was verified
 */
void openid_manual_create(void)
{
	StrBuf *Buf;

	if (havebstr("exit_action")) {
		do_logout();
		return;
	}

	if (havebstr("newuser_action")) {
		Buf = NewStrBuf();
		serv_printf("OIDC %s", bstr("name"));
		StrBuf_ServGetln(Buf);
		if (GetServerStatus(Buf, NULL) == 2) {
			StrBuf *gpass;

			gpass = NewStrBuf();
			serv_puts("SETP GENERATE_RANDOM_PASSWORD");
			StrBuf_ServGetln(gpass);
			StrBufCutLeft(gpass, 4);
			become_logged_in(sbstr("name"), gpass, Buf);
			FreeStrBuf(&gpass);
		}
		FreeStrBuf(&Buf);
	}

	if (WC->logged_in) {
		if (WC->need_regi) {
			display_reg(1);
		} else if (WC->need_vali) {
			validate();
		} else {
			do_welcome();
		}
	} else {
		display_openid_name_request(sbstr("openid_url"), sbstr("name"));
	}

}


/* 
 * Perform authentication using OpenID
 * assemble the checkid_setup request and then redirect to the user's identity provider
 */
void do_openid_login(void)
{
	wcsession *WCC = WC;
	char buf[4096];

	if (havebstr("language")) {
		set_selected_language(bstr("language"));
		go_selected_language();
	}

	if (havebstr("exit_action")) {
		do_logout();
		return;
	}
	if (havebstr("login_action")) {
		snprintf(buf, sizeof buf,
			"OIDS %s|%s://%s/finalize_openid_login|%s://%s",
			bstr("openid_url"),
			 (is_https ? "https" : "http"), ChrPtr(WCC->http_host),
			 (is_https ? "https" : "http"), ChrPtr(WCC->http_host)
		);

		serv_puts(buf);
		serv_getln(buf, sizeof buf);
		if (buf[0] == '2') {
			lprintf(CTDL_DEBUG, "OpenID server contacted; redirecting to %s\n", &buf[4]);
			http_redirect(&buf[4]);
			return;
		}
		else {
			display_openid_login(&buf[4]);
			return;
		}
	}

	/* If we get to this point then something failed. */
	display_openid_login(_("Your password was not accepted."));
}

/* 
 * Complete the authentication using OpenID
 * This function handles the positive or negative assertion from the user's Identity Provider
 */
void finalize_openid_login(void)
{
	StrBuf *Buf;
	wcsession *WCC = WC;
	int already_logged_in = (WCC->logged_in) ;
	int linecount = 0;
	StrBuf *result = NULL;
	StrBuf *username = NULL;
	StrBuf *password = NULL;
	StrBuf *logged_in_response = NULL;
	StrBuf *claimed_id = NULL;

	if (havebstr("openid.mode")) {
		if (!strcasecmp(bstr("openid.mode"), "id_res")) {
			Buf = NewStrBuf();
			serv_puts("OIDF");
			StrBuf_ServGetln(Buf);
			if (GetServerStatus(Buf, NULL) == 8) {
				urlcontent *u;
				void *U;
				long HKLen;
				const char *HKey;
				HashPos *Cursor;
				
				Cursor = GetNewHashPos (WCC->urlstrings, 0);
				while (GetNextHashPos(WCC->urlstrings, Cursor, &HKLen, &HKey, &U)) {
					u = (urlcontent*) U;
					if (!strncasecmp(u->url_key, "openid.", 7)) {
						serv_printf("%s|%s", &u->url_key[7], ChrPtr(u->url_data));
					}
				}

				serv_puts("000");

				linecount = 0;
				while (StrBuf_ServGetln(Buf), strcmp(ChrPtr(Buf), "000")) 
				{
					if (linecount == 0) result = NewStrBufDup(Buf);
					if (!strcasecmp(ChrPtr(result), "authenticate")) {
						if (linecount == 1) {
							username = NewStrBufDup(Buf);
						}
						else if (linecount == 2) {
							password = NewStrBufDup(Buf);
						}
						else if (linecount == 3) {
							logged_in_response = NewStrBufDup(Buf);
						}
					}
					else if (!strcasecmp(ChrPtr(result), "verify_only")) {
						if (linecount == 1) {
							claimed_id = NewStrBufDup(Buf);
						}
						if (linecount == 2) {
							username = NewStrBufDup(Buf);
						}
					}
					++linecount;
				}
			}
			FreeStrBuf(&Buf);
		}
	}

	/* If we were already logged in, this was an attempt to associate an OpenID account */
	if (already_logged_in) {
		display_openids();
		FreeStrBuf(&result);
		FreeStrBuf(&username);
		FreeStrBuf(&password);
		FreeStrBuf(&claimed_id);
		FreeStrBuf(&logged_in_response);
		return;
	}

	/* If this operation logged us in, either by connecting with an existing account or by
	 * auto-creating one using Simple Registration Extension, we're already on our way.
	 */
	if (!strcasecmp(ChrPtr(result), "authenticate")) {
		become_logged_in(username, password, logged_in_response);
	}

	/* The specified OpenID was verified but the desired user name was either not specified via SRI
	 * or conflicts with an existing user.  Either way the user will need to specify a new name.
	 */

	else if (!strcasecmp(ChrPtr(result), "verify_only")) {
		display_openid_name_request(claimed_id, username);
	}

	/* Did we manage to log in?  If so, continue with the normal flow... */
	if (WC->logged_in) {
		if (WC->need_regi) {
			display_reg(1);
		} else {
			do_welcome();
		}
	} else {
		display_openid_login(_("Your password was not accepted."));
	}

	FreeStrBuf(&result);
	FreeStrBuf(&username);
	FreeStrBuf(&password);
	FreeStrBuf(&claimed_id);
	FreeStrBuf(&logged_in_response);
}


/*
 * Display a welcome screen to the user.
 *
 * If this is the first time login, and the web based setup is enabled, 
 * lead the user through the setup routines
 */
void do_welcome(void)
{
	StrBuf *Buf;
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
	if (!get_preference("startpage", &Buf)) {
		Buf = NewStrBuf ();
		StrBufPrintf(Buf, "dotskip&room=_BASEROOM_");
		set_preference("startpage", Buf, 1);
	}
	if (ChrPtr(Buf)[0] == '/') {
		StrBufCutLeft(Buf, 1);
	}
	if (StrLength(Buf) == 0)
		StrBufAppendBufPlain(Buf, "dotgoto?room=_BASEROOM_", -1, 0);
	http_redirect(ChrPtr(Buf));
}


/*
 * Disconnect from the Citadel server, and end this WebCit session
 */
void end_webcit_session(void) {
	
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

	FlushStrBuf(WC->wc_username);
	FlushStrBuf(WC->wc_password);
	FlushStrBuf(WC->wc_roomname);
	FlushStrBuf(WC->wc_fullname);

	/* FIXME: this is to suppress the iconbar displaying, because we aren't
	   actually logged out yet */
	WC->logged_in = 0;
	
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
				wprintf("<b>%s</b><br>\n", &buf[4]);
			}
		}
	}

	/* Now see if any more users require validation. */
	serv_puts("GNUR");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '2') {
		wprintf("<b>");
		wprintf(_("No users require validation at this time."));
		wprintf("</b><br>\n");
		wDumpContent(1);
		return;
	}
	if (buf[0] != '3') {
		wprintf("<b>%s</b><br>\n", &buf[4]);
		wDumpContent(1);
		return;
	}

	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<table class=\"auth_validate\"><tr><td>\n");
	wprintf("<div id=\"validate\">");

	safestrncpy(user, &buf[4], sizeof user);
	serv_printf("GREG %s", user);
	serv_getln(cmd, sizeof cmd);
	if (cmd[0] == '1') {
		a = 0;
		do {
			serv_getln(buf, sizeof buf);
			++a;
			if (a == 1)
				wprintf("#%s<br><H1>%s</H1>",
					buf, &cmd[4]);
			if (a == 2) {
				char *pch;
				int haveChar = 0;
				int haveNum = 0;
				int haveOther = 0;
				int count = 0;
				pch = buf;
				while (!IsEmptyStr(pch))
				{
					if (isdigit(*pch))
						haveNum = 1;
					else if (isalpha(*pch))
						haveChar = 1;
					else
						haveOther = 1;
					pch ++;
				}
				count = pch - buf;
				if (count > 7)
					count = 0;
				switch (count){
				case 0:
					pch = _("very weak");
					break;
				case 1:
					pch = _("weak");
					break;
				case 2:
					pch = _("ok");
					break;
				case 3:
				default:
					pch = _("strong");
				}

				wprintf("PW: %s<br>\n", pch);
			}
			if (a == 3)
				wprintf("%s<br>\n", buf);
			if (a == 4)
				wprintf("%s<br>\n", buf);
			if (a == 5)
				wprintf("%s, ", buf);
			if (a == 6)
				wprintf("%s ", buf);
			if (a == 7)
				wprintf("%s<br>\n", buf);
			if (a == 8)
				wprintf("%s<br>\n", buf);
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
		wprintf("<a href=\"validate?nonce=%d?user=", WC->nonce);
		urlescputs(user);
		wprintf("&axlevel=%d\">%s</A>&nbsp;&nbsp;&nbsp;\n",
			a, axdefs[a]);
	}
	wprintf("<br />\n");

	wprintf("</div>\n");
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

	vcard_msgnum = locate_user_vcard_in_this_room();
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
	WCTemplputParams SubTP;
	char buf[SIZ];
	StrBuf *Buf;
	output_headers(1, 1, 1, 0, 0, 0);

	Buf = NewStrBufPlain(_("Change your password"), -1);
	memset(&SubTP, 0, sizeof(WCTemplputParams));
	SubTP.Filter.ContextType = CTX_STRBUF;
	SubTP.Context = Buf;
	DoTemplate(HKEY("beginbox"), NULL, &SubTP);

	FreeStrBuf(&Buf);

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
	wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
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

	do_template("endbox", NULL);
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
		if (WC->wc_password == NULL)
			WC->wc_password = NewStrBufPlain(buf, -1);
		else {
			FlushStrBuf(WC->wc_password);
			StrBufAppendBufPlain(WC->wc_password,  buf, -1, 0);
		}
		display_main_menu();
	}
	else {
		display_changepw();
	}
}

int ConditionalAide(StrBuf *Target, WCTemplputParams *TP)
{
	return (WC->is_aide == 0);
}

int ConditionalRoomAide(StrBuf *Target, WCTemplputParams *TP)
{
	return (WC->is_room_aide == 0);
}

int ConditionalIsLoggedIn(StrBuf *Target, WCTemplputParams *TP) {
  return (WC->logged_in == 0);
}
int ConditionalRoomAcessDelete(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;
	return ( (WCC->is_room_aide) || (WCC->is_mailbox) || (WCC->room_flags2 & QR2_COLLABDEL) );
}



void _display_openid_login(void) {display_openid_login(NULL);}
void _display_reg(void) {display_reg(0);}


void 
InitModule_AUTH
(void)
{
	WebcitAddUrlHandler(HKEY("do_welcome"), do_welcome, ANONYMOUS);
	WebcitAddUrlHandler(HKEY("login"), do_login, ANONYMOUS);
	WebcitAddUrlHandler(HKEY("display_openid_login"), _display_openid_login, ANONYMOUS);
	WebcitAddUrlHandler(HKEY("openid_login"), do_openid_login, ANONYMOUS);
	WebcitAddUrlHandler(HKEY("finalize_openid_login"), finalize_openid_login, ANONYMOUS);
	WebcitAddUrlHandler(HKEY("openid_manual_create"), openid_manual_create, ANONYMOUS);
	WebcitAddUrlHandler(HKEY("do_logout"), do_logout, 0);
	WebcitAddUrlHandler(HKEY("validate"), validate, 0);
	WebcitAddUrlHandler(HKEY("display_reg"), _display_reg, 0);
	WebcitAddUrlHandler(HKEY("display_changepw"), display_changepw, 0);
	WebcitAddUrlHandler(HKEY("changepw"), changepw, 0);
	WebcitAddUrlHandler(HKEY("termquit"), do_logout, 0);

	RegisterConditional(HKEY("COND:AIDE"), 2, ConditionalAide, CTX_NONE);
	RegisterConditional(HKEY("COND:ROOMAIDE"), 2, ConditionalRoomAide, CTX_NONE);
	RegisterConditional(HKEY("COND:ACCESS:DELETE"), 2, ConditionalRoomAcessDelete, CTX_NONE);
	RegisterConditional(HKEY("COND:LOGGEDIN"), 2, ConditionalIsLoggedIn, CTX_NONE);

	return ;
}
