/* 
 * Global system configuration.
 * 
 * Developers: please do NOT remove the fields labelled "not in use".  We
 * can't simply remove them from the struct, because this gets written to
 * disk, and if you change it then you'll break all existing systems.
 * However, if you'd like to reclaim some of that space for another use, feel
 * free to do so, as long as the sizes are kept identical.
 */

CFG_VALUE(STRING_BUF(c_nodename, 16),		" Unqualified \"short\" nodename     ");
CFG_VALUE(STRING_BUF(c_fqdn, 64),		" Fully Qualified Domain Name      ");
CFG_VALUE(STRING_BUF(c_humannode, 21),		" long name of system              ");
CFG_VALUE(STRING_BUF(c_phonenum, 16),		" Dialup number of system          ");
CFG_VALUE(UID_T(c_ctdluid),			" UID under which we run Citadel   ");
CFG_VALUE(CHAR(c_creataide),			" room creator = room aide  flag   ");
CFG_VALUE(INTEGER(c_sleeping),			" watchdog timer setting           ");
CFG_VALUE(CHAR(c_initax),			" initial access level             ");
CFG_VALUE(CHAR(c_regiscall),			" call number to register on       ");
CFG_VALUE(CHAR(c_twitdetect),			" twit detect flag                 ");
CFG_VALUE(STRING_BUF(c_twitroom, ROOMNAMELEN),	" twit detect msg move to room     ");
CFG_VALUE(STRING_BUF(c_moreprompt, 80),		" paginator prompt                 ");
CFG_VALUE(CHAR(c_restrict),			" restrict internet mail flag      ");
NO_ARTV(CFG_VALUE(LONG(c_niu_1),		" (not in use)                     ")); ///////
CFG_VALUE(STRING_BUF(c_site_location, 32),	" physical location of server      ");
CFG_VALUE(STRING_BUF(c_sysadm, 26),		" name of system administrator     ");
NO_ARTV(CFG_VALUE(STRING_BUF(c_niu_2, 15),	" (not in use)                     "));
CFG_VALUE(INTEGER(c_setup_level),		" what rev level we've setup to    ");
CFG_VALUE(INTEGER(c_maxsessions),		" maximum concurrent sessions      ");
/* c_ip_addr is out of sortorder; its located after c_ldap_bind_pw in the old export */
CFG_VALUE(STRING_BUF(c_ip_addr, 20),		" IP address to listen on          "); 
CFG_VALUE(INTEGER(c_port_number),		" Cit listener port (usually 504)  ");
NO_ARTV(CFG_VALUE(INTEGER(c_ipgm_secret),	" internal program authentication  "));
CFG_VALUE(SUBSTRUCT(struct ExpirePolicy c_ep),	" System default msg expire policy ");
SUBSTRUCT_ELEMENT(INTEGER(c_ep.expire_mode);)
SUBSTRUCT_ELEMENT(INTEGER(c_ep.expire_value);)
CFG_VALUE(INTEGER(c_userpurge),			" System default user purge (days) ");
CFG_VALUE(INTEGER(c_roompurge),			" System default room purge (days) ");
CFG_VALUE(STRING_BUF(c_logpages, ROOMNAMELEN),	" Room to log pages to (or not)    ");
CFG_VALUE(CHAR(c_createax),			" Axlevel required to create rooms ");
CFG_VALUE(LONG(c_maxmsglen),			" Maximum message length           ");
CFG_VALUE(INTEGER(c_min_workers),		" Lower limit on number of threads ");
CFG_VALUE(INTEGER(c_max_workers),		" Upper limit on number of threads ");
CFG_VALUE(INTEGER(c_pop3_port),			" POP3 listener port (usually 110) ");
CFG_VALUE(INTEGER(c_smtp_port),			" SMTP listener port (usually 25)  ");
////purge_hour? mbxexp? ldap_host? ldap_port?
CFG_VALUE(INTEGER(c_rfc822_strict_from),	" 1 = don't correct From: forgeries");
CFG_VALUE(INTEGER(c_aide_zap),			" Are Aides allowed to zap rooms?  ");
CFG_VALUE(INTEGER(c_imap_port),			" IMAP listener port (usually 143) ");
CFG_VALUE(TIME(c_net_freq),			" how often to run the networker   ");
CFG_VALUE(CHAR(c_disable_newu),			" disable NEWU command             ");
CFG_VALUE(CHAR(c_enable_fulltext),		" enable full text indexing        ");
CFG_VALUE(STRING_BUF(c_baseroom, ROOMNAMELEN),	" Name of baseroom (Lobby)	    ");
CFG_VALUE(STRING_BUF(c_aideroom, ROOMNAMELEN),	" Name of aideroom (Aide)	    ");
CFG_VALUE(INTEGER(c_purge_hour),		" Hour during which db purges run  ");
CFG_VALUE(SUBSTRUCT(struct ExpirePolicy c_mbxep),	" Expire policy for mailbox rooms  ");
SUBSTRUCT_ELEMENT(INTEGER(c_mbxep.expire_mode);)
SUBSTRUCT_ELEMENT(INTEGER(c_mbxep.expire_value);)
CFG_VALUE(STRING_BUF(c_ldap_host, 128),		" Host where LDAP service lives    ");
CFG_VALUE(INTEGER(c_ldap_port),			" Port on host where LDAP lives    ");
CFG_VALUE(STRING_BUF(c_ldap_base_dn, 256),	" LDAP base DN                     ");
CFG_VALUE(STRING_BUF(c_ldap_bind_dn, 256),	" LDAP bind DN                     ");
CFG_VALUE(STRING_BUF(c_ldap_bind_pw, 256),	" LDAP bind password               ");
CFG_VALUE(INTEGER(c_msa_port),			" SMTP MSA listener port (usu 587) ");
CFG_VALUE(INTEGER(c_imaps_port),		" IMAPS listener port (usually 993)");
CFG_VALUE(INTEGER(c_pop3s_port),		" POP3S listener port (usually 995)");
CFG_VALUE(INTEGER(c_smtps_port),		" SMTPS listener port (usually 465)");
CFG_VALUE(CHAR(c_auto_cull),			" Cull db logs automatically?      ");
CFG_VALUE(CHAR(c_instant_expunge),		" IMAP instant expunge deleted msgs");
CFG_VALUE(CHAR(c_allow_spoofing),		" SMTP allow spoofing of my domains");
CFG_VALUE(CHAR(c_journal_email),		" Perform journaling of email      ");
CFG_VALUE(CHAR(c_journal_pubmsgs),		" Perform journaling of non-email  ");
CFG_VALUE(STRING_BUF(c_journal_dest, 128),	" Where to send journalized msgs   ");
CFG_VALUE(STRING_BUF(c_default_cal_zone, 128),	" Default calendar time zone       ");
CFG_VALUE(INTEGER(c_pftcpdict_port),    	" postfix tcptable support, see http://www.postfix.org/tcp_table.5.html ");
CFG_VALUE(INTEGER(c_managesieve_port),		" managesieve port. ");
CFG_VALUE(INTEGER(c_auth_mode),		" 0 = built-in Citadel auth), 1 = underlying host system auth ");
CFG_VALUE(STRING_BUF(c_funambol_host, 256),	" Funambol host. Blank to disable ");
CFG_VALUE(INTEGER(c_funambol_port),		" Funambol port ");
CFG_VALUE(STRING_BUF(c_funambol_source, 256),	" Funambol sync source ");
CFG_VALUE(STRING_BUF(c_funambol_auth, 256),	" Funambol auth details ");
CFG_VALUE(CHAR(c_rbl_at_greeting),		" Check RBL's at connect instead of after RCPT ");
CFG_VALUE(STRING_BUF(c_master_user, 32),	" Master user name ");
CFG_VALUE(STRING_BUF(c_master_pass, 32),	" Master user password ");
CFG_VALUE(STRING_BUF(c_pager_program, 256), 	" External pager program (blank to disable)");
CFG_VALUE(CHAR(c_imap_keep_from),		" IMAP keep original from header in msgs");
CFG_VALUE(INTEGER(c_xmpp_c2s_port),		" XMPP client-to-server port (usually 5222)");
CFG_VALUE(INTEGER(c_xmpp_s2s_port),		" XMPP server-to-server port (usually 5269)");
CFG_VALUE(TIME(c_pop3_fetch),		" How often to fetch POP3 messages");
CFG_VALUE(TIME(c_pop3_fastest),		" Users can specify POP3 fetching this often");
CFG_VALUE(INTEGER(c_spam_flag_only),	" 1 = flag instead of reject spam");
CFG_VALUE(INTEGER(c_xmpps_c2s_port),		" XMPP client-to-server SSL port (usually 5223)");
