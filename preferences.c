/*
 * $Id$
 *
 * Manage user preferences with a little help from the Citadel server.
 *
 */

#include "webcit.h"
#include "webserver.h"
#include "groupdav.h"


HashList *PreferenceHooks;

typedef struct _Prefs {
	long Type;
	const char *Setting;
	const char *PrefStr;
} Prefs;

void RegisterPreference(const char *Setting, const char *PrefStr, long Type)
{
	Prefs *Newpref = (Prefs*) malloc(sizeof(Prefs));
	Newpref->Setting = Setting;
	Newpref->PrefStr = PrefStr;
	Newpref->Type = Type;
	Put(PreferenceHooks, Setting, strlen(Setting), Newpref, NULL);
}

const char *PrefGetLocalStr(const char *Setting, long len)
{
	void *hash_value;
	if (GetHash(PreferenceHooks, Setting, len, &hash_value) != 0) {
		Prefs *Newpref = (Prefs*) hash_value;
		return _(Newpref->PrefStr);

	}
	return "";
}

#ifdef DBG_PREFS_HASH
inline const char *PrintPref(void *Prefstr)
{
	return ChrPtr(Prefstr);
}
#endif

/*
 * display preferences dialog
 */
void load_preferences(void) {
	char buf[SIZ];
	long msgnum = 0L;
	StrBuf *ReadBuf;
	
	serv_printf("GOTO %s", USERCONFIGROOM);
	serv_getln(buf, sizeof buf);
	if (buf[0] != '2') return;
	
	serv_puts("MSGS ALL|0|1");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '8') {
		serv_puts("subj|__ WebCit Preferences __");
		serv_puts("000");
	}
	while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		msgnum = atol(buf);
	}

	if (msgnum > 0L) {
		serv_printf("MSG0 %ld", msgnum);
		serv_getln(buf, sizeof buf);
		if (buf[0] == '1') {
			ReadBuf = NewStrBuf();
			while (StrBuf_ServGetln(ReadBuf),
			       (strcmp(ChrPtr(ReadBuf), "text") && 
				strcmp(ChrPtr(ReadBuf), "000"))) {
			}
			if (!strcmp(ChrPtr(ReadBuf), "text")) {
				StrBuf *Key;
				StrBuf *Data = NULL;
				StrBuf *LastData = NULL;
				
				Key = NewStrBuf();
				while (StrBuf_ServGetln(ReadBuf), 
				       strcmp(ChrPtr(ReadBuf), "000")) 
				{
					if ((ChrPtr(ReadBuf)[0] == ' ') &&
					    (Data != NULL)) {
						StrBufAppendBuf(Data, ReadBuf, 1);
					}
					else {
						LastData = Data = NewStrBuf();
						StrBufExtract_token(Key, ReadBuf, 0, '|');
						StrBufExtract_token(Data, ReadBuf, 1, '|');
						if (!IsEmptyStr(ChrPtr(Key)))
						{
							Put(WC->hash_prefs, 
							    ChrPtr(Key), StrLength(Key), 
							    Data, 
							    HFreeStrBuf);
						}
						else 
						{
							FreeStrBuf(&Data);
							LastData = NULL;
						}
					}
				}
				FreeStrBuf(&Key);
			}
			FreeStrBuf(&ReadBuf);
		}
	}

	/* Go back to the room we're supposed to be in */
	serv_printf("GOTO %s", WC->wc_roomname);
	serv_getln(buf, sizeof buf);
}

/**
 * \brief Goto the user's configuration room, creating it if necessary.
 * \return 0 on success or nonzero upon failure.
 */
int goto_config_room(void) {
	char buf[SIZ];

	serv_printf("GOTO %s", USERCONFIGROOM);
	serv_getln(buf, sizeof buf);
	if (buf[0] != '2') { /* try to create the config room if not there */
		serv_printf("CRE8 1|%s|4|0", USERCONFIGROOM);
		serv_getln(buf, sizeof buf);
		serv_printf("GOTO %s", USERCONFIGROOM);
		serv_getln(buf, sizeof buf);
		if (buf[0] != '2') return(1);
	}
	return(0);
}

