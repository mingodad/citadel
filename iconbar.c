/*
 * $Id$
 *
 * Displays and customizes the iconbar.
 */

#include "webcit.h"


/** Values for ib_displayas */
#define IB_PICTEXT	0 /**< picture and text */
#define IB_PICONLY	1 /**< just a picture */
#define IB_TEXTONLY	2 /**< just text */

void DontDeleteThis(void *Data){}

#define IconbarIsEnabled(a, b) IconbarIsENABLED(a, sizeof(a) - 1, b)

long IconbarIsENABLED(const char *key, size_t keylen, long defval)
{
	void *Data;
	if (GetHash(WC->IconBarSettings, key, keylen,
		    &Data))
		return (long) Data;
	else 
		return defval;
}

#ifdef DBG_ICONBAR_HASH
static char nbuf[32];
inline const char *PrintInt(void *Prefstr)
{
	snprintf(nbuf, sizeof(nbuf), "%ld", (long)Prefstr);
	return nbuf;
}
#endif

/** Produces a stylesheet which hides any iconbar icons the user does not want */
void doUserIconStylesheet(void) {
  HashPos *pos;
  void *Data;
  long value;
  const char *key;
  long HKLen;

  LoadIconSettings();
  output_custom_content_header("text/css");
  hprintf("Cache-Control: private\r\n");
  
  begin_burst();
  wprintf("#global { left: 16%%; }\r\n");
  pos = GetNewHashPos(WC->IconBarSettings, 0);
  while(GetNextHashPos(WC->IconBarSettings, pos, &HKLen, &key, &Data)) {
    value = (long) Data;
    if (value == 0 
	&& strncasecmp("ib_displayas",key,12) 
	&& strncasecmp("ib_logoff", key, 9)) {
	    /* Don't shoot me for this */
      wprintf("#%s { display: none !important; }\r\n",key);
    } else if (!strncasecmp("ib_users",key, 8) && value == 2) {
      wprintf("#online_users { display: block; !important } \r\n");
    }
  }
  DeleteHashPos(&pos);
  end_burst();
}

int ConditionalIsActiveStylesheet(StrBuf *Target, WCTemplputParams *TP) {
  long testFor = TP->Tokens->Params[3]->lvalue;
  int ib_displayas = IconbarIsEnabled("ib_displayas",IB_PICTEXT);
  return (testFor == ib_displayas);
}

void LoadIconSettings(void)
{
	wcsession *WCC = WC;
	StrBuf *iconbar = NULL;
	StrBuf *buf;
	StrBuf *key;
	long val;
	int i, nTokens;

	buf = NewStrBuf();;
	key = NewStrBuf();
	if (WCC->IconBarSettings == NULL)
		WCC->IconBarSettings = NewHash(1, NULL);
	/**
	 * The initialized values of these variables also happen to
	 * specify the default values for users who haven't customized
	 * their iconbars.  These should probably be set in a master
	 * configuration somewhere.
	 */

	if (get_preference("iconbar", &iconbar)) {
		nTokens = StrBufNum_tokens(iconbar, ',');
		for (i=0; i<nTokens; ++i) {
			StrBufExtract_token(buf, iconbar, i, ',');
			StrBufExtract_token(key, buf, 0, '=');
			val = StrBufExtract_long(buf, 1, '=');
			Put(WCC->IconBarSettings, 
			    ChrPtr(key), StrLength(key),
			    (void*)val, DontDeleteThis);
		}
	}

#ifdef DBG_ICONBAR_HASH
	dbg_PrintHash(WCC->IconBarSetttings, PrintInt, NULL);
#endif

	FreeStrBuf(&key);
	FreeStrBuf(&buf);
}

/**
 * \brief display a customized version of the iconbar
 */
