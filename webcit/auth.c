/*
 * These functions handle authentication of users to a Citadel server.
 *
 * Copyright (c) 1996-2012 by the citadel.org team
 *
 * This program is open source software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "webcit.h"
#include "webserver.h"
#include <ctype.h>

extern uint32_t hashlittle( const void *key, size_t length, uint32_t initval);

/*
 * Access level definitions.  This is initialized from a function rather than a
 * static array so that the strings may be localized.
 */
char *axdefs[7]; 

void initialize_axdefs(void) {

	/* an erased user */
	axdefs[0] = _("Deleted");       

	/* a new user */
	axdefs[1] = _("New User");      

	/* a trouble maker */
	axdefs[2] = _("Problem User");  

	/* user with normal privileges */
	axdefs[3] = _("Local User");    

	/* a user that may access network resources */
	axdefs[4] = _("Network User");  

	/* a moderator */
	axdefs[5] = _("Preferred User");

	/* chief */
	axdefs[6] = _("Aide");          
}



/* 
 * Display the login screen
 * mesg = the error message if last attempt failed.
 */
void display_login(void)
{
	begin_burst();
	output_headers(1, 0, 0, 0, 1, 0);
	do_template("login");
	end_burst();
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
	StrBuf *Buf;
	StrBuf *FloorDiv;

	WCC->logged_in = 1;

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
	if (WCC->axlevel >= 6) {
		WCC->is_aide = 1;
	}

	load_preferences();

	Buf = NewStrBuf();
	serv_puts("CHEK");
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL) == 2) {
		const char *pch;

		pch = ChrPtr(Buf) + 4;
		/*WCC->new_mail  =*/ StrBufExtractNext_long(Buf, &pch, '|');
		WCC->need_regi = StrBufExtractNext_long(Buf, &pch, '|');
		WCC->need_vali = StrBufExtractNext_long(Buf, &pch, '|');
		if (WCC->cs_inet_email == NULL)
			WCC->cs_inet_email  = NewStrBuf();
		StrBufExtract_NextToken(WCC->cs_inet_email, Buf, &pch, '|');
	}
	get_preference("floordiv_expanded", &FloorDiv);
	WCC->floordiv_expanded = FloorDiv;
	FreeStrBuf(&Buf);
	FlushRoomlist();
}


/* 
 * modal/ajax version of 'login' (username and password)
 */
void ajax_login_username_password(void) {
	StrBuf *Buf = NewStrBuf();

	serv_printf("USER %s", bstr("name"));
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL) == 3) {
		serv_printf("PASS %s", bstr("pass"));
		StrBuf_ServGetln(Buf);
		if (GetServerStatus(Buf, NULL) == 2) {
			become_logged_in(sbstr("name"), sbstr("pass"), Buf);
		}
	}

	/* The client is expecting to read back a citadel protocol response */
	wc_printf("%s", ChrPtr(Buf));
	FreeStrBuf(&Buf);
}



/* 
 * modal/ajax version of 'new user' (username and password)
 */
void ajax_login_newuser(void) {
	StrBuf *NBuf = NewStrBuf();
	StrBuf *SBuf = NewStrBuf();

	serv_printf("NEWU %s", bstr("name"));
	StrBuf_ServGetln(NBuf);
	if (GetServerStatus(NBuf, NULL) == 2) {
		become_logged_in(sbstr("name"), sbstr("pass"), NBuf);
		serv_printf("SETP %s", bstr("pass"));
		StrBuf_ServGetln(SBuf);
	}

	/* The client is expecting to read back a citadel protocol response */
	wc_printf("%s", ChrPtr(NBuf));
	FreeStrBuf(&NBuf);
	FreeStrBuf(&SBuf);
}



/* 
 * Try to create an account manually after an OpenID was verified
 */
