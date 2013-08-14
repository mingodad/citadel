/*
 * Displays and customizes the iconbar.
 *
 * Copyright (c) 1996-2012 by the citadel.org team
 *
 * This program is open source software.  You can redistribute it and/or
 * modify it under the terms of the GNU General Public License, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "webcit.h"

/* Values for ib_displayas ... don't change these or you will break the templates */
#define IB_PICTEXT	0	/* picture and text */
#define IB_PICONLY	1	/* just a picture */
#define IB_TEXTONLY	2	/* just text */

void DontDeleteThis(void *Data){}

#define IconbarIsEnabled(a, b) IconbarIsENABLED(a, sizeof(a) - 1, b)


HashList *IB_Seeting_Order = NULL;
typedef struct _dflt_IB_Setting {
	int         DefVal;  /* default value for non-set users */
	long        n;       /* counter for internal purposes   */
	const char *Key;     /* Stringvalue */
	long        len;     /* Length... */
}dflt_IB_Setting;

long nIBV = 0;
dflt_IB_Setting IconbarDefaults[] = {
	{0,  0, HKEY("unused")},
	{0,  1, HKEY("ib_displayas")},
	{0,  2, HKEY("ib_logo")},
	{1,  3, HKEY("ib_summary")},
	{1,  4, HKEY("ib_inbox")},
	{1,  5, HKEY("ib_calendar")},
	{1,  6, HKEY("ib_contacts")},
	{1,  7, HKEY("ib_notes")},
	{1,  8, HKEY("ib_tasks")},
	{1,  9, HKEY("ib_rooms")},
	{1, 10, HKEY("ib_users")},
	{1, 11, HKEY("ib_chat")},
	{1, 12, HKEY("ib_advanced")},
	{1, 13, HKEY("ib_logoff")},
	{1, 14, HKEY("ib_citadel")},
	{0, 15, HKEY("")}
};

HashList *IBDfl = NULL;


long IconbarIsENABLED(long val, const char *key, size_t keylen)
{
	void *vIBDfl = NULL;
	wcsession *WCC = WC;

	if ((WCC != NULL) && 
	    (WCC->IBSettingsVec != NULL) && 
	    (val < nIBV))
	{
		return WCC->IBSettingsVec[val];
	}
	if (GetHash(IBDfl, key, keylen, &vIBDfl)) {
		dflt_IB_Setting *Set = (dflt_IB_Setting*)vIBDfl;
		return Set->DefVal;
	}
	else 
		return 1;
}

#ifdef DBG_ICONBAR_HASH
static char nbuf[32];
inline const char *PrintInt(void *Prefstr)
{
	snprintf(nbuf, sizeof(nbuf), "%ld", (long)Prefstr);
	return nbuf;
}
#endif

/* 
	hprintf("Cache-Control: private\r\n");
*/


int ConditionalIsActiveStylesheet(StrBuf *Target, WCTemplputParams *TP) {
	long testFor;
	long lookAt;
	long ib_displayas;

	lookAt = GetTemplateTokenNumber(Target, TP, 3, IB_PICTEXT);
	testFor = GetTemplateTokenNumber(Target, TP, 2, IB_PICTEXT);



	ib_displayas = IconbarIsENABLED(lookAt, TKEY(3));
/*
	printf ("%ld == %ld ? %s : %s\n", 
		testFor, 
		ib_displayas, 
		IconbarDefaults[lookAt ].Key, 
		ChrPtr(TP->Tokens->FlatToken));
*/

	return (testFor == ib_displayas);
}

