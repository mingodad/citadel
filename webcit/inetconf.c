/* 
 * $Id$
 *
 * Functions which handle Internet domain configuration etc.
 */

#include "webcit.h"
#include "webserver.h"


typedef enum _e_cfg {
	ic_localhost,
	ic_directory,
	ic_smarthost,
	ic_fallback,
	ic_rbl,
	ic_spamass,
	ic_masq,
	ic_clamav,
	ic_notify,
	ic_max
} ECfg;

typedef struct _ConstStrBuf {
	const char *name;
	size_t len;
} ConstStrBuf;


  /* These are server config keywords; do not localize! */
ConstStrBuf CfgNames[] = {
	{ HKEY("localhost") },
	{ HKEY("directory") },
	{ HKEY("smarthost") },
	{ HKEY("fallbackhost") },
	{ HKEY("rbl") },
	{ HKEY("spamassassin") },
	{ HKEY("masqdomain") },
	{ HKEY("clamav") },
	{ HKEY("notify") }
};

	


/*
 * display the inet config dialog 
 */
void load_inetconf(void)
{
	wcsession *WCC = WC;
	StrBuf *Buf, *CfgToken, *Value;
	void *vHash;
	HashList *Hash;
	char nnn[64];
	int i, len, nUsed;
	
	WCC->InetCfg = NewHash(1, NULL);

	for (i = 0; i < (sizeof(CfgNames) / sizeof(ConstStrBuf)); i++) {
		Hash = NewHash(1, NULL);
		Put(WCC->InetCfg, CfgNames[i].name, CfgNames[i].len, Hash, HDeleteHash);
	}

	serv_printf("CONF GETSYS|application/x-citadel-internet-config");
	Buf = NewStrBuf();
	StrBuf_ServGetln(Buf);
		
	if (GetServerStatus(Buf, NULL) == 1) {
		CfgToken = NewStrBuf();
		while ((len = StrBuf_ServGetln(Buf),
			strcmp(ChrPtr(Buf), "000"))) {
			Value = NewStrBuf();
 
			StrBufExtract_token(CfgToken, Buf, 1, '|');
			StrBufExtract_token(Value, Buf, 0, '|');
			GetHash(WCC->InetCfg, ChrPtr(CfgToken), StrLength(CfgToken), &vHash);
			Hash = (HashList*) vHash;
			if (Hash == NULL) {
				lprintf(1, "ERROR Loading inet config line: [%s]\n", 
					ChrPtr(Buf));
				FreeStrBuf(&Value);
				continue;
			}
			nUsed = GetCount(Hash);
			nUsed = snprintf(nnn, sizeof(nnn), "%d", nUsed+1);
			Put(Hash, nnn, nUsed, Value, HFreeStrBuf); 
		}
		FreeStrBuf(&CfgToken);
	}
	FreeStrBuf(&Buf);
}


/*
 * save changes to the inet config
 */
void new_save_inetconf(void) {
	wcsession *WCC = WC;
	HashList *Hash;
	StrBuf *Str;
	StrBuf *Buf;
	const StrBuf *eType, *eNum, *eName;
	char nnn[64];
	void *vHash, *vStr;
	int i, nUsed;

	load_inetconf();
	eType = sbstr("etype");

	GetHash(WCC->InetCfg, ChrPtr(eType), StrLength(eType), &vHash);
	Hash = (HashList*) vHash;
	if (Hash == NULL) {
		StrBufPrintf(WCC->ImportantMsg, _("Invalid Parameter"));
		url_do_template();
		return;
	}

	if (strcasecmp(bstr("oper"), "delete") == 0) {
		eNum = sbstr("ename");
		if (!GetHash(Hash, ChrPtr(eNum), StrLength(eNum), &vStr) ||
		    (vStr == NULL)) {
			StrBufPrintf(WCC->ImportantMsg, _("Invalid Parameter"));
			url_do_template();
			return;
		}

		Str = (StrBuf*)vStr;
		StrBufPrintf(WCC->ImportantMsg, _("%s has been deleted."), ChrPtr(Str));
		FlushStrBuf(Str);	
	}
	else if (!strcasecmp(bstr("oper"), "add")) {
		StrBuf *name;
		eName = sbstr("ename");
		if (eName == NULL) {
			StrBufPrintf(WCC->ImportantMsg, _("Invalid Parameter"));
			url_do_template();
			return;
		}

		nUsed = GetCount(Hash);
		nUsed = snprintf(nnn, sizeof(nnn), "%d", nUsed+1);
		name = NewStrBufDup(eName);
		StrBufTrim(name);
		Put(Hash, nnn, nUsed, name, HFreeStrBuf); 
		StrBufPrintf(WCC->ImportantMsg, "%s %s", 
			     /*<domain> added status message*/ _("added."), 
			     ChrPtr(eName));
	}

	Buf = NewStrBuf();
	serv_printf("CONF PUTSYS|application/x-citadel-internet-config");
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL) == 4) {
		for (i = 0; i < (sizeof(CfgNames) / sizeof(ConstStrBuf)); i++) {
			HashPos *where;
			const char *Key;
			long KeyLen;

			GetHash(WCC->InetCfg, CfgNames[i].name, CfgNames[i].len, &vHash);
			Hash = (HashList*) vHash;
			if (Hash == NULL) {
				sprintf(WC->ImportantMessage, _("Invalid Parameter"));
				url_do_template();
				return;
			}
			if (GetCount(Hash) > 0) {
				where = GetNewHashPos(Hash, 0);
				while (GetNextHashPos(Hash, where, &KeyLen, &Key, &vStr)) {
					Str = (StrBuf*) vStr;
					if ((Str!= NULL) && (StrLength(Str) > 0))
						serv_printf("%s|%s", 
							    ChrPtr(Str),
							    CfgNames[i].name); 
				}
				DeleteHashPos(&where);
			}			
		}
		serv_puts("000");
		DeleteHash(&WCC->InetCfg);
	}
	FreeStrBuf(&Buf);
	url_do_template();
}

void InetCfgSubst(StrBuf *TemplBuffer, WCTemplputParams *TP)
{
	SVPutBuf("SERVCFG:INET:HOSTNAME", CTX, 1);
}

void DeleteInetConfHash(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;

	if (WCC->InetCfg != NULL)
		DeleteHash(&WCC->InetCfg);

}


HashList *GetInetConfHash(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;
	void *vHash;

	if (WCC->InetCfg == NULL)
		load_inetconf();
	GetHash(WCC->InetCfg, TKEY(5), &vHash);
	svprintf(HKEY("SERVCFG:INET:TYPE"), WCS_STRING, TP->Tokens->Params[5]->Start);
	return vHash;
}

void 
InitModule_INETCONF
(void)
{
	WebcitAddUrlHandler(HKEY("save_inetconf"), "", 0, new_save_inetconf, 0);
	RegisterIterator("SERVCFG:INET", 1, NULL, GetInetConfHash, InetCfgSubst, NULL, CTX_INETCFG, CTX_NONE, IT_NOFLAG);
	RegisterNamespace("SERVCFG:FLUSHINETCFG",0, 0, DeleteInetConfHash, NULL, CTX_NONE);
}