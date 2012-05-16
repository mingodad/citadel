/*
 * Administrative screen for site-wide configuration
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

HashList *ZoneHash = NULL;

ConstStr ExpirePolicyString = {CStrOf(roompolicy)     };

ConstStr ExpirePolicyStrings[][2] = {
	{ { CStrOf(roompolicy)     } , { strof(roompolicy)     "_value", sizeof(strof(roompolicy)     "_value") - 1 } },
	{ { CStrOf(floorpolicy)    } , { strof(floorpolicy)    "_value", sizeof(strof(floorpolicy)    "_value") - 1 } },
	{ { CStrOf(sitepolicy)     } , { strof(sitepolicy)     "_value", sizeof(strof(sitepolicy)     "_value") - 1 } },
	{ { CStrOf(mailboxespolicy)} , { strof(mailboxespolicy)"_value", sizeof(strof(mailboxespolicy)"_value") - 1 } }
};

void LoadExpirePolicy(GPEXWhichPolicy which)
{
	StrBuf *Buf;
	wcsession *WCC = WC;
	long State;
	const char *Pos = NULL;

	serv_printf("GPEX %s", ExpirePolicyStrings[which][0].Key);
	Buf = NewStrBuf();
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, &State) == 2) {
		Pos = ChrPtr(Buf) + 4;
		WCC->Policy[which].expire_mode = StrBufExtractNext_long(Buf, &Pos, '|');
		WCC->Policy[which].expire_value = StrBufExtractNext_long(Buf, &Pos, '|');
	}
	else if (State == 550)
		AppendImportantMessage(_("Higher access is required to access this function."), -1);
	FreeStrBuf(&Buf);
}

void SaveExpirePolicyFromHTTP(GPEXWhichPolicy which)
{
	StrBuf *Buf;
	long State;

	serv_printf("SPEX %s|%d|%d", 
		            ExpirePolicyStrings[which][0].Key,
		    ibcstr( ExpirePolicyStrings[which][1] ),
		    ibcstr( ExpirePolicyStrings[which][1] )  );

	Buf = NewStrBuf();
	StrBuf_ServGetln(Buf);
	GetServerStatus(Buf, &State);
	if (State == 550)
		AppendImportantMessage(_("Higher access is required to access this function."), -1);
	FreeStrBuf(&Buf);
}

int ConditionalExpire(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;
	GPEXWhichPolicy which;
	int CompareWith;

	which = GetTemplateTokenNumber(Target, TP, 2, 0);
	CompareWith = GetTemplateTokenNumber(Target, TP, 3, 0);

	LoadExpirePolicy(which);
	
	return WCC->Policy[which].expire_mode == CompareWith;
}

void tmplput_ExpireValue(StrBuf *Target, WCTemplputParams *TP)
{
	GPEXWhichPolicy which;
	wcsession *WCC = WC;
		
	which = GetTemplateTokenNumber(Target, TP, 0, 0);
	LoadExpirePolicy(which);
	StrBufAppendPrintf(Target, "%d", WCC->Policy[which].expire_value);
}


void tmplput_ExpireMode(StrBuf *Target, WCTemplputParams *TP)
{
	GPEXWhichPolicy which;
	wcsession *WCC = WC;
		
	which = GetTemplateTokenNumber(Target, TP, 2, 0);
	LoadExpirePolicy(which);
	StrBufAppendPrintf(Target, "%d", WCC->Policy[which].expire_mode);
}


void LoadZoneFiles(void)
{
	icalarray *zones;
	int z;
	long len;
	const char *this_zone;
	StrBuf *ZName;
	
	ZoneHash = NewHash(1, NULL);
	ZName = NewStrBufPlain(HKEY("UTC"));
	Put(ZoneHash, HKEY("UTC"), ZName, HFreeStrBuf);
	zones = icaltimezone_get_builtin_timezones();
	for (z = 0; z < zones->num_elements; ++z) {
		/* syslog(9, "Location: %-40s tzid: %s\n",
			icaltimezone_get_location(icalarray_element_at(zones, z)),
			icaltimezone_get_tzid(icalarray_element_at(zones, z))
		); */
		this_zone = icaltimezone_get_location(icalarray_element_at(zones, z));
		len = strlen(this_zone);
		ZName = NewStrBufPlain(this_zone, len);
		Put(ZoneHash, this_zone, len, ZName, HFreeStrBuf);
	}
	SortByHashKey(ZoneHash, 0);
}


typedef struct _CfgMapping {
	int type;
	const char *Key;
	long len;
} CfgMapping;

#define CFG_STR 1
#define CFG_YES 2
#define CFG_NO 3