void display_customize_iconbar(void) {
	int i;
	int bar = 0;
	long val;

	int ib_displayas;

	LoadIconSettings();

	output_headers(1, 1, 2, 0, 0, 0);
	wprintf("<div id=\"banner\">");
	wprintf("<h1>");
	wprintf(_("Customize the icon bar"));
	wprintf("</h1></div>\n");

	wprintf("<div id=\"content\" class=\"service\">\n");

	wprintf("<div class=\"fix_scrollbar_bug\">");

	wprintf("<form method=\"post\" action=\"commit_iconbar\">\n");
	wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);

	wprintf("<table class=\"altern\" >\n");
	wprintf("<tr><td></td><td colspan=\"2\"><b>");
	wprintf(_("Display icons as:"));
	wprintf("</b>");
	ib_displayas = IconbarIsEnabled("ib_displayas",IB_PICTEXT);
	for (i=0; i<=2; ++i) {
		wprintf("<input type=\"radio\" name=\"ib_displayas\" value=\"%d\"", i);
		if (ib_displayas == i) wprintf(" CHECKED");
		wprintf(">");
		if (i == IB_PICTEXT)	wprintf(_("pictures and text"));
		if (i == IB_PICONLY)	wprintf(_("pictures only"));
		if (i == IB_TEXTONLY)	wprintf(_("text only"));
		wprintf("\n");
	}
	wprintf("<br />\n");

	wprintf(_("Select the icons you would like to see displayed "
		"in the 'icon bar' menu on the left side of the "
		"screen."));
	wprintf("</td></tr>\n");

	bar = 1 - bar;
	val = IconbarIsEnabled("ib_logo", 0);
	wprintf("<tr class=\"%s\"><td>"
		"<input type=\"radio\" name=\"ib_logo\" value=\"yes\" %s> %s &nbsp;&nbsp;&nbsp;"
		"<input type=\"radio\" name=\"ib_logo\" value=\"no\" %s> %s <br />"
		"</td><td>"
		"<img src=\"image&name=hello\" width=\"48\" alt=\"&nbsp;\">"
		"</td><td>"
		"<b>%s</b><br />"
		"%s"
		"</td></tr>\n",
		(bar ? "even" : "odd"),
		(val ? "CHECKED" : ""),_("Yes"),
		(!val ? "CHECKED" : ""),_("No"),
		_("Site logo"),
		_("An icon describing this site")
	);

	bar = 1 - bar;
	val = IconbarIsEnabled("ib_summary", 1);
	wprintf("<tr class=\"%s\"><td>"
		"<input type=\"radio\" name=\"ib_summary\" value=\"yes\" %s> %s &nbsp;&nbsp;&nbsp;"
		"<input type=\"radio\" name=\"ib_summary\" value=\"no\" %s> %s <br />"
		"</td><td>"
		"<img src=\"static/summscreen_48x.gif\" alt=\"&nbsp;\">"
		"</td><td>"
		"<b>%s</b><br />"
		"%s"
		"</td></tr>\n",
		(bar ? "even" : "odd"),
		(val ? "CHECKED" : ""),_("Yes"),
		(!val ? "CHECKED" : ""),_("No"),
		_("Summary"),
		_("Your summary page")
	);

	bar = 1 - bar;
	val = IconbarIsEnabled("ib_inbox", 1);
	wprintf("<tr class=\"%s\"><td>"
		"<input type=\"radio\" name=\"ib_inbox\" value=\"yes\" %s> %s &nbsp;&nbsp;&nbsp;"
		"<input type=\"radio\" name=\"ib_inbox\" value=\"no\" %s> %s <br />"
		"</td><td>"
		"<img src=\"static/privatemess_48x.gif\" alt=\"&nbsp;\">"
		"</td><td>"
		"<b>%s</b><br />"
		"%s"
		"</td></tr>\n",
		(bar ? "even" : "odd"),
		(val ? "CHECKED" : ""),_("Yes"),
		(!val ? "CHECKED" : ""),_("No"),
		_("Mail (inbox)"),
		_("A shortcut to your email Inbox")
	);

	bar = 1 - bar;
	val = IconbarIsEnabled("ib_contacts", 1);
	wprintf("<tr class=\"%s\"><td>"
		"<input type=\"radio\" name=\"ib_contacts\" value=\"yes\" %s> %s &nbsp;&nbsp;&nbsp;"
		"<input type=\"radio\" name=\"ib_contacts\" value=\"no\" %s> %s <br />"
		"</td><td>"
		"<img src=\"static/viewcontacts_48x.gif\" alt=\"&nbsp;\">"
		"</td><td>"
		"<b>%s</b><br />"
		"%s"
		"</td></tr>\n",
		(bar ? "even" : "odd"),
		(val ? "CHECKED" : ""),_("Yes"),
		(!val ? "CHECKED" : ""),_("No"),
		_("Contacts"),
		_("Your personal address book")
	);

	bar = 1 - bar;
	val = IconbarIsEnabled("ib_notes", 1);
	wprintf("<tr class=\"%s\"><td>"
		"<input type=\"radio\" name=\"ib_notes\" value=\"yes\" %s> %s &nbsp;&nbsp;&nbsp;"
		"<input type=\"radio\" name=\"ib_notes\" value=\"no\" %s> %s <br />"
		"</td><td>"
		"<img src=\"static/storenotes_48x.gif\" alt=\"&nbsp;\">"
		"</td><td>"
		"<b>%s</b><br />"
		"%s"
		"</td></tr>\n",
		(bar ? "even" : "odd"),
		(val ? "CHECKED" : ""),_("Yes"),
		(!val ? "CHECKED" : ""),_("No"),
		_("Notes"),
		_("Your personal notes")
	);

	bar = 1 - bar;
	val = IconbarIsEnabled("ib_calendar", 1);
	wprintf("<tr class=\"%s\"><td>"
		"<input type=\"radio\" name=\"ib_calendar\" value=\"yes\" %s> %s &nbsp;&nbsp;&nbsp;"
		"<input type=\"radio\" name=\"ib_calendar\" value=\"no\" %s> %s <br />"
		"</td><td>"
		"<img src=\"static/calarea_48x.gif\" alt=\"&nbsp;\">"
		"</td><td>"
		"<b>%s</b><br />"
		"%s"
		"</td></tr>\n",
		(bar ? "even" : "odd"),
		(val ? "CHECKED" : ""),_("Yes"),
		(!val ? "CHECKED" : ""),_("No"),
		_("Calendar"),
		_("A shortcut to your personal calendar")
	);

	bar = 1 - bar;
	val = IconbarIsEnabled("ib_tasks", 1);
	wprintf("<tr class=\"%s\"><td>"
		"<input type=\"radio\" name=\"ib_tasks\" value=\"yes\" %s> %s &nbsp;&nbsp;&nbsp;"
		"<input type=\"radio\" name=\"ib_tasks\" value=\"no\" %s> %s <br />"
		"</td><td>"
		"<img src=\"static/taskmanag_48x.gif\" alt=\"&nbsp;\">"
		"</td><td>"
		"<b>%s</b><br />"
		"%s"
		"</td></tr>\n",
		(bar ? "even" : "odd"),
		(val ? "CHECKED" : ""),_("Yes"),
		(!val ? "CHECKED" : ""),_("No"),
		_("Tasks"),
		_("A shortcut to your personal task list")
	);

	bar = 1 - bar;
	val = IconbarIsEnabled("ib_rooms", 1);
	wprintf("<tr class=\"%s\"><td>"
		"<input type=\"radio\" name=\"ib_rooms\" value=\"yes\" %s> %s &nbsp;&nbsp;&nbsp;"
		"<input type=\"radio\" name=\"ib_rooms\" value=\"no\" %s> %s <br />"
		"</td><td>"
		"<img src=\"static/chatrooms_48x.gif\" alt=\"&nbsp;\">"
		"</td><td>"
		"<b>%s</b><br />"
		"%s"
		"</td></tr>\n",
		(bar ? "even" : "odd"),
		(val ? "CHECKED" : ""),_("Yes"),
		(!val ? "CHECKED" : ""),_("No"),
		_("Rooms"),
		_("Clicking this icon displays a list of all accessible "
		"rooms (or folders) available.")
	);

	bar = 1 - bar;
	val = IconbarIsEnabled("ib_users", 1);
	wprintf("<tr class=\"%s\"><td>"
		"<input type=\"radio\" name=\"ib_users\" value=\"yes\" %s> %s &nbsp;&nbsp;&nbsp;"
		"<input type=\"radio\" name=\"ib_users\" value=\"no\" %s> %s <br />"
		"<input type=\"radio\" name=\"ib_users\" value=\"yeslist\" %s> %s"
		"</td><td>"
		"<img src=\"static/usermanag_48x.gif\" alt=\"&nbsp;\">"
		"</td><td>"
		"<b>%s</b>"
		"<br />%s"
		"</td></tr>\n",
		(bar ? "even" : "odd"),
		(val ? "CHECKED" : ""),_("Yes"),
		(!val ? "CHECKED" : ""),_("No"),
		((val > 1) ? "CHECKED" : ""),_("Yes with users list"),
		_("Who is online?"),
		_("Clicking this icon displays a list of all users "
		"currently logged in.")
	);

	bar = 1 - bar;
	val = IconbarIsEnabled("ib_chat", 1);
	wprintf("<tr class=\"%s\"><td>"
		"<input type=\"radio\" name=\"ib_chat\" value=\"yes\" %s> %s &nbsp;&nbsp;&nbsp;"
		"<input type=\"radio\" name=\"ib_chat\" value=\"no\" %s> %s <br />"
		"</td><td>"
		"<img src=\"static/citadelchat_48x.gif\" alt=\"&nbsp;\">"
		"</td><td>"
		"<b>%s</b><br />"
		"%s"
		"</td></tr>\n",
		(bar ? "even" : "odd"),
		(val ? "CHECKED" : ""),_("Yes"),
		(!val ? "CHECKED" : ""),_("No"),
		_("Chat"),
		_("Clicking this icon enters real-time chat mode "
		"with other users in the same room.")
		
	);

	bar = 1 - bar;
	val = IconbarIsEnabled("ib_advanced", 1);
	wprintf("<tr class=\"%s\"><td>"
		"<input type=\"radio\" name=\"ib_advanced\" value=\"yes\" %s> %s &nbsp;&nbsp;&nbsp;"
		"<input type=\"radio\" name=\"ib_advanced\" value=\"no\" %s> %s <br />"
		"</td><td>"
		"<img src=\"static/advanpage2_48x.gif\" alt=\"&nbsp;\">"
		"</td><td>"
		"<b>%s</b><br />"
		"%s"
		"</td></tr>\n",
		(bar ? "even" : "odd"),
		(val ? "CHECKED" : ""),_("Yes"),
		(!val ? "CHECKED" : ""),_("No"),
		_("Advanced options"),
		_("Access to the complete menu of Citadel functions.")

	);

	bar = 1 - bar;
	val = IconbarIsEnabled("ib_citadel", 1);
	wprintf("<tr class=\"%s\"><td>"
		"<input type=\"radio\" name=\"ib_citadel\" value=\"yes\" %s> %s &nbsp;&nbsp;&nbsp;"
		"<input type=\"radio\" name=\"ib_citadel\" value=\"no\" %s> %s <br />"
		"</td><td>"
		"<img border=\"0\" width=\"48\" height=\"48\" "
		"src=\"static/citadel-logo.gif\" alt=\"&nbsp;\">"
		"</td><td>"
		"<b>%s</b><br />"
		"%s"
		"</td></tr>\n",
		(bar ? "even" : "odd"),
		(val ? "CHECKED" : ""),_("Yes"),
		(!val ? "CHECKED" : ""),_("No"),
		_("Citadel logo"),
		_("Displays the 'Powered by Citadel' icon")
	);

	wprintf("</table><br />\n"
		"<center>"
		"<input type=\"submit\" name=\"ok_button\" value=\"%s\">"
		"&nbsp;"
		"<input type=\"submit\" name=\"cancel_button\" value=\"%s\">"
		"</center>\n",
		_("Save changes"),
		_("Cancel")
	);

	wprintf("</form></div>\n");
	wDumpContent(2);
}

