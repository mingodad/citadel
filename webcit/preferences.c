/*
 * Manage user preferences with a little help from the Citadel server.
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
#include "webserver.h"
#include "dav.h"

HashList *PreferenceHooks;
extern HashList *HandlerHash;

typedef struct _PrefDef {
	ePrefType eType;
	StrBuf *Setting;
	const char *PrefStr;
	PrefEvalFunc OnLoad;
	StrBuf *OnLoadName;
} PrefDef;

typedef struct _Preference {
	PrefDef *Type;
	ePrefType eFlatPrefType;

	StrBuf *Key;
	StrBuf *Val;

	long lval;
	long decoded;
	StrBuf *DeQPed;
} Preference;

void DestroyPrefDef(void *vPrefDef)
{
	PrefDef *Prefdef = (PrefDef*) vPrefDef;
	FreeStrBuf(&Prefdef->Setting);
	FreeStrBuf(&Prefdef->OnLoadName);
	free(Prefdef);
}

void DestroyPreference(void *vPref)
{
	Preference *Pref = (Preference*) vPref;
	FreeStrBuf(&Pref->Key);
	FreeStrBuf(&Pref->Val);
	FreeStrBuf(&Pref->DeQPed);
	free(Pref);

}

void _RegisterPreference(const char *Setting, long SettingLen, 
			 const char *PrefStr, 
			 ePrefType Type, 
			 PrefEvalFunc OnLoad, 
			 const char *OnLoadName)
{
	PrefDef *Newpref = (PrefDef*) malloc(sizeof(PrefDef));
	Newpref->Setting = NewStrBufPlain(Setting, SettingLen);
	Newpref->PrefStr = PrefStr;
	Newpref->eType = Type;
	Newpref->OnLoad = OnLoad;
	if (Newpref->OnLoad != NULL) {
		Newpref->OnLoadName = NewStrBufPlain(OnLoadName, -1);
	}
	else
		Newpref->OnLoadName = NULL;
	Put(PreferenceHooks, Setting, SettingLen, Newpref, DestroyPrefDef);
}

const char *PrefGetLocalStr(const char *Setting, long len)
{
	void *hash_value;
	if (GetHash(PreferenceHooks, Setting, len, &hash_value) != 0) {
		PrefDef *Newpref = (PrefDef*) hash_value;
		return _(Newpref->PrefStr);

	}
	return "";
}

#ifdef DBG_PREFS_HASH
inline const char *PrintPref(void *vPref)
{
	Preference *Pref = (Preference*) vPref;
	if (Pref->DeQPed != NULL)
		return ChrPtr(Pref->DeQPed);
	else 
		return ChrPtr(Pref->Val);
}
#endif

void GetPrefTypes(HashList *List)
{
	HashPos *It;
	long len;
	const char *Key;
	void *vSetting;
	void *vPrefDef;
	Preference *Pref;
	PrefDef *PrefType;

	It = GetNewHashPos(List, 0);
	while (GetNextHashPos(List, It, &len, &Key, &vSetting)) 
	{
		Pref = (Preference*) vSetting;
		if (GetHash(PreferenceHooks, SKEY(Pref->Key), &vPrefDef) && 
		    (vPrefDef != NULL)) 
		{
			PrefType = (PrefDef*) vPrefDef;
			Pref->Type = PrefType;
			Pref->eFlatPrefType = Pref->Type->eType;

			syslog(1, "Loading [%s]with type [%d] [\"%s\"]\n",
				ChrPtr(Pref->Key),
				Pref->Type->eType,
				ChrPtr(Pref->Val));

			switch (Pref->Type->eType)
			{
			case PRF_UNSET: /* WHUT? */
				break;
			case PRF_STRING:
				break;
			case PRF_INT:
				Pref->lval = StrTol(Pref->Val);
				Pref->decoded = 1;
				break;
			case PRF_QP_STRING:
				Pref->DeQPed = NewStrBufPlain(NULL, StrLength(Pref->Val));
				StrBufEUid_unescapize(Pref->DeQPed, Pref->Val);
				Pref->decoded = 1;
				break;
			case PRF_YESNO:
				Pref->lval = strcmp(ChrPtr(Pref->Val), "yes") == 0;
				Pref->decoded = 1;
				break;
			}

			if (PrefType->OnLoad != NULL){

				syslog(1, "Loading with: -> %s(\"%s\", %ld)\n",
					ChrPtr(PrefType->OnLoadName),
					ChrPtr(Pref->Val),
					Pref->lval);
				PrefType->OnLoad(Pref->Val, Pref->lval);
			}
		}
	}
	DeleteHashPos(&It);
}

