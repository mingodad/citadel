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


void ParsePref(HashList **List, StrBuf *ReadBuf)
{
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
				Put(*List, 
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


/*
 * display preferences dialog
 */
void load_preferences(void) 
{
	StrBuf *ReadBuf;
	char buf[SIZ];
	long msgnum = 0L;
	
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
				ParsePref(&WC->hash_prefs, ReadBuf);
			}
		}
		FreeStrBuf(&ReadBuf);
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

void WritePrefsToServer(HashList *Hash)
{
	long len;
	HashPos *HashPos;
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
	DeleteHashPos(&HashPos);
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

		WritePrefsToServer(WC->hash_prefs);
		serv_puts("");
		serv_puts("000");
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

/*
 * Offer to make any page the user's "start page."
 */
void offer_start_page(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType) {
	wprintf("<a href=\"change_start_page?startpage=");
	urlescputs(WC->this_page);
	wprintf("\">");
	wprintf(_("Make this my start page"));
	wprintf("</a>");
#ifdef TECH_PREVIEW
	wprintf("<br/><a href=\"rss?room=");
	urlescputs(WC->wc_roomname);
	wprintf("\" title=\"RSS 2.0 feed for ");
	escputs(WC->wc_roomname);
	wprintf("\"><img alt=\"RSS\" border=\"0\" src=\"static/xml_button.gif\"/></a>\n");
#endif
}


/*
 * Change the user's start page
 */
void change_start_page(void) {

	if (bstr("startpage") == NULL) {
		safestrncpy(WC->ImportantMessage,
			_("You no longer have a start page selected."),
			sizeof WC->ImportantMessage);
		display_main_menu();
		return;
	}

	set_preference("startpage", NewStrBufPlain(bstr("startpage"), -1), 1);

	output_headers(1, 1, 0, 0, 0, 0);
	do_template("newstartpage", NULL);
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
	if (get_PREFERENCE(TKEY(0), &Setting))
	StrBufAppendTemplate(Target, nArgs, Tokens, Context, ContextType, Setting, 1);
}

void tmplput_CFG_Descr(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *Context, int ContextType)
{
	const char *SettingStr;
	SettingStr = PrefGetLocalStr(TKEY(0));
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

	if (!get_PREFERENCE(TKEY(2), &Pref)) 
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

int ConditionalHazePreference(WCTemplateToken *Tokens, void *Context, int ContextType)
{
	StrBuf *Pref;

	if (!get_PREFERENCE(TKEY(2), &Pref) || 
	    (Pref == NULL)) 
		return 0;
	else 
		return 1;
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
	WebcitAddUrlHandler(HKEY("set_preferences"), set_preferences, 0);
	WebcitAddUrlHandler(HKEY("change_start_page"), change_start_page, 0);

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
	RegisterIterator("PREF:ZONE", 0, ZoneHash, NULL, CfgZoneTempl, NULL, CTX_PREF, CTX_NONE, IT_NOFLAG);

	RegisterConditional(HKEY("COND:PREF"), 4, ConditionalPreference, CTX_NONE);
	RegisterConditional(HKEY("COND:PREF:SET"), 4, ConditionalHazePreference, CTX_NONE);
	
	RegisterIterator("PREF:VALID:EMAIL:ADDR", 0, NULL, 
			 GetGVEAHash, NULL, DeleteGVEAHash, CTX_STRBUF, CTX_NONE, IT_NOFLAG);
	RegisterIterator("PREF:VALID:EMAIL:NAME", 0, NULL, 
			 GetGVSNHash, NULL, DeleteGVSNHash, CTX_STRBUF, CTX_NONE, IT_NOFLAG);

}
/*@}*/