/**
 * \brief commit the changes of an edited iconbar ????
 */
void commit_iconbar(void) {
	StrBuf *iconbar;
	StrBuf *buf;
	int i;

	char *boxen[] = {
		"ib_logo",
		"ib_summary",
		"ib_inbox",
		"ib_calendar",
		"ib_contacts",
		"ib_notes",
		"ib_tasks",
		"ib_rooms",
		"ib_users",
		"ib_chat",
		"ib_advanced",
		"ib_logoff",
		"ib_citadel"
	};

	if (!havebstr("ok_button")) {
		display_main_menu();
		return;
	}

	iconbar = NewStrBuf();
	buf = NewStrBuf();
	StrBufPrintf(iconbar, "ib_displayas=%d", ibstr("ib_displayas"));
	for (i=0; i<(sizeof(boxen)/sizeof(char *)); ++i) {
		char *Val;
		if (!strcasecmp(BSTR(boxen[i]), "yes")) {
			Val = "1";
		}
		else if (!strcasecmp(BSTR(boxen[i]), "yeslist")) {
			Val = "2";
		}
		else {
			Val = "0";
		}
		StrBufPrintf(buf, ",%s=%s", boxen[i], Val);
		StrBufAppendBuf(iconbar, buf, 0);

	}
	FreeStrBuf(&buf);
	set_preference("iconbar", iconbar, 1);

	output_headers(1, 1, 2, 0, 0, 0);
	/* TODO: TEMPLATE */
	wprintf("<div id=\"banner\">\n");
	wprintf("<h1>");
	wprintf(_("Customize the icon bar"));
	wprintf("</h1></div>\n");

	wprintf("<div id=\"content\" class=\"service\">\n");
	wprintf(
		"<center><table border=1 bgcolor=\"#ffffff\"><tr><td>"
		"<img src=\"static/advanpage2_48x.gif\">"
		"&nbsp;");
	wprintf(_("Your icon bar has been updated.  Please select any of its "
		  "choices to continue.<br/><span style=\"font-weight: bold;\">You may need to force refresh (SHIFT-F5) in order for changes to take effect</span>"));
	wprintf("</td></tr></table>\n");
	wDumpContent(2);
#ifdef DBG_ICONBAR_HASH
	dbg_PrintHash(WC->IconBarSetttings, PrintInt, NULL);
#endif
}


void tmplput_iconbar(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;
	
	if ((WCC != NULL) && (WCC->logged_in)) {
	  DoTemplate(HKEY("iconbar"), NULL, &NoCtx);
	}
}

void 
InitModule_ICONBAR
(void)
{
  WebcitAddUrlHandler(HKEY("user_iconbar"), doUserIconStylesheet, 0);
  WebcitAddUrlHandler(HKEY("commit_iconbar"), commit_iconbar, 0);
  RegisterConditional(HKEY("COND:ICONBAR:ACTIVE"), 3, ConditionalIsActiveStylesheet, CTX_NONE);
	WebcitAddUrlHandler(HKEY("display_customize_iconbar"), display_customize_iconbar, 0);
	RegisterNamespace("ICONBAR", 0, 0, tmplput_iconbar, 0);

}


/*@}*/