/**
 * \brief save the modifications
 */
void save_preferences(void) {
	char buf[SIZ];
	long msgnum = 0L;
	
	if (goto_config_room() != 0) return;	/* oh well. */
	serv_puts("MSGS ALL|0|1");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '8') {
		serv_puts("subj|__ WebCit Preferences __");
		serv_puts("000");
	}
	while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		msgnum = atol(buf);
	}

	if (msgnum > 0L) {
		serv_printf("DELE %ld", msgnum);
		serv_getln(buf, sizeof buf);
	}

	serv_printf("ENT0 1||0|1|__ WebCit Preferences __|");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '4') {
		long len;
		HashPos *HashPos;
		HashList *Hash;
		void *Value;
		const char *Key;
		StrBuf *Buf;
		StrBuf *SubBuf = NULL;
		
		Hash = WC->hash_prefs;
#ifdef DBG_PREFS_HASH
		dbg_PrintHash(Hash, PrintPref, NULL);
#endif
		HashPos = GetNewHashPos(Hash, 0);
		while (GetNextHashPos(Hash, HashPos, &len, &Key, &Value)!=0)
		{
			size_t nchars;
			Buf = (StrBuf*) Value;
			if (Buf == NULL)
				continue;
			nchars = StrLength(Buf);
			if (nchars > 80){
				int n = 0;
				size_t offset, nchars;
				if (SubBuf == NULL)
					SubBuf = NewStrBuf();
				nchars = 1;
				offset = 0;
				while (nchars > 0) {
					if (n == 0)
						nchars = 70;
					else 
						nchars = 80;

					nchars = StrBufSub(SubBuf, Buf, offset, nchars);
					
					if (n == 0)
						serv_printf("%s|%s", Key, ChrPtr(SubBuf));
					else
						serv_printf(" %s", ChrPtr(SubBuf));

					offset += nchars;
					nchars = StrLength(Buf) - offset;
					n++;
				}
				
			}
			else
				serv_printf("%s|%s", Key, ChrPtr(Buf));
			
		}
		if (SubBuf != NULL)
			FreeStrBuf(&SubBuf);
		serv_puts("");
		serv_puts("000");
		DeleteHashPos(&HashPos);
	}

	/** Go back to the room we're supposed to be in */
	serv_printf("GOTO %s", WC->wc_roomname);
	serv_getln(buf, sizeof buf);
}

/**
 * \brief query the actual setting of key in the citadel database
 * \param key config key to query
 * \param keylen length of the key string
 * \param value StrBuf-value to the key to get
 * \returns found?
 */
int get_PREFERENCE(const char *key, size_t keylen, StrBuf **value)
{
	void *hash_value = NULL;
#ifdef DBG_PREFS_HASH
	dbg_PrintHash(WC->hash_prefs, PrintPref, NULL);
#endif
	if (GetHash(WC->hash_prefs, key, keylen, &hash_value) == 0) {
		*value = NULL;
		return 0;
	}
	else {
		*value = NULL;
		*value = (StrBuf*) hash_value;
		return 1;
	}
}

/**
 * \brief	Write a key into the webcit preferences database for this user
 *
 * \params	key		key whichs value is to be modified
 * \param keylen length of the key string
 * \param	value		value to set
 * \param	save_to_server	1 = flush all data to the server, 0 = cache it for now
 */
void set_PREFERENCE(const char *key, size_t keylen, StrBuf *value, int save_to_server) {
	
	Put(WC->hash_prefs, key, keylen, value, HFreeStrBuf);
	
	if (save_to_server) save_preferences();
}

int get_PREF_LONG(const char *key, size_t keylen, long *value, long Default)
{
	int ret;
	StrBuf *val;
	ret = get_PREFERENCE(key, keylen, &val);
	if (ret) {
		*value = atol(ChrPtr(val));
	}
	else {
		*value = Default;
	}

	return ret;
}


