/*
 * $Id$
 *
 */

#include "serv_extensions.h"
#include "citadel_dirs.h"
/* 
 * Global system configuration.
 * 
 * Developers: please do NOT remove the fields labelled "not in use".  We
 * can't simply remove them from the struct, because this gets written to
 * disk, and if you change it then you'll break all existing systems.
 * However, if you'd like to reclaim some of that space for another use, feel
 * free to do so, as long as the sizes are kept identical.
 */
struct config {
	char c_nodename[16];		/* Unqualified "short" nodename     */
	char c_fqdn[64];		/* Fully Qualified Domain Name      */
	char c_humannode[21];		/* Long name of system              */
	char c_phonenum[16];		/* Dialup number of system          */
	uid_t c_ctdluid;		/* UID under which we run Citadel   */
	char c_creataide;		/* room creator = room aide  flag   */
	int c_sleeping;			/* watchdog timer setting           */
	char c_initax;			/* initial access level             */
	char c_regiscall;		/* call number to register on       */
	char c_twitdetect;		/* twit detect flag                 */
	char c_twitroom[ROOMNAMELEN];	/* twit detect msg move to room     */
	char c_moreprompt[80];		/* paginator prompt                 */
	char c_restrict;		/* restrict Internet mail flag      */
	long c_niu_1;			/* (not in use)                     */
	char c_site_location[32];	/* physical location of server      */
	char c_sysadm[26];		/* name of system administrator     */
	char c_niu_2[15];		/* (not in use)                     */
	int c_setup_level;		/* what rev level we've setup to    */
	int c_maxsessions;		/* maximum concurrent sessions      */
	char c_ip_addr[20];		/* IP address to listen on          */
	int c_port_number;		/* Cit listener port (usually 504)  */
	int c_ipgm_secret;		/* Internal program authentication  */
	struct ExpirePolicy c_ep;	/* System default msg expire policy */
	int c_userpurge;		/* System default user purge (days) */
	int c_roompurge;		/* System default room purge (days) */
	char c_logpages[ROOMNAMELEN];	/* Room to log pages to (or not)    */
	char c_createax;		/* Axlevel required to create rooms */
	long c_maxmsglen;		/* Maximum message length           */
	int c_min_workers;		/* Lower limit on number of threads */
	int c_max_workers;		/* Upper limit on number of threads */
	int c_pop3_port;		/* POP3 listener port (usually 110) */
	int c_smtp_port;		/* SMTP listener port (usually 25)  */
	int c_rfc822_strict_from;	/* 1 = don't correct From: forgeries*/
	int c_aide_zap;			/* Are Aides allowed to zap rooms?  */
	int c_imap_port;		/* IMAP listener port (usually 143) */
	time_t c_net_freq;		/* how often to run the networker   */
	char c_disable_newu;		/* disable NEWU command             */
	char c_enable_fulltext;		/* enable full text indexing        */
	char c_baseroom[ROOMNAMELEN];	/* Name of baseroom (Lobby)	    */
	char c_aideroom[ROOMNAMELEN];	/* Name of aideroom (Aide)	    */
	int c_purge_hour;		/* Hour during which db purges run  */
	struct ExpirePolicy c_mbxep;	/* Expire policy for mailbox rooms  */
	char c_ldap_host[128];		/* Host where LDAP service lives    */
	int c_ldap_port;		/* Port on host where LDAP lives    */
	char c_ldap_base_dn[256];	/* LDAP base DN                     */
	char c_ldap_bind_dn[256];	/* LDAP bind DN                     */
	char c_ldap_bind_pw[256];	/* LDAP bind password               */
	int c_msa_port;			/* SMTP MSA listener port (usu 587) */
	int c_imaps_port;		/* IMAPS listener port (usually 993)*/
	int c_pop3s_port;		/* POP3S listener port (usually 995)*/
	int c_smtps_port;		/* SMTPS listener port (usually 465)*/
	char c_auto_cull;		/* Cull db logs automatically?      */
	char c_instant_expunge;		/* IMAP instant expunge deleted msgs*/
	char c_allow_spoofing;		/* SMTP allow spoofing of my domains*/
	char c_journal_email;		/* Perform journaling of email      */
	char c_journal_pubmsgs;		/* Perform journaling of non-email  */
	char c_journal_dest[128];	/* Where to send journalized msgs   */
	char c_default_cal_zone[128];	/* Default calendar time zone       */
	int c_pftcpdict_port;           /* postfix tcptable support, see http://www.postfix.org/tcp_table.5.html */
	int c_managesieve_port;         /* managesieve port. */
	int c_auth_mode;		/* 0 = built-in Citadel auth; 1 = underlying host system auth */
};


void get_config(void);
void put_config(void);
extern struct config config;

