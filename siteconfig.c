/*
 * $Id$
 *
 * Administrative screen for site-wide configuration
 */


#include "webcit.h"
#include "webserver.h"


HashList *ZoneHash = NULL;



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
		/* lprintf(9, "Location: %-40s tzid: %s\n",
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
}CfgMapping;

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
	{CFG_YES , HKEY("c_spam_flag_only")}
};


/*
 *  display all configuration items
 */
void load_siteconfig(void)
{
	wcsession *WCC = WC;
	StrBuf *Buf, *CfgToken;
	HashList *Cfg;
	char buf[SIZ];
	long len;
	int i;
	
	if (WCC->ServCfg == NULL)
		WCC->ServCfg = NewHash(1, NULL);
	Cfg = WCC->ServCfg;

	serv_printf("CONF get");
	serv_getln(buf, sizeof buf);
	i = 0;
	Buf = NewStrBuf();
	while ((sizeof(ServerConfig) / sizeof(CfgMapping)) &&
	       (len = StrBuf_ServGetln(Buf),
		strcmp(ChrPtr(Buf), "000")) && 
	       (i <= sizeof(ServerConfig))) 
	{
		Put(Cfg,
		    ServerConfig[i].Key, 
		    ServerConfig[i].len, 
		    Buf, 
		    HFreeStrBuf);
		i++;
		if (i <= sizeof(ServerConfig) / sizeof(CfgMapping))
			Buf = NewStrBuf();
		else
			Buf = NULL;			
	}
	FreeStrBuf(&Buf);

	serv_puts("GPEX site");
	Buf = NewStrBuf();
	CfgToken = NULL;
	StrBuf_ServGetln(Buf);
	if (ChrPtr(Buf)[0] == '2') {
		StrBufCutLeft(Buf, 4);

		CfgToken = NewStrBuf();
		StrBufExtract_token(CfgToken, Buf, 0, '|');
		Put(Cfg, HKEY("sitepolicy"), CfgToken, HFreeStrBuf);

		CfgToken = NewStrBuf();
		StrBufExtract_token(CfgToken, Buf, 1, '|');
		Put(Cfg, HKEY("sitevalue"), CfgToken, HFreeStrBuf);
	}

	serv_puts("GPEX mailboxes");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '2') {
		StrBufCutLeft(Buf, 4);

		CfgToken = NewStrBuf();
		StrBufExtract_token(CfgToken, Buf, 0, '|');
		Put(Cfg, HKEY("mboxpolicy"), CfgToken, HFreeStrBuf);

		CfgToken = NewStrBuf();
		StrBufExtract_token(CfgToken, Buf, 1, '|');
		Put(Cfg, HKEY("mboxvalue"), CfgToken, HFreeStrBuf);
	}
}


/**
 * parse siteconfig changes 
 */
void siteconfig(void)
{
	wcsession *WCC = WC;
	int i;
	char buf[256];

	if (strlen(bstr("ok_button")) == 0) {
		display_aide_menu();
		return;
	}
	serv_printf("CONF set");
	serv_getln(buf, sizeof buf);
	if (buf[0] != '4') {
		safestrncpy(WCC->ImportantMessage, &buf[4], sizeof WCC->ImportantMessage);
		display_aide_menu();
		return;
	}

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

	serv_printf("SPEX site|%d|%d", ibstr("sitepolicy"), ibstr("sitevalue"));
	serv_getln(buf, sizeof buf);
	serv_printf("SPEX mailboxes|%d|%d", ibstr("mboxpolicy"), ibstr("mboxvalue"));
	serv_getln(buf, sizeof buf);

	strcpy(serv_info.serv_default_cal_zone, bstr("c_default_cal_zone"));

	safestrncpy(WCC->ImportantMessage, _("Your system configuration has been updated."),
		sizeof WCC->ImportantMessage);
	DeleteHash(&WCC->ServCfg);
	display_aide_menu();
}

void tmplput_servcfg(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *Context, int ContextType)
{
	wcsession *WCC = WC;
	void *vBuf;
	StrBuf *Buf;

	if (WCC->is_aide) {
		if (WCC->ServCfg == NULL)
			load_siteconfig();
		GetHash(WCC->ServCfg, TKEY(0), &vBuf);
		Buf = (StrBuf*) vBuf;
		StrBufAppendTemplate(Target, nArgs, Tokens, Context, ContextType, Buf, 1);
	}
}

int ConditionalServCfg(WCTemplateToken *Tokens, void *Context, int ContextType)
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
		if (Tokens->nParameters == 3) {
			return 1;
		}
		else if (Tokens->Params[3]->Type == TYPE_STR)
			return (strcmp(Tokens->Params[3]->Start, ChrPtr(Buf)) == 0);
		else return (StrTol(Buf) == Tokens->Params[3]->lvalue);
	}
	else return 0;
}

int ConditionalServCfgSubst(WCTemplateToken *Tokens, void *Context, int ContextType)
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

		return CompareSubstToStrBuf(Buf, Tokens->Params[3]);
	}
	else return 0;
}

void 
InitModule_SITECONFIG
(void)
{
	WebcitAddUrlHandler(HKEY("siteconfig"), siteconfig, CTX_NONE);

	RegisterNamespace("SERV:CFG", 1, 2, tmplput_servcfg, CTX_NONE);
	RegisterConditional(HKEY("COND:SERVCFG"), 3, ConditionalServCfg, CTX_NONE);
	RegisterConditional(HKEY("COND:SERVCFG:SUBST"), 4, ConditionalServCfgSubst, CTX_NONE);
}