void set_PREF_LONG(const char *key, size_t keylen, long value, int save_to_server)
{
	StrBuf *val;
	if (get_PREFERENCE(key, keylen, &val)) {
		StrBufPrintf(val, "%ld", value);
	}
	else {
		val = NewStrBuf();
		StrBufPrintf(val, "%ld", value);
		set_PREFERENCE(key, keylen, val, save_to_server);
	}
}



int get_PREF_YESNO(const char *key, size_t keylen, int *value, int Default)
{
	int ret;
	StrBuf *val;
	ret = get_PREFERENCE(key, keylen, &val);
	if (ret) {
		*value = strcmp(ChrPtr(val), "yes") == 0;
	}
	else {
		*value = Default;
	}

	return ret;
}

void set_PREF_YESNO(const char *key, size_t keylen, int value, int save_to_server)
{
	StrBuf *val;
	if (get_PREFERENCE(key, keylen, &val)) {
		StrBufPrintf(val, "%s", (value)?"yes":"no");
	}
	else {
		val = NewStrBuf();
		StrBufPrintf(val, "%s", (value)?"yes":"no");
		set_PREFERENCE(key, keylen, val, save_to_server);
	}
}

StrBuf *get_ROOM_PREFS(const char *key, size_t keylen)
{
	StrBuf *pref_name, *pref_value;
	
	pref_name = NewStrBuf ();
	StrBufPrintf(pref_name, "%s %s", key, WC->wc_roomname);
	get_pref(pref_name, &pref_value);
	FreeStrBuf(&pref_name);
	return pref_value;
}

void set_ROOM_PREFS(const char *key, size_t keylen, StrBuf *value, int save_to_server)
{
	StrBuf *pref_name;
	
	pref_name = NewStrBuf ();
	StrBufPrintf(pref_name, "%s %s", key, WC->wc_roomname);
	set_PREFERENCE(ChrPtr(pref_name), StrLength(pref_name), value, save_to_server);
	FreeStrBuf(&pref_name);
}

/** 
 * \brief display form for changing your preferences and settings
 */