CfgMapping ServerConfig[] = {
	{CFG_STR, HKEY("c_nodename")},
	{CFG_STR, HKEY("c_fqdn")},
	{CFG_STR, HKEY("c_humannode")},
	{CFG_STR, HKEY("c_phonenum")},
	{CFG_YES, HKEY("c_creataide")},
	{CFG_STR, HKEY("c_sleeping")},
	{CFG_STR, HKEY("c_initax")},
	{CFG_YES, HKEY("c_regiscall")},
	{CFG_YES, HKEY("c_twitdetect")},
	{CFG_STR, HKEY("c_twitroom")},
	{CFG_STR, HKEY("c_moreprompt")},
	{CFG_YES, HKEY("c_restrict")},
	{CFG_STR, HKEY("c_bbs_city")},
	{CFG_STR, HKEY("c_sysadm")},
	{CFG_STR, HKEY("c_maxsessions")},
	{CFG_STR, HKEY("reserved1")},
	{CFG_STR, HKEY("c_userpurge")},
	{CFG_STR, HKEY("c_roompurge")},
	{CFG_STR, HKEY("c_logpages")},
	{CFG_STR, HKEY("c_createax")},
	{CFG_STR, HKEY("c_maxmsglen")},
	{CFG_STR, HKEY("c_min_workers")},
	{CFG_STR, HKEY("c_max_workers")},
	{CFG_STR, HKEY("c_pop3_port")},
	{CFG_STR, HKEY("c_smtp_port")},
	{CFG_NO , HKEY("c_rfc822_strict_from")},	/* note: reverse bool */
	{CFG_YES, HKEY("c_aide_zap")},
	{CFG_STR, HKEY("c_imap_port")},
	{CFG_STR, HKEY("c_net_freq")},
	{CFG_YES, HKEY("c_disable_newu")},
	{CFG_STR, HKEY("reserved2")},
	{CFG_STR, HKEY("c_purge_hour")},
	{CFG_STR, HKEY("c_ldap_host")},
	{CFG_STR, HKEY("c_ldap_port")},
	{CFG_STR, HKEY("c_ldap_base_dn")},
	{CFG_STR, HKEY("c_ldap_bind_dn")},
	{CFG_STR, HKEY("c_ldap_bind_pw")},
	{CFG_STR, HKEY("c_ip_addr")},
	{CFG_STR, HKEY("c_msa_port")},
	{CFG_STR, HKEY("c_imaps_port")},
	{CFG_STR, HKEY("c_pop3s_port")},
	{CFG_STR, HKEY("c_smtps_port")},
	{CFG_YES, HKEY("c_enable_fulltext")},
	{CFG_YES, HKEY("c_auto_cull")},
	{CFG_YES, HKEY("c_instant_expunge")},
	{CFG_YES, HKEY("c_allow_spoofing")},
	{CFG_YES, HKEY("c_journal_email")},
	{CFG_YES, HKEY("c_journal_pubmsgs")},
	{CFG_STR, HKEY("c_journal_dest")},
	{CFG_STR, HKEY("c_default_cal_zone")},
	{CFG_STR, HKEY("c_pftcpdict_port")},
	{CFG_STR, HKEY("c_mgesve_port")},
	{CFG_STR, HKEY("c_auth_mode")},
	{CFG_STR, HKEY("c_funambol_host")},
	{CFG_STR, HKEY("c_funambol_port")},
	{CFG_STR, HKEY("c_funambol_source")},
	{CFG_STR, HKEY("c_funambol_auth")},
	{CFG_YES, HKEY("c_rbl_at_greeting")},
	{CFG_STR, HKEY("c_master_user")},
	{CFG_STR, HKEY("c_master_pass")},
	{CFG_STR, HKEY("c_pager_program")},
	{CFG_YES, HKEY("c_imap_keep_from")},
	{CFG_STR, HKEY("c_xmpp_c2s_port")},
	{CFG_STR, HKEY("c_xmpp_s2s_port")},
	{CFG_STR, HKEY("c_pop3_fetch")},
	{CFG_STR, HKEY("c_pop3_fastest")},
	{CFG_YES, HKEY("c_spam_flag_only")},
	{CFG_YES, HKEY("c_guest_logins")}
};



/*
 *  display all configuration items
 */