void ParsePref(HashList **List, StrBuf *ReadBuf)
{
	int Done = 0;
	Preference *Data = NULL;
	Preference *LastData = NULL;
				
	while (!Done) {
		if (StrBuf_ServGetln(ReadBuf) < 0)
			break;
		if ( (StrLength(ReadBuf)==3) && 
		     !strcmp(ChrPtr(ReadBuf), "000")) {
			Done = 1;
			break;
		}

		if ((ChrPtr(ReadBuf)[0] == ' ') &&
		    (LastData != NULL)) {
			StrBufAppendBuf(LastData->Val, ReadBuf, 1);
		}
		else {
			LastData = Data = malloc(sizeof(Preference));
			memset(Data, 0, sizeof(Preference));
			Data->Key = NewStrBuf();
			Data->Val = NewStrBuf();
			StrBufExtract_token(Data->Key, ReadBuf, 0, '|');
			StrBufExtract_token(Data->Val, ReadBuf, 1, '|');

			/***************** BEGIN VILE SLEAZY HACK ************************/

			/* some users might still have this start page configured, which now breaks */

			if (	(!strcasecmp(ChrPtr(Data->Key), "startpage"))
				&& (!strcasecmp(ChrPtr(Data->Val), "/do_template?template=summary_page"))
			) {
				FreeStrBuf(&Data->Val);
				Data->Val = NewStrBufPlain(HKEY("/summary"));
			}

			/******************* END VILE SLEAZY HACK ************************/

			if (!IsEmptyStr(ChrPtr(Data->Key)))
			{
				Put(*List, 
				    SKEY(Data->Key),
				    Data, 
				    DestroyPreference);
			}
			else 
			{
				StrBufTrim(ReadBuf);
				syslog(1, "ignoring spurious preference line: [%s]\n", 
					ChrPtr(ReadBuf));
				DestroyPreference(Data);
				LastData = NULL;
			}
			Data = NULL;
		}
	}
	GetPrefTypes(*List);
}


/*
 * display preferences dialog
 */
void load_preferences(void) 
{
	folder Room;
	wcsession *WCC = WC;
	int Done = 0;
	StrBuf *ReadBuf;
	long msgnum = 0L;
	
	memset(&Room, 0, sizeof(folder));
	ReadBuf = NewStrBufPlain(NULL, SIZ * 4);
	if (goto_config_room(ReadBuf, &Room) != 0) {
		FreeStrBuf(&ReadBuf);
		FlushFolder(&Room);

		return;	/* oh well. */
	}

	serv_puts("MSGS ALL|0|1");
	StrBuf_ServGetln(ReadBuf);
	if (GetServerStatus(ReadBuf, NULL) == 8) {
		serv_puts("subj|__ WebCit Preferences __");
		serv_puts("000");
	}
	while (!Done && (StrBuf_ServGetln(ReadBuf) >= 0)) {
		if ( (StrLength(ReadBuf)==3) && !strcmp(ChrPtr(ReadBuf), "000")) {
			Done = 1;
			break;
		}
		msgnum = StrTol(ReadBuf);
	}

	if (msgnum > 0L) {
		serv_printf("MSG0 %ld", msgnum);
		StrBuf_ServGetln(ReadBuf);
		if (GetServerStatus(ReadBuf, NULL) == 1) {
			while (	(StrBuf_ServGetln(ReadBuf) >= 0)
				&& (strcmp(ChrPtr(ReadBuf), "text")
				&& strcmp(ChrPtr(ReadBuf), "000"))
			) {
				/* flush */
			}
			if (!strcmp(ChrPtr(ReadBuf), "text")) {
				ParsePref(&WCC->hash_prefs, ReadBuf);
			}
		}
	}

	/* Go back to the room we're supposed to be in */
	if (StrLength(WCC->CurRoom.name) > 0) {
		serv_printf("GOTO %s", ChrPtr(WCC->CurRoom.name));
		StrBuf_ServGetln(ReadBuf);
		GetServerStatus(ReadBuf, NULL);
	}
	FreeStrBuf(&ReadBuf);
	FlushFolder(&Room);
}


/*
 * Go to the user's configuration room, creating it if necessary.
 * returns 0 on success or nonzero upon failure.
 */
int goto_config_room(StrBuf *Buf, folder *Room) 
{
	serv_printf("GOTO %s", USERCONFIGROOM);
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL) != 2) {	/* try to create the config room if not there */
		serv_printf("CRE8 1|%s|4|0", USERCONFIGROOM);
		StrBuf_ServGetln(Buf);
		GetServerStatus(Buf, NULL);

		serv_printf("GOTO %s", USERCONFIGROOM);
		StrBuf_ServGetln(Buf);
		if (GetServerStatus(Buf, NULL) != 2) {
			return(1);
		}
	}
	ParseGoto(Room, Buf);
	return(0);
}