void display_preferences(void)
{
	output_headers(1, 1, 1, 0, 0, 0);
	StrBuf *ebuf = NULL;
	int i;
	long DayEnd, DayStart, WeekStart;
	int UseSig, ShowEmptyFloors;
	int time_format;
	time_t tt;
	struct tm tm;
	char daylabel[32];
	StrBuf *Buf;
	StrBuf *Signature;

	time_format = get_time_format_cached ();

        wprintf("<div class=\"box\">\n");
        wprintf("<div class=\"boxlabel\">");
        wprintf(_("Preferences and settings"));
        wprintf("</div>");

        wprintf("<div class=\"boxcontent\">");

	/** begin form */
	wprintf("<form name=\"prefform\" action=\"set_preferences\" "
		"method=\"post\">\n");
	wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);

	/** begin table */
        wprintf("<table class=\"altern\">\n");

	/**
	 * Room list view
	 */
	get_preference("roomlistview", &Buf);
	wprintf("<tr class=\"even\"><td>");
	wprintf(PrefGetLocalStr(HKEY("roomlistview")));
	wprintf("</td><td>");

	wprintf("<input type=\"radio\" name=\"roomlistview\" VALUE=\"folders\"");
	if (!strcasecmp(ChrPtr(Buf), "folders")) wprintf(" checked");
	wprintf(">");
	wprintf(_("Tree (folders) view"));
	wprintf("</input>&nbsp;&nbsp;&nbsp;");

	wprintf("<input type=\"radio\" name=\"roomlistview\" VALUE=\"rooms\"");
	if (IsEmptyStr(ChrPtr(Buf)) || !strcasecmp(ChrPtr(Buf), "rooms")) wprintf(" checked");
	wprintf(">");
	wprintf(_("Table (rooms) view"));
	wprintf("</input>\n");

	wprintf("</td></tr>\n");

	/**
	 * Time hour format
	 */

	wprintf("<tr class=\"odd\"><td>");
	wprintf(PrefGetLocalStr(HKEY("calhourformat")));
	wprintf("</td><td>");

	wprintf("<input type=\"radio\" name=\"calhourformat\" VALUE=\"12\"");
	if (time_format == WC_TIMEFORMAT_AMPM) 
		wprintf(" checked");
	wprintf(">");
	wprintf(_("12 hour (am/pm)"));
	wprintf("</input>&nbsp;&nbsp;&nbsp;");

	wprintf("<input type=\"radio\" name=\"calhourformat\" VALUE=\"24\"");
	if (time_format == WC_TIMEFORMAT_24)
		wprintf(" checked");
	wprintf(">");
	wprintf(_("24 hour"));
	wprintf("</input>\n");

	wprintf("</td></tr>\n");

	/**
	 * Calendar day view -- day start time
	 */
	get_pref_long("daystart", &DayStart, 8);

	wprintf("<tr class=\"even\"><td>");
	wprintf(PrefGetLocalStr(HKEY("daystart")));
	wprintf("</td><td>");

	wprintf("<select name=\"daystart\" size=\"1\">\n");
	for (i=0; i<=23; ++i) {

		if (time_format == WC_TIMEFORMAT_24) {
			wprintf("<option %s value=\"%d\">%d:00</option>\n",
				((DayStart == i) ? "selected" : ""),
				i, i
			);
		}
		else {
			wprintf("<option %s value=\"%d\">%s</option>\n",
				((DayStart == i) ? "selected" : ""),
				i, hourname[i]
			);
		}

	}
	wprintf("</select>\n");
	wprintf("</td></tr>\n");

	/**
	 * Calendar day view -- day end time
	 */
	get_pref_long("dayend", &DayEnd, 17);

	wprintf("<tr class=\"odd\"><td>");
	wprintf(PrefGetLocalStr(HKEY("dayend")));
	wprintf("</td><td>");

	wprintf("<select name=\"dayend\" size=\"1\">\n");
	for (i=0; i<=23; ++i) {

		if (time_format == WC_TIMEFORMAT_24) {
			wprintf("<option %s value=\"%d\">%d:00</option>\n",
				((DayEnd == i) ? "selected" : ""),
				i, i
			);
		}
		else {
			wprintf("<option %s value=\"%d\">%s</option>\n",
				((DayEnd == i) ? "selected" : ""),
				i, hourname[i]
			);
		}

	}
	wprintf("</select>\n");
	wprintf("</td></tr>\n");

	/**
	 * Day of week to begin calendar month view
	 */
	get_pref_long("weekstart", &WeekStart, 17);
	wprintf("<tr class=\"even\"><td>");
	wprintf(PrefGetLocalStr(HKEY("weekstart")));
	wprintf("</td><td>");

	wprintf("<select name=\"weekstart\" size=\"1\">\n");

	for (i=0; i<=1; ++i) {
                tt = time(NULL);
                localtime_r(&tt, &tm);
		tm.tm_wday = i;
                wc_strftime(daylabel, sizeof daylabel, "%A", &tm);

		wprintf("<option %s value=\"%d\">%s</option>\n",
			((WeekStart == i) ? "selected" : ""),
			i, daylabel
		);
	}

	wprintf("</select>\n");
	wprintf("</td></tr>\n");

	/**
	 * Signature
	 */
	get_pref_yesno("use_sig", &UseSig, 0);
	wprintf("<tr class=\"odd\"><td>");
	wprintf(_("Attach signature to email messages?"));
	wprintf("</td><td>");

	wprintf("	<script type=\"text/javascript\">					"
		"	function show_or_hide_sigbox() {					"
		"		if ( $F('yes_sig') ) {						"
		"			$('signature_box').style.display = 'inline';		"
		"		}								"
		"		else {								"
		"			$('signature_box').style.display = 'none';		"
		"		}								"
		"	}									"
		"	</script>								"
	);

	wprintf(PrefGetLocalStr(HKEY("use_sig")));

	wprintf("<input type=\"radio\" id=\"no_sig\" name=\"use_sig\" VALUE=\"no\"");
	if (!UseSig) wprintf(" checked");
	wprintf(" onChange=\"show_or_hide_sigbox();\" >");
	wprintf(_("No signature"));
	wprintf("</input>&nbsp,&nbsp;&nbsp;\n");

	wprintf("<input type=\"radio\" id=\"yes_sig\" name=\"use_sig\" VALUE=\"yes\"");
	if (UseSig) wprintf(" checked");
	wprintf(" onChange=\"show_or_hide_sigbox();\" >");
	wprintf(PrefGetLocalStr(HKEY("signature")));
	wprintf("<div id=\"signature_box\">"
		"<br><textarea name=\"signature\" cols=\"40\" rows=\"5\">"
	);

	get_preference("signature", &Signature);
	ebuf = NewStrBuf();
	StrBufEUid_unescapize(ebuf, Signature);
	StrEscPuts(ebuf);
	FreeStrBuf(&ebuf);
	wprintf("</textarea>"
		"</div>"
	);

	wprintf("</input>\n");

	wprintf("</td></tr>\n");

	wprintf("	<script type=\"text/javascript\">	"
		"	show_or_hide_sigbox();			"
		"	</script>				"
	);

	/** Character set to assume is in use for improperly encoded headers */
	if (!get_preference("default_header_charset", &Buf)) {
		Buf = NewStrBuf();
		StrBufPrintf(Buf, "%s", "UTF-8");
		set_preference("default_header_charset", Buf, 0);
	}
	wprintf("<tr class=\"even\"><td>");
	wprintf(PrefGetLocalStr(HKEY("default_header_charset")));
	wprintf("</td><td>");
	wprintf("<input type=\"text\" NAME=\"default_header_charset\" MAXLENGTH=\"32\" VALUE=\"");
	StrEscPuts(Buf); // here shouldn't be bad chars, so...
	wprintf("\">");
	wprintf("</td></tr>");

	/**
	 * Show empty floors?
	 */

	get_pref_yesno("emptyfloors", &ShowEmptyFloors, 0);
	wprintf("<tr class=\"odd\"><td>");
	wprintf(PrefGetLocalStr(HKEY("emptyfloors")));
	wprintf("</td><td>");

	wprintf("<input type=\"radio\" name=\"emptyfloors\" VALUE=\"yes\"");
	if (ShowEmptyFloors) wprintf(" checked");
	wprintf(">");
	wprintf(_("Yes"));
	wprintf("</input>&nbsp;&nbsp;&nbsp;");

	wprintf("<input type=\"radio\" name=\"emptyfloors\" VALUE=\"no\"");
	if (!ShowEmptyFloors) wprintf(" checked");
	wprintf(">");
	wprintf(_("No"));
	wprintf("</input>\n");

	wprintf("</td></tr>\n");

	/** end table */
	wprintf("</table>\n");

	/** submit buttons */
	wprintf("<div class=\"buttons\"> ");
	wprintf("<input type=\"submit\" name=\"change_button\" value=\"%s\">"
		"&nbsp;"
		"<INPUT type=\"submit\" name=\"cancel_button\" value=\"%s\">\n",
		_("Change"),
		_("Cancel")
	);
	wprintf("</div>\n");

	/** end form */
	wprintf("</form>\n");
	wprintf("</div>\n");
	wDumpContent(1);
}