void LoadIconSettings(StrBuf *iconbar, long lvalue)
{
	void *vIBDfl;
	dflt_IB_Setting *Set;
	const char *pCh = NULL;

	wcsession *WCC = WC;
	StrBuf *buf;
	StrBuf *key;
	long val;

	buf = NewStrBuf();
	key = NewStrBuf();
	if (WCC->IBSettingsVec == NULL)
	{
		WCC->IBSettingsVec = (long*) malloc (nIBV * sizeof(long));
	}
	/*
	 * The initialized values of these variables also happen to
	 * specify the default values for users who haven't customized
	 * their iconbars.  These should probably be set in a master
	 * configuration somewhere.
	 */

	while (StrBufExtract_NextToken(buf, iconbar, &pCh,  ',') >= 0)
	{
		StrBufExtract_token(key, buf, 0, '=');
		val = StrBufExtract_long(buf, 1, '=');

		if (!GetHash(IBDfl, SKEY(key), &vIBDfl)) 
			continue;
		Set = (dflt_IB_Setting*)vIBDfl;

		WCC->IBSettingsVec[Set->n] = val;
/*		printf("%ld %s %s -> %ld \n", Set->n, Set->Key, IconbarDefaults[Set->n].Key, val);*/
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


/*
 * Display the icon bar as long as we have an active session,
 * and either the user is logged in or the server allows guest mode.
 */
void tmplput_iconbar(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;
	
	 if ( (WCC != NULL)     &&
	      ((WCC->logged_in) ||
	      ((WCC->serv_info != NULL) &&
	       (WCC->serv_info->serv_supports_guest))
		      ) )
	 {
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
	int i = 1;
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


int ConditionalWholistExpanded(StrBuf *Target, WCTemplputParams *TP)
{
	int r = 0;
	if (WC) r = WC->ib_wholist_expanded;
	syslog(LOG_DEBUG, "ConditionalWholistExpanded() returns %d", r);
	return(r);
}


int ConditionalRoomlistExpanded(StrBuf *Target, WCTemplputParams *TP)
{
	if (WC) return(WC->ib_roomlist_expanded);
	return(0);
}



/*
 * Toggle the roomlist expanded state in session memory
 */
void toggle_roomlist_expanded_state(void) {
	wcsession *WCC = WC;

	if (!WCC) {
		wc_printf("no session");
		return;
	}

	WCC->ib_roomlist_expanded = IBSTR("wstate");
	wc_printf("%d", WCC->ib_roomlist_expanded);
	syslog(LOG_DEBUG, "ib_roomlist_expanded set to %d", WCC->ib_roomlist_expanded);
}


/*
 * Toggle the wholist expanded state in session memory
 */
void toggle_wholist_expanded_state(void) {
	wcsession *WCC = WC;

	if (!WCC) {
		wc_printf("no session");
		return;
	}

	WCC->ib_wholist_expanded = IBSTR("wstate");
	wc_printf("%d", WCC->ib_wholist_expanded);
	syslog(LOG_DEBUG, "ib_wholist_expanded set to %d", WCC->ib_wholist_expanded);
}


void 
InitModule_ICONBAR
(void)
{
	long l;

	/*WebcitAddUrlHandler(HKEY("user_iconbar"), "", 0, doUserIconStylesheet, 0); */
	WebcitAddUrlHandler(HKEY("commit_iconbar"), "", 0, commit_iconbar, 0);
	WebcitAddUrlHandler(HKEY("toggle_wholist_expanded_state"), "", 0, toggle_wholist_expanded_state, AJAX);
	WebcitAddUrlHandler(HKEY("toggle_roomlist_expanded_state"), "", 0, toggle_roomlist_expanded_state, AJAX);
	RegisterConditional("COND:ICONBAR:ACTIVE", 3, ConditionalIsActiveStylesheet, CTX_NONE);
	RegisterNamespace("ICONBAR", 0, 0, tmplput_iconbar, NULL, CTX_NONE);
	RegisterConditional("COND:ICONBAR:WHOLISTEXPANDED", 0, ConditionalWholistExpanded, CTX_NONE);
	RegisterConditional("COND:ICONBAR:ROOMLISTEXPANDED", 0, ConditionalRoomlistExpanded, CTX_NONE);

	RegisterPreference("iconbar", _("Iconbar Setting"), PRF_STRING, LoadIconSettings);
	l = 1;
	while (IconbarDefaults[l].len != 0)
	{
		RegisterTokenParamDefine(IconbarDefaults[l].Key, 
					 IconbarDefaults[l].len, l);
		l ++;
	}
	nIBV = l;
}



void 
SessionDestroyModule_ICONBAR
(wcsession *sess)
{
	if (sess->IBSettingsVec != NULL)
		free(sess->IBSettingsVec);
}