void openid_manual_create(void)
{
	StrBuf *Buf;

	/* Did the user change his mind?  Pack up and go home. */
	if (havebstr("exit_action")) {
		begin_burst();
		output_headers(1, 0, 0, 0, 1, 0);
		do_template("authpopup_finished");
		end_burst();
		return;
	}


	/* Ok, let's give this a try.  Can we create the new user? */

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

	/* Did we manage to log in?  If so, continue with the normal flow... */
	if (WC->logged_in) {
		if (WC->logged_in) {
			begin_burst();
			output_headers(1, 0, 0, 0, 1, 0);
			do_template("authpopup_finished");
			end_burst();
		}
	} else {
		/* Still no good!  Go back to teh dialog to select a username */
		const StrBuf *Buf;
		putbstr("__claimed_id", NewStrBufDup(sbstr("openid_url")));
		Buf = sbstr("name");
		if (StrLength(Buf) > 0)
			putbstr("__username", NewStrBufDup(Buf));
		begin_burst();
		output_headers(1, 0, 0, 0, 1, 0);
		wc_printf("<html><body>");
		do_template("openid_manual_create");
		wc_printf("</body></html>");
		end_burst();
	}

}


/* 
 * Perform authentication using OpenID
 * assemble the checkid_setup request and then redirect to the user's identity provider
 */
void do_openid_login(void)
{
	char buf[4096];

	snprintf(buf, sizeof buf,
		"OIDS %s|%s/finalize_openid_login|%s",
		bstr("openid_url"),
		ChrPtr(site_prefix),
		ChrPtr(site_prefix)
	);

	serv_puts(buf);
	serv_getln(buf, sizeof buf);
	if (buf[0] == '2') {
		syslog(LOG_DEBUG, "OpenID server contacted; redirecting to %s\n", &buf[4]);
		http_redirect(&buf[4]);
		return;
	}

	begin_burst();
	output_headers(1, 0, 0, 0, 1, 0);
	wc_printf("<html><body>");
	escputs(&buf[4]);
	wc_printf("</body></html>");
	end_burst();
}


/* 
 * Complete the authentication using OpenID
 * This function handles the positive or negative assertion from the user's Identity Provider
 */