void WritePrefsToServer(HashList *Hash)
{
	wcsession *WCC = WC;
	long len;
	HashPos *HashPos;
	void *vPref;
	const char *Key;
	Preference *Pref;
	StrBuf *SubBuf = NULL;
	
	Hash = WCC->hash_prefs;
#ifdef DBG_PREFS_HASH
	dbg_PrintHash(Hash, PrintPref, NULL);
#endif
	HashPos = GetNewHashPos(Hash, 0);
	while (GetNextHashPos(Hash, HashPos, &len, &Key, &vPref)!=0)
	{
		size_t nchars;
		if (vPref == NULL)
			continue;
		Pref = (Preference*) vPref;
		nchars = StrLength(Pref->Val);
		if (nchars > 80){
			int n = 0;
			size_t offset, nchars;
			if (SubBuf == NULL)
				SubBuf = NewStrBufPlain(NULL, SIZ);
			nchars = 1;
			offset = 0;
			while (nchars > 0) {
				if (n == 0)
					nchars = 70;
				else 
					nchars = 80;
				
				nchars = StrBufSub(SubBuf, Pref->Val, offset, nchars);
				
				if (n == 0) {
					serv_printf("%s|%s", ChrPtr(Pref->Key), ChrPtr(SubBuf));
				}
				else {
					serv_printf(" %s", ChrPtr(SubBuf));
				}
				
				offset += nchars;
				nchars = StrLength(Pref->Val) - offset;
				n++;
			}
			
		}
		else {
			serv_printf("%s|%s", ChrPtr(Pref->Key), ChrPtr(Pref->Val));
		}
		
	}
	FreeStrBuf(&SubBuf);
	DeleteHashPos(&HashPos);
}

/*
 * save the modifications
 */
void save_preferences(void) 
{
	folder Room;
	wcsession *WCC = WC;
	int Done = 0;
	StrBuf *ReadBuf;
	long msgnum = 0L;
	
	ReadBuf = NewStrBuf();
	memset(&Room, 0, sizeof(folder));
	if (goto_config_room(ReadBuf, &Room) != 0) {
		FreeStrBuf(&ReadBuf);
		FlushFolder(&Room);

		return;	/* oh well. */
	}

	/* make shure the config room has the right type, else it might reject our config */
	if (Room.view != VIEW_BBS) {
		serv_printf("VIEW %d", VIEW_BBS);
		StrBuf_ServGetln(ReadBuf);
		if (GetServerStatus(ReadBuf, NULL) != 2) {
			/* UPS? */
		}
		else if (goto_config_room(ReadBuf, &Room) != 0) {
			FreeStrBuf(&ReadBuf);
			FlushFolder(&Room);
			
			return;	/* oh well. */
		}
	}

	serv_puts("MSGS ALL|0|1");
	StrBuf_ServGetln(ReadBuf);
	if (GetServerStatus(ReadBuf, NULL) == 8) {
		serv_puts("subj|__ WebCit Preferences __");
		serv_puts("000");
	}
	while (!Done && (StrBuf_ServGetln(ReadBuf) >= 0)) {
		if ( (StrLength(ReadBuf)==3) && !strcmp(ChrPtr(ReadBuf), "000")) {
			Done = 1;
			break;
		}
		msgnum = StrTol(ReadBuf);
	}

	if (msgnum > 0L) {
		serv_printf("DELE %ld", msgnum);
		StrBuf_ServGetln(ReadBuf);
		GetServerStatus(ReadBuf, NULL);
	}

	serv_printf("ENT0 1||0|1|__ WebCit Preferences __|");
	StrBuf_ServGetln(ReadBuf);
	if (GetServerStatus(ReadBuf, NULL) == 4) {

		WritePrefsToServer(WCC->hash_prefs);
		serv_puts("");
		serv_puts("000");
	}

	/** Go back to the room we're supposed to be in */
	if (StrLength(WCC->CurRoom.name) > 0) {
		serv_printf("GOTO %s", ChrPtr(WCC->CurRoom.name));
		StrBuf_ServGetln(ReadBuf);
		GetServerStatus(ReadBuf, NULL);
	}
	FreeStrBuf(&ReadBuf);
	FlushFolder(&Room);
}

/*
 * query the actual setting of key in the citadel database
 *
 * key		config key to query
 * keylen	length of the key string
 * value	StrBuf-value to the key to get
 * returns:	found?
 */
int get_pref_backend(const char *key, size_t keylen, Preference **Pref)
{
	void *hash_value = NULL;
#ifdef DBG_PREFS_HASH
	dbg_PrintHash(WC->hash_prefs, PrintPref, NULL);
#endif
	if (GetHash(WC->hash_prefs, key, keylen, &hash_value) == 0) {
		*Pref = NULL;
		return 0;
	}
	else {
		*Pref = (Preference*) hash_value;
		return 1;
	}
}