/**
 * \brief Commit new preferences and settings
 */
void set_preferences(void)
{
	long fmt;
	StrBuf *buf, *encBuf;
	int *time_format_cache;
	
	 time_format_cache = &(WC->time_format_cache);

	 if (!havebstr("change_button")) {
		 safestrncpy(WC->ImportantMessage, 
			 _("Cancelled.  No settings were changed."),
			 sizeof WC->ImportantMessage);
		 display_main_menu();
		 return;
	 }

	 /**
	  * Set the last argument to 1 only for the final setting, so
	  * we don't send the prefs file to the server repeatedly
	  */
	 set_preference("roomlistview", NewStrBufPlain(bstr("roomlistview"), -1), 0);
	 fmt = lbstr("calhourformat");
	 set_pref_long("calhourformat", fmt, 0);
	 if (fmt == 24) 
		 *time_format_cache = WC_TIMEFORMAT_24;
	 else
		 *time_format_cache = WC_TIMEFORMAT_AMPM;

	 set_pref_long("weekstart", lbstr("weekstart"), 0);
	 set_pref_yesno("use_sig", yesbstr("use_sig"), 0);
	 set_pref_long("daystart", lbstr("daystart"), 0);
	 set_pref_long("dayend", lbstr("dayend"), 0);
	 set_preference("default_header_charset", NewStrBufPlain(bstr("default_header_charset"), -1), 0);
	 set_preference("emptyfloors", NewStrBufPlain(bstr("emptyfloors"), -1), 0);
	 set_preference("defaultfrom", NewStrBufDup(sbstr("defaultfrom")), 0);
	 set_preference("defaultname", NewStrBufDup(sbstr("defaultname")), 0);
	 set_preference("defaulthandle", NewStrBufDup(sbstr("defaulthandle")), 0);


	buf = NewStrBufPlain(bstr("signature"), -1);
	encBuf = NewStrBuf();
	StrBufEUid_escapize(encBuf, buf);
	set_preference("signature", encBuf, 1);
	FreeStrBuf(&buf);

	display_main_menu();
}