void finalize_openid_login(void)
{
	StrBuf *Buf;
	wcsession *WCC = WC;
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
				int len;
				
				Cursor = GetNewHashPos (WCC->Hdr->urlstrings, 0);
				while (GetNextHashPos(WCC->Hdr->urlstrings, Cursor, &HKLen, &HKey, &U)) {
					u = (urlcontent*) U;
					if (!strncasecmp(u->url_key, "openid.", 7)) {
						serv_printf("%s|%s", &u->url_key[7], ChrPtr(u->url_data));
					}
				}

				serv_puts("000");

				linecount = 0;
				while (len = StrBuf_ServGetln(Buf), 
				       ((len >= 0) &&
					((len != 3) || strcmp(ChrPtr(Buf), "000") )))
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

	/*
	 * Is this an attempt to associate a new OpenID with an account that is already logged in?
	 */
	if ( (WCC->logged_in) && (havebstr("attach_existing")) ) {
		display_openids();
	}

	/* If this operation logged us in, either by connecting with an existing account or by
	 * auto-creating one using Simple Registration Extension, we're already on our way.
	 */
	else if (!strcasecmp(ChrPtr(result), "authenticate")) {
		become_logged_in(username, password, logged_in_response);

		/* Did we manage to log in?  If so, continue with the normal flow... */
		if (WC->logged_in) {
			begin_burst();
			output_headers(1, 0, 0, 0, 1, 0);
			do_template("authpopup_finished");
			end_burst();
		} else {
			begin_burst();
			output_headers(1, 0, 0, 0, 1, 0);
			wc_printf("<html><body>");
			wc_printf(_("An error has occurred."));
			wc_printf("</body></html>");
			end_burst();
		}
	}

	/* The specified OpenID was verified but the desired user name was either not specified via SRE
	 * or conflicts with an existing user.  Either way the user will need to specify a new name.
	 */
	else if (!strcasecmp(ChrPtr(result), "verify_only")) {
		putbstr("__claimed_id", claimed_id);
		claimed_id = NULL;
		if (StrLength(username) > 0) {
			putbstr("__username", username);
			username = NULL;
		}
		begin_burst();
		output_headers(1, 0, 0, 0, 1, 0);
		wc_printf("<html><body>");
		do_template("openid_manual_create");
		wc_printf("</body></html>");
		end_burst();
	}

	/* Something went VERY wrong if we get to this point */
	else {
		syslog(1, "finalize_openid_login() failed to do anything.  This is a code problem.\n");
		begin_burst();
		output_headers(1, 0, 0, 0, 1, 0);
		wc_printf("<html><body>");
		wc_printf(_("An error has occurred."));
		wc_printf("</body></html>");
		end_burst();
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
				abs(HashLittle(ctdlhost, strlen(ctdlhost))),
				abs(HashLittle(ctdlport, strlen(ctdlport)))
			);

			fp = fopen(wizard_filename, "r");
			if (fp != NULL) {
				fgets(buf, sizeof buf, fp);
				buf[strlen(buf)-1] = 0;
				fclose(fp);
				if (atoi(buf) == serv_info.serv_rev_level) {
					setup_wizard = 1;	/* already run */
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
		StrBufPrintf(Buf, "dotskip?room=_BASEROOM_");
		set_preference("startpage", Buf, 1);
	}
	if (ChrPtr(Buf)[0] == '/') {
		StrBufCutLeft(Buf, 1);
	}
	if (StrLength(Buf) == 0) {
		StrBufAppendBufPlain(Buf, "dotgoto?room=_BASEROOM_", -1, 0);
	}
	syslog(9, "Redirecting to user's start page: %s\n", ChrPtr(Buf));
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
 * Log out the session with the Citadel server
 */
void do_logout(void)
{
	wcsession *WCC = WC;
	char buf[SIZ];

	FlushStrBuf(WCC->wc_username);
	FlushStrBuf(WCC->wc_password);
	FlushStrBuf(WCC->wc_fullname);
	FlushRoomlist();

	serv_puts("LOUT");
	serv_getln(buf, sizeof buf);
	WCC->logged_in = 0;

	FlushStrBuf(WCC->CurRoom.name);

	/* Calling output_headers() this way causes the cookies to be un-set */
	output_headers(1, 1, 0, 1, 0, 0);

	/* For sites in guest mode, redirect to the landing page after we're logged out */
	if (WC->serv_info->serv_supports_guest) {
		wc_printf("	<script type=\"text/javascript\">	"
			"	window.location='/';			"
			"	</script>				"
		);
	}

	wc_printf("<div id=\"logout_screen\">");
        wc_printf("<div class=\"box\">");
        wc_printf("<div class=\"boxlabel\">");
	wc_printf(_("Log off"));
        wc_printf("</div><div class=\"boxcontent\">");
	serv_puts("MESG goodbye");
	serv_getln(buf, sizeof buf);

	if (WCC->serv_sock >= 0) {
		if (buf[0] == '1') {
			fmout("'CENTER'");
		} else {
			wc_printf("Goodbye\n");
		}
	}
	else {
		wc_printf(_("This program was unable to connect or stay "
			"connected to the Citadel server.  Please report "
			"this problem to your system administrator.")
		);
		wc_printf("<a href=\"http://www.citadel.org/doku.php/"
			"faq:mastering_your_os:net#netstat\">%s</a>",
			_("Read More..."));
	}

	wc_printf("<hr /><div class=\"buttons\"> "
		"<span class=\"button_link\"><a href=\".\">");
	wc_printf(_("Log in again"));
	wc_printf("</a></span>");
	wc_printf("</div></div></div>\n");
	if (WC->serv_info->serv_supports_guest) {
		display_default_landing_page();
		return;
	}

	wDumpContent(2);
	end_webcit_session();
}


/* 
 * Special page for monitoring scripts etc
 */
void monitor(void)
{
	output_headers(0, 0, 0, 0, 0, 0);

	hprintf("Content-type: text/plain\r\n"
		"Server: " PACKAGE_STRING "\r\n"
		"Connection: close\r\n"
	);
	begin_burst();

	wc_printf("Connection to Citadel server at %s:%s : %s\r\n",
		ctdlhost, ctdlport,
		(WC->connected ? "SUCCESS" : "FAIL")
	);

	wDumpContent(0);
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

	output_headers(1, 1, 1, 0, 0, 0);

        do_template("box_begin_1");
        StrBufAppendBufPlain(WC->WBuf, _("Validate new users"), -1, 0);
        do_template("box_begin_2");

	/* If the user just submitted a validation, process it... */
	safestrncpy(buf, bstr("user"), sizeof buf);
	if (!IsEmptyStr(buf)) {
		if (havebstr("axlevel")) {
			serv_printf("VALI %s|%s", buf, bstr("axlevel"));
			serv_getln(buf, sizeof buf);
			if (buf[0] != '2') {
				wc_printf("<b>%s</b><br>\n", &buf[4]);
			}
		}
	}

	/* Now see if any more users require validation. */
	serv_puts("GNUR");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '2') {
		wc_printf("<b>");
		wc_printf(_("No users require validation at this time."));
		wc_printf("</b><br>\n");
		wDumpContent(1);
		return;
	}
	if (buf[0] != '3') {
		wc_printf("<b>%s</b><br>\n", &buf[4]);
		wDumpContent(1);
		return;
	}

	wc_printf("<table class=\"auth_validate\"><tr><td>\n");
	wc_printf("<div id=\"validate\">");

	safestrncpy(user, &buf[4], sizeof user);
	serv_printf("GREG %s", user);
	serv_getln(cmd, sizeof cmd);
	if (cmd[0] == '1') {
		a = 0;
		do {
			serv_getln(buf, sizeof buf);
			++a;
			if (a == 1)
				wc_printf("#%s<br><H1>%s</H1>",
					buf, &cmd[4]);
			if (a == 2) {
				char *pch;
				int haveChar = 0;
				int haveNum = 0;
				int haveOther = 0;
				int haveLong = 0;
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
				if (pch - buf > 7)
					haveLong = 1;
				switch (haveLong + 
					haveChar + 
					haveNum + 
					haveOther)
				{
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

				wc_printf("PW: %s<br>\n", pch);
			}
			if (a == 3)
				wc_printf("%s<br>\n", buf);
			if (a == 4)
				wc_printf("%s<br>\n", buf);
			if (a == 5)
				wc_printf("%s, ", buf);
			if (a == 6)
				wc_printf("%s ", buf);
			if (a == 7)
				wc_printf("%s<br>\n", buf);
			if (a == 8)
				wc_printf("%s<br>\n", buf);
			if (a == 9)
				wc_printf(_("Current access level: %d (%s)\n"),
					atoi(buf), axdefs[atoi(buf)]);
		} while (strcmp(buf, "000"));
	} else {
		wc_printf("<H1>%s</H1>%s<br>\n", user, &cmd[4]);
	}

	wc_printf("<hr />");
	wc_printf(_("Select access level for this user:"));
	wc_printf("<br>\n");
	for (a = 0; a <= 6; ++a) {
		wc_printf("<a href=\"validate?nonce=%d?user=", WC->nonce);
		urlescputs(user);
		wc_printf("&axlevel=%d\">%s</A>&nbsp;&nbsp;&nbsp;\n",
			a, axdefs[a]);
	}
	wc_printf("<br>\n");

	wc_printf("</div>\n");
	wc_printf("</td></tr></table>\n");
	do_template("box_end");
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
	folder Room;
	StrBuf *Buf;
	message_summary *VCMsg = NULL;
	wc_mime_attachment *VCAtt = NULL;
	long vcard_msgnum;

	Buf = NewStrBuf();
	memset(&Room, 0, sizeof(folder));
	if (goto_config_room(Buf, &Room) != 0) {
		syslog(9, "display_reg() exiting because goto_config_room() failed\n");
		if (during_login) {
			pop_destination();
		}
		else {
			display_main_menu();
		}
		FreeStrBuf(&Buf);
		FlushFolder(&Room);		
		return;
	}
	FlushFolder(&Room);

	FreeStrBuf(&Buf);
	vcard_msgnum = locate_user_vcard_in_this_room(&VCMsg, &VCAtt);
	if (vcard_msgnum < 0L) {
		syslog(9, "display_reg() exiting because locate_user_vcard_in_this_room() failed\n");
		if (during_login) {
			pop_destination();
		}
		else {
			display_main_menu();
		}
		return;
	}

	if (during_login) {
		do_edit_vcard(vcard_msgnum, "1", VCMsg, VCAtt, "pop", USERCONFIGROOM);
	}
	else {
		StrBuf *ReturnTo;
		ReturnTo = NewStrBufPlain(HKEY("display_main_menu?go="));
		StrBufAppendBuf(ReturnTo, WC->CurRoom.name, 0);
		do_edit_vcard(vcard_msgnum, "1", VCMsg, VCAtt, ChrPtr(ReturnTo), USERCONFIGROOM);
		FreeStrBuf(&ReturnTo);
	}

}


/*
 * display form for changing your password
 */
void display_changepw(void)
{
	wcsession *WCC = WC;
	WCTemplputParams SubTP;
	char buf[SIZ];
	StrBuf *Buf;
	output_headers(1, 1, 1, 0, 0, 0);

	Buf = NewStrBufPlain(_("Change your password"), -1);
	memset(&SubTP, 0, sizeof(WCTemplputParams));
	SubTP.Filter.ContextType = CTX_STRBUF;
	SubTP.Context = Buf;
	DoTemplate(HKEY("box_begin"), NULL, &SubTP);

	FreeStrBuf(&Buf);

	if (StrLength(WCC->ImportantMsg) > 0) {
		wc_printf("<span class=\"errormsg\">"
			  "%s</span><br>\n", ChrPtr(WCC->ImportantMsg));
		FlushStrBuf(WCC->ImportantMsg);
	}

	serv_puts("MESG changepw");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') {
		fmout("CENTER");
	}

	wc_printf("<form name=\"changepwform\" action=\"changepw\" method=\"post\">\n");
	wc_printf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
	wc_printf("<table class=\"altern\" ");
	wc_printf("<tr class=\"even\"><td>");
	wc_printf(_("Enter new password:"));
	wc_printf("</td><td>");
	wc_printf("<input type=\"password\" name=\"newpass1\" value=\"\" maxlength=\"20\"></td></tr>\n");
	wc_printf("<tr class=\"odd\"><td>");
	wc_printf(_("Enter it again to confirm:"));
	wc_printf("</td><td>");
	wc_printf("<input type=\"password\" name=\"newpass2\" value=\"\" maxlength=\"20\"></td></tr>\n");
	wc_printf("</table>\n");

	wc_printf("<div class=\"buttons\">\n");
	wc_printf("<input type=\"submit\" name=\"change_action\" value=\"%s\">", _("Change password"));
	wc_printf("&nbsp;");
	wc_printf("<input type=\"submit\" name=\"cancel_action\" value=\"%s\">\n", _("Cancel"));
	wc_printf("</div>\n");
	wc_printf("</form>\n");

	do_template("box_end");
	wDumpContent(1);
}

/*
 * change password
 * if passwords match, propagate it to citserver.
 */
void changepw(void)
{
	StrBuf *Line;
	char newpass1[32], newpass2[32];

	if (!havebstr("change_action")) {
		AppendImportantMessage(_("Cancelled.  Password was not changed."), -1);
		display_main_menu();
		return;
	}

	safestrncpy(newpass1, bstr("newpass1"), sizeof newpass1);
	safestrncpy(newpass2, bstr("newpass2"), sizeof newpass2);

	if (strcasecmp(newpass1, newpass2)) {
		AppendImportantMessage(_("They don't match.  Password was not changed."), -1);
		display_changepw();
		return;
	}

	if (IsEmptyStr(newpass1)) {
		AppendImportantMessage(_("Blank passwords are not allowed."), -1);
		display_changepw();
		return;
	}

	Line = NewStrBuf();
	serv_printf("SETP %s", newpass1);
	StrBuf_ServGetln(Line);
	if (GetServerStatusMsg(Line, NULL, 1, 0) == 2) {
		if (WC->wc_password == NULL)
			WC->wc_password = NewStrBufPlain(
				ChrPtr(Line) + 4, 
				StrLength(Line) - 4);
		else {
			FlushStrBuf(WC->wc_password);
			StrBufAppendBufPlain(WC->wc_password,  
					     ChrPtr(Line) + 4, 
					     StrLength(Line) - 4, 0);
		}
		display_main_menu();
	}
	else {
		display_changepw();
	}
	FreeStrBuf(&Line);
}


int ConditionalHaveAccessCreateRoom(StrBuf *Target, WCTemplputParams *TP)
{
	StrBuf *Buf;	

	Buf = NewStrBuf();
	serv_puts("CRE8 0");
	StrBuf_ServGetln(Buf);

	if (GetServerStatus(Buf, NULL) == 2) {
		StrBufCutLeft(Buf, 4);
		AppendImportantMessage(SKEY(Buf));
		FreeStrBuf(&Buf);
		return 0;
	}
	FreeStrBuf(&Buf);
	return 1;
}


int ConditionalAide(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;
	return (WCC != NULL) ? ((WCC->logged_in == 0)||(WC->is_aide == 0)) : 0;
}


int ConditionalIsLoggedIn(StrBuf *Target, WCTemplputParams *TP) 
{
	wcsession *WCC = WC;
	return (WCC != NULL) ? (WCC->logged_in == 0) : 0;

}


/* 
 * toggle the session over to a different language
 */
void switch_language(void) {
	set_selected_language(bstr("lang"));
	pop_destination();
}


void _display_reg(void) {
	display_reg(0);
}


void Header_HandleAuth(StrBuf *Line, ParsedHttpHdrs *hdr)
{
	if (hdr->HR.got_auth == NO_AUTH) /* don't override cookie auth... */
	{
		if (strncasecmp(ChrPtr(Line), "Basic", 5) == 0) {
			StrBufCutLeft(Line, 6);
			StrBufDecodeBase64(Line);
			hdr->HR.plainauth = Line;
			hdr->HR.got_auth = AUTH_BASIC;
		}
		else 
			syslog(1, "Authentication scheme not supported! [%s]\n", ChrPtr(Line));
	}
}


void CheckAuthBasic(ParsedHttpHdrs *hdr)
{
/*
  todo: enable this if we can have other sessions than authenticated ones.
	if (hdr->DontNeedAuth)
		return;
*/
	StrBufAppendBufPlain(hdr->HR.plainauth, HKEY(":"), 0);
	StrBufAppendBuf(hdr->HR.plainauth, hdr->HR.user_agent, 0);
}


void GetAuthBasic(ParsedHttpHdrs *hdr)
{
	const char *Pos = NULL;
	if (hdr->c_username == NULL)
		hdr->c_username = NewStrBufPlain(HKEY(DEFAULT_HTTPAUTH_USER));
	if (hdr->c_password == NULL)
		hdr->c_password = NewStrBufPlain(HKEY(DEFAULT_HTTPAUTH_PASS));
	StrBufExtract_NextToken(hdr->c_username, hdr->HR.plainauth, &Pos, ':');
	StrBufExtract_NextToken(hdr->c_password, hdr->HR.plainauth, &Pos, ':');
}


void Header_HandleCookie(StrBuf *Line, ParsedHttpHdrs *hdr)
{
	const char *pch;
/*
  todo: enable this if we can have other sessions than authenticated ones.
	if (hdr->DontNeedAuth)
		return;
*/
	pch = strstr(ChrPtr(Line), "webcit=");
	if (pch == NULL) {
		return;
	}

	hdr->HR.RawCookie = Line;
	StrBufCutLeft(hdr->HR.RawCookie, (pch - ChrPtr(hdr->HR.RawCookie)) + 7);
	StrBufDecodeHex(hdr->HR.RawCookie);

	cookie_to_stuff(Line, &hdr->HR.desired_session,
			hdr->c_username,
			hdr->c_password,
			hdr->c_roomname,
			hdr->c_language
	);
	hdr->HR.got_auth = AUTH_COOKIE;
}


void 
HttpNewModule_AUTH
(ParsedHttpHdrs *httpreq)
{
	httpreq->c_username = NewStrBufPlain(HKEY(DEFAULT_HTTPAUTH_USER));
	httpreq->c_password = NewStrBufPlain(HKEY(DEFAULT_HTTPAUTH_PASS));
	httpreq->c_roomname = NewStrBuf();
	httpreq->c_language = NewStrBuf();
}


void 
HttpDetachModule_AUTH
(ParsedHttpHdrs *httpreq)
{
	FLUSHStrBuf(httpreq->c_username);
	FLUSHStrBuf(httpreq->c_password);
	FLUSHStrBuf(httpreq->c_roomname);
	FLUSHStrBuf(httpreq->c_language);
}


void 
HttpDestroyModule_AUTH
(ParsedHttpHdrs *httpreq)
{
	FreeStrBuf(&httpreq->c_username);
	FreeStrBuf(&httpreq->c_password);
	FreeStrBuf(&httpreq->c_roomname);
	FreeStrBuf(&httpreq->c_language);
}


void 
InitModule_AUTH
(void)
{
	initialize_axdefs();
	RegisterHeaderHandler(HKEY("COOKIE"), Header_HandleCookie);
	RegisterHeaderHandler(HKEY("AUTHORIZATION"), Header_HandleAuth);

	/* no url pattern at all? Show login. */
	WebcitAddUrlHandler(HKEY(""), "", 0, do_welcome, ANONYMOUS|COOKIEUNNEEDED);

	WebcitAddUrlHandler(HKEY("do_welcome"), "", 0, do_welcome, ANONYMOUS|COOKIEUNNEEDED);
	WebcitAddUrlHandler(HKEY("openid_login"), "", 0, do_openid_login, ANONYMOUS);
	WebcitAddUrlHandler(HKEY("finalize_openid_login"), "", 0, finalize_openid_login, ANONYMOUS);
	WebcitAddUrlHandler(HKEY("openid_manual_create"), "", 0, openid_manual_create, ANONYMOUS);
	WebcitAddUrlHandler(HKEY("validate"), "", 0, validate, 0);
	WebcitAddUrlHandler(HKEY("do_welcome"), "", 0, do_welcome, 0);
	WebcitAddUrlHandler(HKEY("display_reg"), "", 0, _display_reg, 0);
	WebcitAddUrlHandler(HKEY("display_changepw"), "", 0, display_changepw, 0);
	WebcitAddUrlHandler(HKEY("changepw"), "", 0, changepw, 0);
	WebcitAddUrlHandler(HKEY("termquit"), "", 0, do_logout, 0);
	WebcitAddUrlHandler(HKEY("do_logout"), "", 0, do_logout, ANONYMOUS|COOKIEUNNEEDED|FORCE_SESSIONCLOSE);
	WebcitAddUrlHandler(HKEY("monitor"), "", 0, monitor, ANONYMOUS|COOKIEUNNEEDED|FORCE_SESSIONCLOSE);
	WebcitAddUrlHandler(HKEY("ajax_login_username_password"), "", 0, ajax_login_username_password, AJAX|ANONYMOUS);
	WebcitAddUrlHandler(HKEY("ajax_login_newuser"), "", 0, ajax_login_newuser, AJAX|ANONYMOUS);
	WebcitAddUrlHandler(HKEY("switch_language"), "", 0, switch_language, ANONYMOUS);
	RegisterConditional(HKEY("COND:AIDE"), 2, ConditionalAide, CTX_NONE);
	RegisterConditional(HKEY("COND:LOGGEDIN"), 2, ConditionalIsLoggedIn, CTX_NONE);
	RegisterConditional(HKEY("COND:MAY_CREATE_ROOM"), 2,  ConditionalHaveAccessCreateRoom, CTX_NONE);
	return;
}


void 
SessionDestroyModule_AUTH
(wcsession *sess)
{
	FreeStrBuf(&sess->wc_username);
	FreeStrBuf(&sess->wc_fullname);
	FreeStrBuf(&sess->wc_password);
	FreeStrBuf(&sess->httpauth_pass);
	FreeStrBuf(&sess->cs_inet_email);
}