int get_PREFERENCE(const char *key, size_t keylen, StrBuf **value)
{
	Preference *Pref;
	int Ret;

	Ret = get_pref_backend(key, keylen, &Pref);
	if (Ret != 0)
		*value = Pref->Val;
	else
		*value = NULL;
	return Ret;
}

/*
 * Write a key into the webcit preferences database for this user
 *
 * key			key whichs value is to be modified
 * keylen		length of the key string
 * value		value to set
 * save_to_server	1 = flush all data to the server, 0 = cache it for now
 */
long compare_preference(const Preference *PrefA, const Preference *PrefB)
{
	ePrefType TypeA, TypeB;

	if (PrefA->Type != NULL) {
		TypeA = PrefA->Type->eType;
	}
	else {
		TypeA = PrefA->eFlatPrefType;
	}

	if (PrefB->Type != NULL) {
		TypeB = PrefB->Type->eType;
	}
	else {
		TypeB = PrefB->eFlatPrefType;
	}

	if (	(TypeA != PRF_UNSET)
		&& (TypeB != PRF_UNSET)
		&& (TypeA != TypeB)
	) {
		if (TypeA > TypeB) {
			return 1;
		}
		else {	/* (PrefA->Type < PrefB->Type) */
			return -1;
		}
	}

	if (TypeB == PRF_UNSET) {
		TypeA = PRF_UNSET;
	}
		    
	switch (TypeA)
	{
	default:
	case PRF_UNSET:
	case PRF_STRING:
		return strcmp(ChrPtr(PrefA->Val), ChrPtr(PrefB->Val));
	case PRF_YESNO:
	case PRF_INT:
		if (PrefA->lval == PrefB->lval)
			return 0;
		else if (PrefA->lval > PrefB->lval)
			return 1;
		else
			return -1;
	case PRF_QP_STRING:
		return strcmp(ChrPtr(PrefA->DeQPed), 
			      ChrPtr(PrefB->DeQPed));
	}
}


/*
 * Write a key into the webcit preferences database for this user
 *
 * key			key which value is to be modified
 * keylen		length of the key string
 * value		value to set
 * save_to_server	1 = flush all data to the server, 0 = cache it for now
 */
void set_preference_backend(const char *key, size_t keylen, 
			    long lvalue, 
			    StrBuf *value, 
			    long lPrefType,
			    int save_to_server, 
			    PrefDef *PrefType) 
{
	wcsession *WCC = WC;
	void *vPrefDef;
	void *vPrefB;
	Preference *Pref;

	Pref = (Preference*) malloc(sizeof(Preference));
	memset(Pref, 0, sizeof(Preference));
	Pref->Key = NewStrBufPlain(key, keylen);

	if ((PrefType == NULL) &&
	    GetHash(PreferenceHooks, SKEY(Pref->Key), &vPrefDef) && 
	    (vPrefDef != NULL))
		PrefType = (PrefDef*) vPrefDef;

	if (PrefType != NULL)
	{
		Pref->Type = PrefType;
		Pref->eFlatPrefType = PrefType->eType;
		if (Pref->Type->eType != lPrefType)
			syslog(1, "warning: saving preference with wrong type [%s] %d != %ld \n",
				key, Pref->Type->eType, lPrefType);
		switch (Pref->Type->eType)
		{
		case PRF_UNSET: /* default to string... */
		case PRF_STRING:
			Pref->Val = value;
			Pref->decoded = 1;
			break;
		case PRF_INT:
			Pref->lval = lvalue;
			Pref->Val = value;
			if (Pref->Val == NULL)
				Pref->Val = NewStrBufPlain(NULL, 64);
			StrBufPrintf(Pref->Val, "%ld", lvalue);
			Pref->decoded = 1;
			break;
		case PRF_QP_STRING:
			Pref->DeQPed = value;
			Pref->Val = NewStrBufPlain(NULL, StrLength(Pref->DeQPed) * 3);
			StrBufEUid_escapize(Pref->Val, Pref->DeQPed);
			Pref->decoded = 1;
			break;
		case PRF_YESNO:
			Pref->lval = lvalue;
			if (lvalue) 
				Pref->Val = NewStrBufPlain(HKEY("yes"));
			else
				Pref->Val = NewStrBufPlain(HKEY("no"));
			Pref->decoded = 1;
			break;
		}
		if (Pref->Type->OnLoad != NULL)
			Pref->Type->OnLoad(Pref->Val, Pref->lval);
	}
	else {
		Pref->eFlatPrefType = lPrefType;
		switch (lPrefType)
		{
		case PRF_STRING:
			Pref->Val = value;
			Pref->decoded = 1;
			break;
		case PRF_INT:
			Pref->lval = lvalue;
			Pref->Val = value;
			if (Pref->Val == NULL)
				Pref->Val = NewStrBufPlain(NULL, 64);
			StrBufPrintf(Pref->Val, "%ld", lvalue);
			Pref->decoded = 1;
			break;
		case PRF_QP_STRING:
			Pref->DeQPed = value;
			Pref->Val = NewStrBufPlain(NULL, StrLength(Pref->DeQPed) * 3);
			StrBufEUid_escapize(Pref->Val, Pref->DeQPed);
			Pref->decoded = 1;
			break;
		case PRF_YESNO:
			Pref->lval = lvalue;
			if (lvalue) 
				Pref->Val = NewStrBufPlain(HKEY("yes"));
			else
				Pref->Val = NewStrBufPlain(HKEY("no"));
			Pref->decoded = 1;
			break;
		}
	}

	if ((save_to_server != 0) && 
	    GetHash(WCC->hash_prefs, key, keylen, &vPrefB) && 
	    (vPrefB != NULL) && 
	    (compare_preference (Pref, vPrefB) == 0))
		save_to_server = 0;

	Put(WCC->hash_prefs, key, keylen, Pref, DestroyPreference);
	
	if (save_to_server) WCC->SavePrefsToServer = 1;
}

