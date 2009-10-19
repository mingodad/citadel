/*
 * $Id$
 *
 * Displays and customizes the iconbar.
 */

#include "webcit.h"

/* Values for ib_displayas ... don't change these or you will break the templates */
#define IB_PICTEXT	0	/* picture and text */
#define IB_PICONLY	1	/* just a picture */
#define IB_TEXTONLY	2	/* just text */

void DontDeleteThis(void *Data){}

#define IconbarIsEnabled(a, b) IconbarIsENABLED(a, sizeof(a) - 1, b)



typedef struct _dflt_IB_Setting {
	int DefVal;
	const char *Key;
	long len;
}dflt_IB_Setting;

dflt_IB_Setting IconbarDefaults[] = {
	{0, HKEY("ib_displayas")},
	{0, HKEY("ib_logo")},
	{1, HKEY("ib_summary")},
	{1, HKEY("ib_inbox")},
	{1, HKEY("ib_calendar")},
	{1, HKEY("ib_contacts")},
	{1, HKEY("ib_notes")},
	{1, HKEY("ib_tasks")},
	{1, HKEY("ib_rooms")},
	{1, HKEY("ib_users")},
	{1, HKEY("ib_chat")},
	{1, HKEY("ib_advanced")},
	{1, HKEY("ib_logoff")},
	{1, HKEY("ib_citadel")},
	{0, HKEY("")}
};

HashList *IBDfl = NULL;

long IconbarGetDefault(const char *key, size_t keylen)
{
	void *vIBDfl;

	if (GetHash(IBDfl, key, keylen, &vIBDfl)) {
		dflt_IB_Setting *Set = (dflt_IB_Setting*)vIBDfl;
		return Set->DefVal;
	}
	return 0;
}
long IconbarIsENABLED(const char *key, size_t keylen, long defval)
{
	void *Data = NULL;
	wcsession *WCC = WC;

	if (WCC == NULL) 
		return defval;

	if (GetHash(WCC->IconBarSettings, 
		    key, 
		    keylen,
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

/* Produces a stylesheet which hides any iconbar icons the user does not want */
void doUserIconStylesheet(void) {
	HashPos *pos;
	void *Data;
	long value;
	const char *key;
	long HKLen;
	
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
	long testFor;
	int ib_displayas;

	testFor = GetTemplateTokenNumber(Target, TP, 3, IB_PICTEXT);
	ib_displayas = IconbarIsENABLED(TKEY(2),IconbarGetDefault(TKEY(2)));
	return (testFor == ib_displayas);
}

void LoadIconSettings(StrBuf *iconbar, long lvalue)
{
	wcsession *WCC = WC;
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

	nTokens = StrBufNum_tokens(iconbar, ',');
	for (i=0; i<nTokens; ++i) {
		StrBufExtract_token(buf, iconbar, i, ',');
		StrBufExtract_token(key, buf, 0, '=');
		val = StrBufExtract_long(buf, 1, '=');
		Put(WCC->IconBarSettings, 
		    ChrPtr(key), StrLength(key),
		    (void*)val, DontDeleteThis);
	}

#ifdef DBG_ICONBAR_HASH
	dbg_PrintHash(WCC->IconBarSetttings, PrintInt, NULL);
#endif

	FreeStrBuf(&key);
	FreeStrBuf(&buf);
}


/*
 * save changes to iconbar settings
 */
void commit_iconbar(void) {
	const StrBuf *MimeType;
	StrBuf *iconbar;
	StrBuf *buf;
	int i;


	if (!havebstr("ok_button")) {
		display_main_menu();
		return;
	}

	iconbar = NewStrBuf();
	buf = NewStrBuf();
	StrBufPrintf(iconbar, "ib_displayas=%d", ibstr("ib_displayas"));
	for (i=0; i<(sizeof(IconbarDefaults)/sizeof(dflt_IB_Setting )); ++i) {
		char *Val;
		if (!strcasecmp(Bstr(IconbarDefaults[i].Key,
				     IconbarDefaults[i].len),
				"yes")) 
		{
			Val = "1";
		}
		else if (!strcasecmp(Bstr(IconbarDefaults[i].Key,
					  IconbarDefaults[i].len),
				     "yeslist")) 
		{
			Val = "2";
		}
		else {
			Val = "0";
		}
		StrBufPrintf(buf, ",%s=%s", IconbarDefaults[i].Key, Val);
		StrBufAppendBuf(iconbar, buf, 0);

	}
	FreeStrBuf(&buf);
	set_preference("iconbar", iconbar, 1);


	begin_burst();
	MimeType = DoTemplate(HKEY("iconbar_save"), NULL, &NoCtx);
	http_transmit_thing(ChrPtr(MimeType), 0);
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
ServerShutdownModule_ICONBAR
(void)
{
	DeleteHash(&IBDfl);
}



void
ServerStartModule_ICONBAR
(void)
{
	int i = 0;
	IBDfl = NewHash(1, NULL);

	while (IconbarDefaults[i].len != 0)
	{
		Put(IBDfl, 
		    IconbarDefaults[i].Key, 
		    IconbarDefaults[i].len, 
		    &IconbarDefaults[i], 
		    reference_free_handler);
		i++;
	}
}

void 
InitModule_ICONBAR
(void)
{
	WebcitAddUrlHandler(HKEY("user_iconbar"), doUserIconStylesheet, 0);
	WebcitAddUrlHandler(HKEY("commit_iconbar"), commit_iconbar, 0);
	RegisterConditional(HKEY("COND:ICONBAR:ACTIVE"), 3, ConditionalIsActiveStylesheet, CTX_NONE);
	RegisterNamespace("ICONBAR", 0, 0, tmplput_iconbar, NULL, CTX_NONE);

	RegisterPreference("iconbar", _("Iconbar Setting"), PRF_STRING, LoadIconSettings);
}



void 
SessionDestroyModule_ICONBAR
(wcsession *sess)
{
	DeleteHash(&sess->IconBarSettings);
}