void load_siteconfig(void)
{
	wcsession *WCC = WC;
	StrBuf *Buf;
	HashList *Cfg;
	long len;
	int i, j;
	
	if (WCC->ServCfg == NULL)
		WCC->ServCfg = NewHash(1, NULL);
	Cfg = WCC->ServCfg;

	Buf = NewStrBuf();

	serv_printf("CONF get");
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL) != 1) {
		StrBufCutLeft(Buf, 4);
		AppendImportantMessage(SKEY(Buf));
		FreeStrBuf(&Buf);
		return;
		
	}
	j = i = 0;
	while (len = StrBuf_ServGetln(Buf),
	       (len >= 0) && 
	       ((len != 3) || strcmp(ChrPtr(Buf), "000")))
	{
		if (i < (sizeof(ServerConfig) / sizeof(CfgMapping)))
		{
			Put(Cfg,
			    ServerConfig[i].Key, 
			    ServerConfig[i].len, 
			    Buf, 
			    HFreeStrBuf);
			i++;
			Buf = NewStrBuf();
		}
		else {
			if (j == 0)
				AppendImportantMessage(_("WARNING: Failed to parse Server Config; do you run a to new citserver?"), -1);
			j++;
		}
	}
	FreeStrBuf(&Buf);

	LoadExpirePolicy(roompolicy);
	LoadExpirePolicy(floorpolicy);
	LoadExpirePolicy(mailboxespolicy);
	LoadExpirePolicy(sitepolicy);
}



/*
 * parse siteconfig changes 
 */
void siteconfig(void)
{
	wcsession *WCC = WC;
	int i;
	StrBuf *Line;

	if (strlen(bstr("ok_button")) == 0) {
		display_aide_menu();
		return;
	}
	Line = NewStrBuf();
	serv_printf("CONF set");
	StrBuf_ServGetln(Line);
	if (GetServerStatusMsg(Line, NULL, 1, 4) != 4) {
		display_aide_menu();
		FreeStrBuf(&Line);
		return;
	}

	FreeStrBuf(&Line);

	for (i=0; i < (sizeof(ServerConfig) / sizeof(CfgMapping)); i ++)
	{
		switch (ServerConfig[i].type) {
		default:
		case CFG_STR:
			serv_putbuf(SBstr(ServerConfig[i].Key, ServerConfig[i].len));
			break;
		case CFG_YES:
			serv_puts(YesBstr(ServerConfig[i].Key, 
					  ServerConfig[i].len) ?
				  "1" : "0");
			break;
		case CFG_NO:
			serv_puts(YesBstr(ServerConfig[i].Key, 
					  ServerConfig[i].len) ?
				  "0" : "1");
			break;
		}
	}
        serv_puts("000");

	SaveExpirePolicyFromHTTP(sitepolicy);
	SaveExpirePolicyFromHTTP(mailboxespolicy);

	FreeStrBuf(&WCC->serv_info->serv_default_cal_zone);
	WCC->serv_info->serv_default_cal_zone = NewStrBufDup(sbstr("c_default_cal_zone"));

	AppendImportantMessage(_("Your system configuration has been updated."), -1);
	DeleteHash(&WCC->ServCfg);
	display_aide_menu();
}

void tmplput_servcfg(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;
	void *vBuf;
	StrBuf *Buf;

	if (WCC->is_aide) {
		if (WCC->ServCfg == NULL)
			load_siteconfig();
		GetHash(WCC->ServCfg, TKEY(0), &vBuf);
		Buf = (StrBuf*) vBuf;
		StrBufAppendTemplate(Target, TP, Buf, 1);
	}
}

int ConditionalServCfg(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;
	void *vBuf;
	StrBuf *Buf;

	if (WCC->is_aide) {
		if (WCC->ServCfg == NULL)
			load_siteconfig();
		GetHash(WCC->ServCfg, TKEY(2), &vBuf);
		if (vBuf == NULL) return 0;
		Buf = (StrBuf*) vBuf;
		if (TP->Tokens->nParameters == 3) {
			return 1;
		}
		else if (IS_NUMBER(TP->Tokens->Params[3]->Type))
			return (StrTol(Buf) == GetTemplateTokenNumber (Target, TP, 3, 0));
		else
		{
			const char *pch;
			long len;
			
			GetTemplateTokenString(Target, TP, 3, &pch, &len);
		
			return ((len == StrLength(Buf)) &&
				(strcmp(pch, ChrPtr(Buf)) == 0));
		}

	}
	else return 0;
}

int ConditionalServCfgCTXStrBuf(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;
	void *vBuf;
	StrBuf *Buf;
	StrBuf *ZoneToCheck = (StrBuf*) CTX;

	if ((WCC->is_aide) || (ZoneToCheck == NULL)) {
		if (WCC->ServCfg == NULL)
			load_siteconfig();
		GetHash(WCC->ServCfg, TKEY(2), &vBuf);
		if (vBuf == NULL) return 0;
		Buf = (StrBuf*) vBuf;

		return strcmp(ChrPtr(Buf), ChrPtr(ZoneToCheck)) == 0;
	}
	else return 0;
}

/*----------------------------------------------------------------------------*
 *              Displaying Logging                                            *
 *----------------------------------------------------------------------------*/