void set_PREFERENCE(const char *key, size_t keylen, StrBuf *value, int save_to_server) 
{
	set_preference_backend(key, keylen, 0, value, PRF_STRING, save_to_server, NULL);
}

int get_PREF_LONG(const char *key, size_t keylen, long *value, long Default)
{
	Preference *Pref;
	int Ret;

	Ret = get_pref_backend(key, keylen, &Pref);
	if (Ret == 0) {
		*value = Default;
		return 0;
	}

	if (Pref->decoded)
		*value = Pref->lval;
	else {
		*value = Pref->lval = atol(ChrPtr(Pref->Val));
		Pref->decoded = 1;
	}
	return Ret;
}


void set_PREF_LONG(const char *key, size_t keylen, long value, int save_to_server)
{
	set_preference_backend(key, keylen, value, NULL, PRF_INT, save_to_server, NULL);
}

int get_PREF_YESNO(const char *key, size_t keylen, int *value, int Default)
{
	Preference *Pref;
	int Ret;

	Ret = get_pref_backend(key, keylen, &Pref);
	if (Ret == 0) {
		*value = Default;
		return 0;
	}

	if (Pref->decoded)
		*value = Pref->lval;
	else {
		*value = Pref->lval = strcmp(ChrPtr(Pref->Val), "yes") == 0;
		Pref->decoded = 1;
	}
	return Ret;
}

void set_PREF_YESNO(const char *key, size_t keylen, long value, int save_to_server)
{
	set_preference_backend(key, keylen, value, NULL, PRF_YESNO, save_to_server, NULL);
}

int get_room_prefs_backend(const char *key, size_t keylen, 
			   Preference **Pref)
{
	StrBuf *pref_name;
	int Ret;

	pref_name = NewStrBufPlain (HKEY("ROOM:"));
	StrBufAppendBuf(pref_name, WC->CurRoom.name, 0);
	StrBufAppendBufPlain(pref_name, HKEY(":"), 0);
	StrBufAppendBufPlain(pref_name, key, keylen, 0);
	Ret = get_pref_backend(SKEY(pref_name), Pref);
	FreeStrBuf(&pref_name);

	return Ret;
}

const StrBuf *get_X_PREFS(const char *key, size_t keylen, 
			  const char *xkey, size_t xkeylen)
{
	int ret;
	StrBuf *pref_name;
	Preference *Prf;
	
	pref_name = NewStrBufPlain (HKEY("XPREF:"));
	StrBufAppendBufPlain(pref_name, xkey, xkeylen, 0);
	StrBufAppendBufPlain(pref_name, HKEY(":"), 0);
	StrBufAppendBufPlain(pref_name, key, keylen, 0);

	ret = get_pref_backend(SKEY(pref_name), &Prf);
	FreeStrBuf(&pref_name);

	if (ret)
		return Prf->Val;
	else return NULL;
}

void set_X_PREFS(const char *key, size_t keylen, const char *xkey, size_t xkeylen, StrBuf *value, int save_to_server)
{
	StrBuf *pref_name;
	
	pref_name = NewStrBufPlain (HKEY("XPREF:"));
	StrBufAppendBufPlain(pref_name, xkey, xkeylen, 0);
	StrBufAppendBufPlain(pref_name, HKEY(":"), 0);
	StrBufAppendBufPlain(pref_name, key, keylen, 0);

	set_preference_backend(SKEY(pref_name), 0, value, PRF_STRING, save_to_server, NULL);
	FreeStrBuf(&pref_name);
}