#define PRF_STRING 1
#define PRF_INT 2
#define PRF_QP_STRING 3
#define PRF_YESNO 4


void tmplput_CFG_Value(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *Context, int ContextType)
{
	StrBuf *Setting;
	if (get_PREFERENCE(Tokens->Params[0]->Start,
			   Tokens->Params[0]->len,
			   &Setting))
	StrBufAppendTemplate(Target, nArgs, Tokens, Context, ContextType, Setting, 1);
}

void tmplput_CFG_Descr(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *Context, int ContextType)
{
	const char *SettingStr;
	SettingStr = PrefGetLocalStr(Tokens->Params[0]->Start,
				     Tokens->Params[0]->len);
	if (SettingStr != NULL) 
		StrBufAppendBufPlain(Target, SettingStr, -1, 0);
}


void CfgZoneTempl(StrBuf *TemplBuffer, void *vContext, WCTemplateToken *Token)
{
	StrBuf *Zone = (StrBuf*) vContext;

	SVPutBuf("ZONENAME", Zone, 1);
}

int ConditionalPreference(WCTemplateToken *Tokens, void *Context, int ContextType)
{
	StrBuf *Pref;

	if (!get_PREFERENCE(Tokens->Params[2]->Start,
			    Tokens->Params[2]->len,
			    &Pref)) 
		return 0;
	
	if (Tokens->nParameters == 3) {
		return 1;
	}
	else if (Tokens->Params[3]->Type == TYPE_STR)
		return ((Tokens->Params[3]->len == StrLength(Pref)) &&
			(strcmp(Tokens->Params[3]->Start, ChrPtr(Pref)) == 0));
	else 
		return (StrTol(Pref) == Tokens->Params[3]->lvalue);
}