typedef struct __LogStatusStruct {
	int Enable;
	StrBuf *Name;
}LogStatusStruct;

void DeleteLogStatusStruct(void *v)
{
	LogStatusStruct *Stat = (LogStatusStruct*)v;

	FreeStrBuf(&Stat->Name);
	free(Stat);
}

void tmplput_servcfg_LogName(StrBuf *Target, WCTemplputParams *TP)
{
        LogStatusStruct *Stat = (LogStatusStruct*) CTX;
	StrBufAppendTemplate(Target, TP, Stat->Name, 1);
}

int ConditionalServCfgThisLogEnabled(StrBuf *Target, WCTemplputParams *TP)
{
        LogStatusStruct *Stat = (LogStatusStruct*) CTX;
	return Stat->Enable;
}

HashList *iterate_GetSrvLogEnable(StrBuf *Target, WCTemplputParams *TP)
{
        HashList *Hash = NULL;
	StrBuf *Buf;
	LogStatusStruct *Stat;
	const char *Pos;
	int Done = 0;
	long len;
	int num_logs = 0;

        serv_puts("LOGP");
        Buf = NewStrBuf();
        StrBuf_ServGetln(Buf);
        if (GetServerStatus(Buf, NULL) == 1) {
                Hash = NewHash(1, Flathash);
                while (!Done) {
                        len = StrBuf_ServGetln(Buf);
                        if ((len <0) || 
                            ((len == 3) &&
                             !strcmp(ChrPtr(Buf), "000")))
                        {
                                Done = 1;
                                break;
                        }
			Stat = (LogStatusStruct*) malloc (sizeof(LogStatusStruct));
			Pos = NULL;
			Stat->Name = NewStrBufPlain(NULL, len);
                        StrBufExtract_NextToken(Stat->Name, Buf, &Pos, '|');
                        Stat->Enable = StrBufExtractNext_int(Buf, &Pos, '|');

                        Put(Hash, IKEY(num_logs), Stat, DeleteLogStatusStruct); 
			num_logs++;
                }
	}
	FreeStrBuf(&Buf);
	return Hash;
}


void 
InitModule_SITECONFIG
(void)
{
	WebcitAddUrlHandler(HKEY("siteconfig"), "", 0, siteconfig, CTX_NONE);

	RegisterNamespace("SERV:CFG", 1, 2, tmplput_servcfg, NULL, CTX_NONE);
	RegisterConditional(HKEY("COND:SERVCFG"), 3, ConditionalServCfg, CTX_NONE);
	RegisterConditional(HKEY("COND:SERVCFG:CTXSTRBUF"), 4, ConditionalServCfgCTXStrBuf, CTX_STRBUF);
	RegisterIterator("PREF:ZONE", 0, ZoneHash, NULL, NULL, NULL, CTX_STRBUF, CTX_NONE, IT_NOFLAG);

	REGISTERTokenParamDefine(roompolicy);
	REGISTERTokenParamDefine(floorpolicy);
	REGISTERTokenParamDefine(sitepolicy);
	REGISTERTokenParamDefine(mailboxespolicy);

	REGISTERTokenParamDefine(EXPIRE_NEXTLEVEL);
	REGISTERTokenParamDefine(EXPIRE_MANUAL);
	REGISTERTokenParamDefine(EXPIRE_NUMMSGS);
	REGISTERTokenParamDefine(EXPIRE_AGE);

	RegisterConditional(HKEY("COND:EXPIRE:MODE"), 2, ConditionalExpire, CTX_NONE);
	RegisterNamespace("EXPIRE:VALUE", 1, 2, tmplput_ExpireValue, NULL, CTX_NONE);
	RegisterNamespace("EXPIRE:MODE", 1, 2, tmplput_ExpireMode, NULL, CTX_NONE);

	RegisterConditional(HKEY("COND:SERVCFG:THISLOGENABLE"), 4, ConditionalServCfgThisLogEnabled, CTX_SRVLOG);
	RegisterIterator("SERVCFG:LOGENABLE", 0, NULL, iterate_GetSrvLogEnable, NULL, DeleteHash, CTX_SRVLOG, CTX_NONE, IT_NOFLAG);
	RegisterNamespace("SERVCFG:LOGNAME", 0, 1, tmplput_servcfg_LogName, NULL, CTX_SRVLOG);
}

void 
ServerStartModule_SITECONFIG
(void)
{
	LoadZoneFiles();
}

void 
ServerShutdownModule_SITECONFIG
(void)
{
	DeleteHash(&ZoneHash);
}


void 
SessionDestroyModule_SITECONFIG
(wcsession *sess)
{
	DeleteHash(&sess->ServCfg);
}