long get_ROOM_PREFS_LONG(const char *key, size_t keylen, long *value, long Default)
{
	Preference *Pref;
	int Ret;

	Ret = get_room_prefs_backend(key, keylen, &Pref);

	if (Ret == 0) {
		*value = Default;
		return 0;
	}

	if (Pref->decoded)
		*value = Pref->lval;
	else {
		*value = Pref->lval = atol(ChrPtr(Pref->Val));
		Pref->decoded = 1;
	}
	return Ret;
}


StrBuf *get_ROOM_PREFS(const char *key, size_t keylen)
{
	Preference *Pref;
	int Ret;

	Ret = get_room_prefs_backend(key, keylen, &Pref);

	if (Ret == 0) {
		return NULL;
	}
	else 
		return Pref->Val;
}

void set_ROOM_PREFS(const char *key, size_t keylen, StrBuf *value, int save_to_server)
{
	StrBuf *pref_name;
	
	pref_name = NewStrBufPlain (HKEY("ROOM:"));
	StrBufAppendBuf(pref_name, WC->CurRoom.name, 0);
	StrBufAppendBufPlain(pref_name, HKEY(":"), 0);
	StrBufAppendBufPlain(pref_name, key, keylen, 0);
	set_preference_backend(SKEY(pref_name), 0, value, PRF_STRING, save_to_server, NULL);
	FreeStrBuf(&pref_name);
}


void GetPreferences(HashList *Setting)
{
        wcsession *WCC = WC;
	HashPos *It;
	long len;
	const char *Key;
	void *vSetting;
	PrefDef *PrefType;
	StrBuf *Buf;
	long lval;
	HashList *Tmp;

	Tmp = WCC->hash_prefs;
	WCC->hash_prefs = Setting;

	It = GetNewHashPos(PreferenceHooks, 0);
	while (GetNextHashPos(PreferenceHooks, It, &len, &Key, &vSetting)) {
		PrefType = (PrefDef*) vSetting;

		if (!HaveBstr(SKEY(PrefType->Setting)))
			continue;
		switch (PrefType->eType) {
		case PRF_UNSET:
		case PRF_STRING:
			Buf = NewStrBufDup(SBstr(SKEY(PrefType->Setting)));
			set_preference_backend(SKEY(PrefType->Setting),
					       0, 
					       Buf, 
					       PRF_STRING,
					       1, 
					       PrefType);
			break;
		case PRF_INT:
			lval = LBstr(SKEY(PrefType->Setting));
			set_preference_backend(SKEY(PrefType->Setting),
					       lval, 
					       NULL, 
					       PRF_INT,
					       1, 
					       PrefType);
			break;
		case PRF_QP_STRING:
			Buf = NewStrBufDup(SBstr(SKEY(PrefType->Setting)));
			set_preference_backend(SKEY(PrefType->Setting),
					       0, 
					       Buf, 
					       PRF_QP_STRING,
					       1, 
					       PrefType);
			break;
		case PRF_YESNO:
			lval = YesBstr(SKEY(PrefType->Setting));
			set_preference_backend(SKEY(PrefType->Setting),
					       lval, 
					       NULL, 
					       PRF_YESNO,
					       1, 
					       PrefType);
			break;
		}
	}
	WCC->hash_prefs = Tmp;
	DeleteHashPos(&It);
}


/*
 * Commit new preferences and settings
 */
void set_preferences(void)
{	
	if (!havebstr("change_button")) {
		AppendImportantMessage(_("Cancelled.  No settings were changed."), -1);
		display_main_menu();
		return;
	}
	GetPreferences(WC->hash_prefs);
	display_main_menu();
}


void tmplput_CFG_Value(StrBuf *Target, WCTemplputParams *TP)
{
	Preference *Pref;
	if (get_pref_backend(TKEY(0), &Pref))
	{
		if (Pref->Type == NULL) {
			StrBufAppendTemplate(Target, TP, Pref->Val, 1);
		}
		switch (Pref->Type->eType)
		{
		case PRF_UNSET: /* default to string... */
		case PRF_STRING:
			StrBufAppendTemplate(Target, TP, Pref->Val, 1);
			break;
		case PRF_INT:
			if (Pref->decoded != 1) {
				if (Pref->Val == NULL)
					Pref->Val = NewStrBufPlain(NULL, 64);
				StrBufPrintf(Pref->Val, "%ld", Pref->lval);
				Pref->decoded = 1;
			}
			StrBufAppendTemplate(Target, TP, Pref->Val, 1);
			break;
		case PRF_QP_STRING:
			if (Pref->decoded != 1) {
				if (Pref->DeQPed == NULL)
					Pref->DeQPed = NewStrBufPlain(NULL, StrLength(Pref->Val));
					
				StrBufEUid_unescapize(Pref->DeQPed, Pref->Val);
				Pref->decoded = 1;
			}
			StrBufAppendTemplate(Target, TP, Pref->DeQPed, 1);
			break;
		case PRF_YESNO:
			if (Pref->decoded != 1) {
				Pref->lval = strcmp(ChrPtr(Pref->Val), "yes") == 0;
				Pref->decoded = 1;
			}
			StrBufAppendTemplate(Target, TP, Pref->Val, 1);
			break;
		}
	}
}

