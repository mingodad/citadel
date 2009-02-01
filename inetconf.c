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
	ic_rbl,
	ic_spamass,
	ic_masq,
	ic_clamav,
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
	{ HKEY("rbl") },
	{ HKEY("spamassassin") },
	{ HKEY("masqdomain") },
	{ HKEY("clamav") }
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
	char buf[SIZ];
	int i, len, nUsed;
	
	WCC->InetCfg = NewHash(1, NULL);

	for (i = 0; i < (sizeof(CfgNames) / sizeof(ConstStrBuf)); i++) {
		Hash = NewHash(1, NULL);
		Put(WCC->InetCfg, CfgNames[i].name, CfgNames[i].len, Hash, HDeleteHash);
	}

	serv_printf("CONF GETSYS|application/x-citadel-internet-config");
	serv_getln(buf, sizeof buf);
	
	if (buf[0] == '1') {
		Buf = NewStrBuf();
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
		FreeStrBuf(&Buf);
		FreeStrBuf(&CfgToken);
	}
}


/*
 * save changes to the inet config
 */
void new_save_inetconf(void) {
	wcsession *WCC = WC;
	HashList *Hash;
	StrBuf *Str;
	const StrBuf *eType, *eNum, *eName;
	char nnn[64];
	void *vHash, *vStr;
	char buf[SIZ];
	int i, nUsed;

	load_inetconf();
	eType = sbstr("etype");

	GetHash(WCC->InetCfg, ChrPtr(eType), StrLength(eType), &vHash);
	Hash = (HashList*) vHash;
	if (Hash == NULL) {
		sprintf(WC->ImportantMessage, _("Invalid Parameter"));
		url_do_template();
		return;
	}

	if (strcasecmp(bstr("oper"), "delete") == 0) {
		eNum = sbstr("ename");
		if (!GetHash(Hash, ChrPtr(eNum), StrLength(eNum), &vStr) ||
		    (vStr == NULL)) {
			sprintf(WC->ImportantMessage, _("Invalid Parameter"));
			url_do_template();
			return;
		}

		Str = (StrBuf*)vStr;
		sprintf(WC->ImportantMessage, _("%s has been deleted."), ChrPtr(Str));
		FlushStrBuf(Str);	
	}
	else if (!strcasecmp(bstr("oper"), "add")) {
		eName = sbstr("ename");
		if (eName == NULL) {
			sprintf(WC->ImportantMessage, _("Invalid Parameter"));
			url_do_template();
			return;
		}

		nUsed = GetCount(Hash);
		nUsed = snprintf(nnn, sizeof(nnn), "%d", nUsed+1);
	
		Put(Hash, nnn, nUsed, NewStrBufDup(eName), HFreeStrBuf); 
		sprintf(WC->ImportantMessage, "%s added.", ChrPtr(eName));
	}

	serv_printf("CONF PUTSYS|application/x-citadel-internet-config");
	serv_getln(buf, SIZ);
	if (buf[0] == '4') {
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
	WebcitAddUrlHandler(HKEY("save_inetconf"), new_save_inetconf, AJAX);
	RegisterIterator("SERVCFG:INET", 1, NULL, GetInetConfHash, InetCfgSubst, NULL, CTX_INETCFG, CTX_NONE, IT_NOFLAG);
	RegisterNamespace("SERVCFG:FLUSHINETCFG",0, 0, DeleteInetConfHash, CTX_NONE);
}