HashList *GetGVEAHash(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *Context, int ContextType)
{
	StrBuf *Rcp;
	HashList *List = NULL;
	int Done = 0;
	int i, n = 1;
	char N[64];

	Rcp = NewStrBuf();
	serv_puts("GVEA");
	StrBuf_ServGetln(Rcp);
	if (ChrPtr(Rcp)[0] == '1') {
		FlushStrBuf(Rcp);
		List = NewHash(1, NULL);
		while (!Done && (StrBuf_ServGetln(Rcp)>=0)) {
			if ( (StrLength(Rcp)==3) && 
			     !strcmp(ChrPtr(Rcp), "000")) 
			{
				Done = 1;
			}
			else {
				i = snprintf(N, sizeof(N), "%d", n);
				StrBufTrim(Rcp);
				Put(List, N, i, Rcp, HFreeStrBuf);
				Rcp = NewStrBuf();
			}
			n++;
		}
	}
	FreeStrBuf(&Rcp);
	return List;
}
void DeleteGVEAHash(HashList **KillMe)
{
	DeleteHash(KillMe);
}

HashList *GetGVSNHash(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *Context, int ContextType)
{
	StrBuf *Rcp;
	HashList *List = NULL;
	int Done = 0;
	int i, n = 1;
	char N[64];

	Rcp = NewStrBuf();
	serv_puts("GVSN");
	StrBuf_ServGetln(Rcp);
	if (ChrPtr(Rcp)[0] == '1') {
		FlushStrBuf(Rcp);
		List = NewHash(1, NULL);
		while (!Done && (StrBuf_ServGetln(Rcp)>=0)) {
			if ( (StrLength(Rcp)==3) && 
			     !strcmp(ChrPtr(Rcp), "000")) 
			{
				Done = 1;
			}
			else {
				i = snprintf(N, sizeof(N), "%d", n);
				StrBufTrim(Rcp);
				Put(List, N, i, Rcp, HFreeStrBuf);
				Rcp = NewStrBuf();
			}
			n++;
		}
	}
	FreeStrBuf(&Rcp);
	return List;
}
void DeleteGVSNHash(HashList **KillMe)
{
	DeleteHash(KillMe);
}


void 
InitModule_PREFERENCES
(void)
{
	WebcitAddUrlHandler(HKEY("display_preferences"), display_preferences, 0);
	WebcitAddUrlHandler(HKEY("set_preferences"), set_preferences, 0);

	RegisterPreference("roomlistview",_("Room list view"),PRF_STRING);
	RegisterPreference("calhourformat",_("Time format"), PRF_INT);
	RegisterPreference("daystart", _("Calendar day view begins at:"), PRF_INT);
	RegisterPreference("dayend", _("Calendar day view ends at:"), PRF_INT);
	RegisterPreference("weekstart",_("Week starts on:"), PRF_INT);

	RegisterPreference("use_sig",_("Attach signature to email messages?"), PRF_YESNO);
	RegisterPreference("signature",_("Use this signature:"),PRF_QP_STRING);
	RegisterPreference("default_header_charset", _("Default character set for email headers:") ,PRF_STRING);
	RegisterPreference("emptyfloors", _("Show empty floors"), PRF_YESNO);
	RegisterPreference("defaultfrom", _("Prefered Email Address"), PRF_STRING);
	RegisterPreference("defaultname", _("Prefered Email Sendername"), PRF_STRING);
	RegisterPreference("defaulthandle", _("Prefered Name for posting messages"), PRF_STRING);

	
	RegisterNamespace("PREF:VALUE", 1, 2, tmplput_CFG_Value, CTX_NONE);
	RegisterNamespace("PREF:DESCR", 1, 1, tmplput_CFG_Descr, CTX_NONE);
	RegisterIterator("PREF:ZONE", 0, ZoneHash, NULL, CfgZoneTempl, NULL, CTX_PREF, CTX_NONE);

	RegisterConditional(HKEY("COND:PREF"), 4, ConditionalPreference, CTX_NONE);
	
	RegisterIterator("PREF:VALID:EMAIL:ADDR", 0, NULL, 
			 GetGVEAHash, NULL, DeleteGVEAHash, CTX_STRBUF, CTX_NONE);
	RegisterIterator("PREF:VALID:EMAIL:NAME", 0, NULL, 
			 GetGVSNHash, NULL, DeleteGVSNHash, CTX_STRBUF, CTX_NONE);

}
/*@}*/