void tmplput_CFG_Descr(StrBuf *Target, WCTemplputParams *TP)
{
	const char *SettingStr;
	SettingStr = PrefGetLocalStr(TKEY(0));
	if (SettingStr != NULL) 
		StrBufAppendBufPlain(Target, SettingStr, -1, 0);
}
void tmplput_CFG_RoomValueLong(StrBuf *Target, WCTemplputParams *TP)
{
	long lvalue;
	long defval = 0;

	if (HAVE_PARAM(1))
		defval = GetTemplateTokenNumber(Target, TP, 1, 0);
	get_ROOM_PREFS_LONG(TKEY(0), &lvalue, defval);
	StrBufAppendPrintf(Target, "%ld", lvalue);
}
void tmplput_CFG_RoomValue(StrBuf *Target, WCTemplputParams *TP)
{
	StrBuf *pref = get_ROOM_PREFS(TKEY(0));
	if (pref != NULL) 
		StrBufAppendBuf(Target, pref, 0);
}
int ConditionalHasRoomPreference(StrBuf *Target, WCTemplputParams *TP) 
{
	if (get_ROOM_PREFS(TP->Tokens->Params[0]->Start, 
			   TP->Tokens->Params[0]->len) != NULL) 
		return 1;
  
	return 0;
}

int ConditionalPreference(StrBuf *Target, WCTemplputParams *TP)
{
	StrBuf *Pref;

	if (!get_PREFERENCE(TKEY(2), &Pref)) 
		return 0;
	
	if (!HAVE_PARAM(3)) {
		return 1;
	}
	else if (IS_NUMBER(TP->Tokens->Params[3]->Type))
	{
		return StrTol(Pref) == GetTemplateTokenNumber (Target, TP, 3, 0);
	}
	else 
	{
		const char *pch;
		long len;
		
		GetTemplateTokenString(Target, TP, 3, &pch, &len);
		
		return ((len == StrLength(Pref)) &&
			(strcmp(pch, ChrPtr(Pref)) == 0));
	}
}

int ConditionalHasPreference(StrBuf *Target, WCTemplputParams *TP)
{
	StrBuf *Pref;

	if (!get_PREFERENCE(TKEY(2), &Pref) || 
	    (Pref == NULL)) 
		return 0;
	else 
		return 1;
}


/********************************************************************************
 *                 preferences stored discrete in citserver
 ********************************************************************************/
HashList *GetGVEAHash(StrBuf *Target, WCTemplputParams *TP)
{
	StrBuf *Rcp;
	HashList *List = NULL;
	int Done = 0;
	int i, n = 1;
	char N[64];

	Rcp = NewStrBuf();
	serv_puts("GVEA");
	StrBuf_ServGetln(Rcp);
	if (GetServerStatus(Rcp, NULL) == 1) {
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

HashList *GetGVSNHash(StrBuf *Target, WCTemplputParams *TP)
{
	StrBuf *Rcp;
	HashList *List = NULL;
	int Done = 0;
	int i, n = 1;
	char N[64];

	Rcp = NewStrBuf();
	serv_puts("GVSN");
	StrBuf_ServGetln(Rcp);
	if (GetServerStatus(Rcp, NULL) == 1) {
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




/*
 * Offer to make any page the user's "start page" (only if logged in)
 */
void offer_start_page(StrBuf *Target, WCTemplputParams *TP)
{
	if (WC->logged_in) {
		wc_printf("<a href=\"change_start_page?startpage=");
		urlescputs(ChrPtr(WC->Hdr->this_page));
		wc_printf("\">");
		wc_printf(_("Make this my start page"));
		wc_printf("</a>");
	};
}


/*
 * Change the user's start page
 */
void change_start_page(void) 
{
	const char *pch;
	void *vHandler;
	int ProhibitSave = 0;
	const StrBuf *pStartPage = sbstr("startpage");

	if (pStartPage != NULL) {
		pch = strchr(ChrPtr(pStartPage), '?');

		if ((pch != NULL) && (
			    GetHash(HandlerHash, ChrPtr(pStartPage), pch - ChrPtr(pStartPage), &vHandler), 
			    (vHandler != NULL) &&
			    ((((WebcitHandler*)vHandler)->Flags & PROHIBIT_STARTPAGE) != 0)))
		{ /* OK, This handler doesn't want to be set as start page, prune it. */
			ProhibitSave = 1;
		}
	}

	if ((pStartPage == NULL) || 
	    (ProhibitSave == 1))
	{
		set_preference_backend(HKEY("startpage"), 
				       0, 
				       NewStrBufPlain(HKEY("")),
				       PRF_STRING,
				       1, 
				       NULL);
		if (ProhibitSave == 1)
			AppendImportantMessage(_("This isn't allowed to become the start page."), -1);
		else
			AppendImportantMessage(_("You no longer have a start page selected."), -1);
		display_main_menu();
		return;
	}



	set_preference_backend(HKEY("startpage"), 
			       0, 
			       NewStrBufDup(pStartPage),
			       PRF_STRING,
			       1, 
			       NULL);

	output_headers(1, 1, 0, 0, 0, 0);
	do_template("newstartpage");
	wDumpContent(1);
}


void LoadStartpage(StrBuf *URL, long lvalue)
{
	const char *pch;
	void *vHandler;
	pch = strchr(ChrPtr(URL), '?');
	if (pch == NULL) {
		/* purge the sins of the past... */
		pch = strchr(ChrPtr(URL), '&');
		if (pch != NULL) {
			StrBufPeek(URL, pch, -1, '?');
			WC->SavePrefsToServer = 1;
		}
	}
	else if (GetHash(HandlerHash, ChrPtr(URL), pch - ChrPtr(URL), &vHandler), 
		 (vHandler != NULL) &&
		 ((((WebcitHandler*)vHandler)->Flags & PROHIBIT_STARTPAGE) != 0))
	{ /* OK, This handler doesn't want to be set as start page, prune it. */
		FlushStrBuf(URL);
		WC->SavePrefsToServer = 1;
	}
}


void 
InitModule_PREFERENCES
(void)
{
	WebcitAddUrlHandler(HKEY("set_preferences"), "", 0, set_preferences, 0);
	WebcitAddUrlHandler(HKEY("change_start_page"), "", 0, change_start_page, 0);

	RegisterPreference("startpage", _("Prefered startpage"), PRF_STRING, LoadStartpage);

	RegisterNamespace("OFFERSTARTPAGE", 0, 0, offer_start_page, NULL, CTX_NONE);
	RegisterNamespace("PREF:ROOM:VALUE", 1, 2, tmplput_CFG_RoomValue,  NULL, CTX_NONE);
	RegisterNamespace("PREF:ROOM:VALUE:INT", 1, 2, tmplput_CFG_RoomValueLong,  NULL, CTX_NONE);
	RegisterNamespace("PREF:VALUE", 1, 2, tmplput_CFG_Value, NULL, CTX_NONE);
	
	RegisterNamespace("PREF:DESCR", 1, 1, tmplput_CFG_Descr, NULL, CTX_NONE);

	RegisterConditional("COND:PREF", 4, ConditionalPreference, CTX_NONE);
	RegisterConditional("COND:PREF:SET", 4, ConditionalHasPreference, CTX_NONE);
	RegisterConditional("COND:ROOM:SET", 4, ConditionalHasRoomPreference, CTX_NONE);
	
	RegisterIterator("PREF:VALID:EMAIL:ADDR", 0, NULL, 
			 GetGVEAHash, NULL, DeleteGVEAHash, CTX_STRBUF, CTX_NONE, IT_NOFLAG);
	RegisterIterator("PREF:VALID:EMAIL:NAME", 0, NULL, 
			 GetGVSNHash, NULL, DeleteGVSNHash, CTX_STRBUF, CTX_NONE, IT_NOFLAG);

}


void 
ServerStartModule_PREFERENCES
(void)
{
	PreferenceHooks = NewHash(1, NULL);
}



void 
ServerShutdownModule_PREFERENCES
(void)
{
	DeleteHash(&PreferenceHooks);
}

void
SessionDetachModule__PREFERENCES
(wcsession *sess)
{
	if (sess->SavePrefsToServer) {
		save_preferences();
		sess->SavePrefsToServer = 0;
	}
}

void
SessionNewModule_PREFERENCES
(wcsession *sess)
{
	sess->hash_prefs = NewHash(1,NULL);
}

void 
SessionDestroyModule_PREFERENCES
(wcsession *sess)
{
	DeleteHash(&sess->hash_prefs);
}
