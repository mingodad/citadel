/*
 * $Id$
 *
 * Administrative screen for site-wide configuration
 *
 */


#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <limits.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <stdarg.h>
#include <pthread.h>
#include <signal.h>
#include "webcit.h"





void display_siteconfig(void)
{
	char buf[SIZ];
	char *whichmenu;
	int i, j;

	/* expire policy settings */
	int sitepolicy = 0;
	int sitevalue = 0;
	int mboxpolicy = 0;
	int mboxvalue = 0;

	output_headers(1, 1, 2, 0, 0, 0, 0);
	wprintf("<div id=\"banner\">\n"
		"<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>"
		"<SPAN CLASS=\"titlebar\">Site configuration</SPAN>"
		"</TD></TR></TABLE>\n"
		"</div>\n<div id=\"content\">\n"
	);

	wprintf("<center><table border=0 width=99%% bgcolor=\"#ffffff\"><tr><td>");

	whichmenu = bstr("whichmenu");

	if (!strcmp(whichmenu, "")) {
		wprintf("<TABLE border=0 cellspacing=0 cellpadding=3 width=100%%>\n");

		wprintf("<TR BGCOLOR=\"#CCCCCC\"><TD>"
			"<A HREF=\"/display_siteconfig?whichmenu=general\">"
			"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
			"SRC=\"/static/advanced-icon.gif\" ALT=\"&nbsp;\">"
			"</TD><TD>"
			"<A HREF=\"/display_siteconfig?whichmenu=general\">"
			"<B>General</B><br />"
			"General site configuration items"
			"</A></TD></TR>\n"
		);

		wprintf("<TR><TD>"
			"<A HREF=\"/display_siteconfig?whichmenu=access\">"
			"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
			"SRC=\"/static/advanced-icon.gif\" ALT=\"&nbsp;\">"
			"</TD><TD>"
			"<A HREF=\"/display_siteconfig?whichmenu=access\">"
			"<B>Access</B><br />"
			"Access controls and site policy settings"
			"</A></TD></TR>\n"
		);

		wprintf("<TR BGCOLOR=\"#CCCCCC\"><TD>"
			"<A HREF=\"/display_siteconfig?whichmenu=network\">"
			"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
			"SRC=\"/static/advanced-icon.gif\" ALT=\"&nbsp;\">"
			"</TD><TD>"
			"<A HREF=\"/display_siteconfig?whichmenu=network\">"
			"<B>Network</B><br />"
			"Network services"
			"</A></TD></TR>\n"
		);

		wprintf("<TR><TD>"
			"<A HREF=\"/display_siteconfig?whichmenu=tuning\">"
			"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
			"SRC=\"/static/advanced-icon.gif\" ALT=\"&nbsp;\">"
			"</TD><TD>"
			"<A HREF=\"/display_siteconfig?whichmenu=tuning\">"
			"<B>Tuning</B><br />"
			"Advanced server fine-tuning controls"
			"</A></TD></TR>\n"
		);

		wprintf("<TR BGCOLOR=\"#CCCCCC\"><TD>"
			"<A HREF=\"/display_siteconfig?whichmenu=ldap\">"
			"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
			"SRC=\"/static/advanced-icon.gif\" ALT=\"&nbsp;\">"
			"</TD><TD>"
			"<A HREF=\"/display_siteconfig?whichmenu=ldap\">"
			"<B>Directory</B><br />"
			"Configure the LDAP connector for Citadel"
			"</A></TD></TR>\n"
		);

		wprintf("<TR><TD>"
			"<A HREF=\"/display_siteconfig?whichmenu=purger\">"
			"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
			"SRC=\"/static/advanced-icon.gif\" ALT=\"&nbsp;\">"
			"</TD><TD>"
			"<A HREF=\"/display_siteconfig?whichmenu=purger\">"
			"<B>Auto-purger</B><br />"
			"Configure automatic expiry of old messages"
			"</A></TD></TR>\n"
		);

		wprintf("</TABLE>");
		wprintf("</td></tr></table></center>\n");
		wDumpContent(1);
		return;
	}

	if (!strcasecmp(whichmenu, "general")) {
		wprintf("<CENTER><H2>General site configuration items</H2></CENTER>\n");
	}

	if (!strcasecmp(whichmenu, "access")) {
		wprintf("<CENTER><H2>Access controls and site policy settings</H2></CENTER>\n");
	}

	if (!strcasecmp(whichmenu, "network")) {
		wprintf("<CENTER><H2>Network services</H2>"
			"Changes made on this screen will not take effect until you restart the Citadel server."
			"</CENTER>\n");
	}

	if (!strcasecmp(whichmenu, "tuning")) {
		wprintf("<CENTER><H2>Advanced server fine-tuning controls</H2></CENTER>\n");
	}

	if (!strcasecmp(whichmenu, "ldap")) {
		wprintf("<CENTER><H2>Citadel LDAP connector configuration</H2>"
			"Changes made on this screen will not take effect until you restart the Citadel server."
			"</CENTER>\n");
	}

	if (!strcasecmp(whichmenu, "purger")) {
		wprintf("<CENTER><H2>Message auto-purger settings</H2>"
			"These settings may be overridden on a per-floor or per-room basis."
			"</CENTER>\n");
	}

	serv_printf("CONF get");
	serv_gets(buf);
	if (buf[0] != '1') {
        	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>");
        	wprintf("<SPAN CLASS=\"titlebar\">Error</SPAN>\n");
        	wprintf("</TD></TR></TABLE><br />\n");
        	wprintf("%s<br />\n", &buf[4]);
		do_template("endbox");
		wDumpContent(1);
		return;
	}

	wprintf("<FORM METHOD=\"POST\" ACTION=\"/siteconfig\">\n");
	wprintf("<TABLE border=0>\n");

	i = 0;
	while (serv_gets(buf), strcmp(buf, "000")) {
		switch (++i) {
		case 1:
			if (!strcasecmp(whichmenu, "general")) {
				wprintf("<TR><TD>Node name</TD><TD>");
				wprintf("<INPUT TYPE=\"text\" NAME=\"c_nodename\" MAXLENGTH=\"15\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"c_nodename\" VALUE=\"%s\">", buf);
			}
			break;
		case 2:
			if (!strcasecmp(whichmenu, "general")) {
				wprintf("<TR><TD>Fully qualified domain name</TD><TD>");
				wprintf("<INPUT TYPE=\"text\" NAME=\"c_fqdn\" MAXLENGTH=\"63\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"c_fqdn\" VALUE=\"%s\">", buf);
			}
			break;
		case 3:
			if (!strcasecmp(whichmenu, "general")) {
				wprintf("<TR><TD>Human-readable node name</TD><TD>");
				wprintf("<INPUT TYPE=\"text\" NAME=\"c_humannode\" MAXLENGTH=\"20\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"c_humannode\" VALUE=\"%s\">", buf);
			}
			break;
		case 4:
			if (!strcasecmp(whichmenu, "general")) {
				wprintf("<TR><TD>Landline telephone number</TD><TD>");
				wprintf("<INPUT TYPE=\"text\" NAME=\"c_phonenum\" MAXLENGTH=\"15\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"c_phonenum\" VALUE=\"%s\">", buf);
			}
			break;
		case 5:
			if (!strcasecmp(whichmenu, "access")) {
				wprintf("<TR><TD>Automatically grant room-aide status to users who create private rooms</TD><TD>");
				wprintf("<INPUT TYPE=\"checkbox\" NAME=\"c_creataide\" VALUE=\"yes\" %s>", ((atoi(buf) != 0) ? "CHECKED" : ""));
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"c_creataide\" VALUE=\"%s\">", buf);
			}
			break;
		case 6:
			if (!strcasecmp(whichmenu, "tuning")) {
				wprintf("<TR><TD>Server connection idle timeout (in seconds)</TD><TD>");
				wprintf("<INPUT TYPE=\"text\" NAME=\"c_sleeping\" MAXLENGTH=\"15\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"c_sleeping\" VALUE=\"%s\">", buf);
			}
			break;
		case 7:
			if (!strcasecmp(whichmenu, "access")) {
				wprintf("<TR><TD>Initial access level for new users</TD><TD>");
				wprintf("<SELECT NAME=\"c_initax\" SIZE=\"1\">\n");
				for (j=0; j<=6; ++j) {
					wprintf("<OPTION %s VALUE=\"%d\">%d - %s</OPTION>\n",
						((atoi(buf) == j) ? "SELECTED" : ""),
						j, j, axdefs[j]
					);
				}
				wprintf("</SELECT>");
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"c_initax\" VALUE=\"%s\">", buf);
			}
			break;
		case 8:
			if (!strcasecmp(whichmenu, "access")) {
				wprintf("<TR><TD>Require registration for new users</TD><TD>");
				wprintf("<INPUT TYPE=\"checkbox\" NAME=\"c_regiscall\" VALUE=\"yes\" %s>", ((atoi(buf) != 0) ? "CHECKED" : ""));
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"c_regiscall\" VALUE=\"%s\">", buf);
			}
			break;
		case 9:
			if (!strcasecmp(whichmenu, "access")) {
				wprintf("<TR><TD>Quarantine messages from problem users</TD><TD>");
				wprintf("<INPUT TYPE=\"checkbox\" NAME=\"c_twitdetect\" VALUE=\"yes\" %s>", ((atoi(buf) != 0) ? "CHECKED" : ""));
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"c_twitdetect\" VALUE=\"%s\">", buf);
			}
			break;
		case 10:
			if (!strcasecmp(whichmenu, "access")) {
				wprintf("<TR><TD>Name of quarantine room</TD><TD>");
				wprintf("<INPUT TYPE=\"text\" NAME=\"c_twitroom\" MAXLENGTH=\"63\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"c_twitroom\" VALUE=\"%s\">", buf);
			}
			break;
		case 11:
			if (!strcasecmp(whichmenu, "general")) {
				wprintf("<TR><TD>Paginator prompt (for text mode clients)</TD><TD>");
				wprintf("<INPUT TYPE=\"text\" NAME=\"c_moreprompt\" MAXLENGTH=\"79\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"c_moreprompt\" VALUE=\"%s\">", buf);
			}
			break;
		case 12:
			if (!strcasecmp(whichmenu, "access")) {
				wprintf("<TR><TD>Restrict access to Internet mail</TD><TD>");
				wprintf("<INPUT TYPE=\"checkbox\" NAME=\"c_restrict\" VALUE=\"yes\" %s>", ((atoi(buf) != 0) ? "CHECKED" : ""));
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"c_restrict\" VALUE=\"%s\">", buf);
			}
			break;
		case 13:
			if (!strcasecmp(whichmenu, "general")) {
				wprintf("<TR><TD>Geographic location of this system</TD><TD>");
				wprintf("<INPUT TYPE=\"text\" NAME=\"c_bbs_city\" MAXLENGTH=\"31\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"c_bbs_city\" VALUE=\"%s\">", buf);
			}
			break;
		case 14:
			if (!strcasecmp(whichmenu, "general")) {
				wprintf("<TR><TD>Name of system administrator</TD><TD>");
				wprintf("<INPUT TYPE=\"text\" NAME=\"c_sysadm\" MAXLENGTH=\"25\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"c_sysadm\" VALUE=\"%s\">", buf);
			}
			break;
		case 15:
			if (!strcasecmp(whichmenu, "tuning")) {
				wprintf("<TR><TD>Maximum concurrent sessions</TD><TD>");
				wprintf("<INPUT TYPE=\"text\" NAME=\"c_maxsessions\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"c_maxsessions\" VALUE=\"%s\">", buf);
			}
			break;
		case 17:
			if (!strcasecmp(whichmenu, "tuning")) {
				wprintf("<TR><TD>Default user purge time (days)</TD><TD>");
				wprintf("<INPUT TYPE=\"text\" NAME=\"c_userpurge\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"c_userpurge\" VALUE=\"%s\">", buf);
			}
			break;
		case 18:
			if (!strcasecmp(whichmenu, "tuning")) {
				wprintf("<TR><TD>Default room purge time (days)</TD><TD>");
				wprintf("<INPUT TYPE=\"text\" NAME=\"c_roompurge\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"c_roompurge\" VALUE=\"%s\">", buf);
			}
			break;
		case 19:
			if (!strcasecmp(whichmenu, "access")) {
				wprintf("<TR><TD>Name of room to log pages</TD><TD>");
				wprintf("<INPUT TYPE=\"text\" NAME=\"c_logpages\" MAXLENGTH=\"63\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"c_logpages\" VALUE=\"%s\">", buf);
			}
			break;
		case 20:
			if (!strcasecmp(whichmenu, "access")) {
				wprintf("<TR><TD>Access level required to create rooms</TD><TD>");
				wprintf("<SELECT NAME=\"c_createax\" SIZE=\"1\">\n");
				for (j=0; j<=6; ++j) {
					wprintf("<OPTION %s VALUE=\"%d\">%d - %s</OPTION>\n",
						((atoi(buf) == j) ? "SELECTED" : ""),
						j, j, axdefs[j]
					);
				}
				wprintf("</SELECT>");
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"c_createax\" VALUE=\"%s\">", buf);
			}
			break;
		case 21:
			if (!strcasecmp(whichmenu, "tuning")) {
				wprintf("<TR><TD>Maximum message length</TD><TD>");
				wprintf("<INPUT TYPE=\"text\" NAME=\"c_maxmsglen\" MAXLENGTH=\"20\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"c_maxmsglen\" VALUE=\"%s\">", buf);
			}
			break;
		case 22:
			if (!strcasecmp(whichmenu, "tuning")) {
				wprintf("<TR><TD>Minimum number of worker threads</TD><TD>");
				wprintf("<INPUT TYPE=\"text\" NAME=\"c_min_workers\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"c_min_workers\" VALUE=\"%s\">", buf);
			}
			break;
		case 23:
			if (!strcasecmp(whichmenu, "tuning")) {
				wprintf("<TR><TD>Maximum number of worker threads</TD><TD>");
				wprintf("<INPUT TYPE=\"text\" NAME=\"c_max_workers\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"c_max_workers\" VALUE=\"%s\">", buf);
			}
			break;
		case 24:
			if (!strcasecmp(whichmenu, "network")) {
				wprintf("<TR><TD>POP3 listener port (-1 to disable)</TD><TD>");
				wprintf("<INPUT TYPE=\"text\" NAME=\"c_pop3_port\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"c_pop3_port\" VALUE=\"%s\">", buf);
			}
			break;
		case 25:
			if (!strcasecmp(whichmenu, "network")) {
				wprintf("<TR><TD>SMTP MTA port (-1 to disable)</TD><TD>");
				wprintf("<INPUT TYPE=\"text\" NAME=\"c_smtp_port\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"c_smtp_port\" VALUE=\"%s\">", buf);
			}
			break;
		case 27:
			if (!strcasecmp(whichmenu, "access")) {
				wprintf("<TR><TD>Allow aides to zap (forget) rooms</TD><TD>");
				wprintf("<INPUT TYPE=\"checkbox\" NAME=\"c_aide_zap\" VALUE=\"yes\" %s>", ((atoi(buf) != 0) ? "CHECKED" : ""));
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"c_aide_zap\" VALUE=\"%s\">", buf);
			}
			break;
		case 28:
			if (!strcasecmp(whichmenu, "network")) {
				wprintf("<TR><TD>IMAP listener port (-1 to disable)</TD><TD>");
				wprintf("<INPUT TYPE=\"text\" NAME=\"c_imap_port\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"c_imap_port\" VALUE=\"%s\">", buf);
			}
			break;
		case 29:
			if (!strcasecmp(whichmenu, "network")) {
				wprintf("<TR><TD>Network run frequency (in seconds)</TD><TD>");
				wprintf("<INPUT TYPE=\"text\" NAME=\"c_net_freq\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"c_net_freq\" VALUE=\"%s\">", buf);
			}
			break;
		case 30:
			if (!strcasecmp(whichmenu, "access")) {
				wprintf("<TR><TD>Disable self-service user account creation</TD><TD>");
				wprintf("<INPUT TYPE=\"checkbox\" NAME=\"c_disable_newu\" VALUE=\"yes\" %s>", ((atoi(buf) != 0) ? "CHECKED" : ""));
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"c_disable_newu\" VALUE=\"%s\">", buf);
			}
			break;
		case 31:
			/* placeholder -- there is no option 31 */
			break;
		case 32:
			if (!strcasecmp(whichmenu, "tuning")) {
				wprintf("<TR><TD>Hour to run database auto-purge</TD><TD>");
				wprintf("<SELECT NAME=\"c_purge_hour\" SIZE=\"1\">\n");
				for (j=0; j<=23; ++j) {
					wprintf("<OPTION %s VALUE=\"%d\">%d:00%s</OPTION>\n",
						((atoi(buf) == j) ? "SELECTED" : ""),
						j,
						((j == 0) ? 12 : ((j>12) ? j-12 : j)),
						((j >= 12) ? "pm" : "am")
					);
				}
				wprintf("</SELECT>");
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"c_purge_hour\" VALUE=\"%s\">", buf);
			}
			break;
		case 33:
			if ( (serv_info.serv_supports_ldap) && (!strcasecmp(whichmenu, "ldap")) ) {
				wprintf("<TR><TD>Host name of LDAP server (blank to disable)</TD><TD>");
				wprintf("<INPUT TYPE=\"text\" NAME=\"c_ldap_host\" MAXLENGTH=\"127\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"c_ldap_host\" VALUE=\"%s\">", buf);
			}
			break;
		case 34:
			if ( (serv_info.serv_supports_ldap) && (!strcasecmp(whichmenu, "ldap")) ) {
				wprintf("<TR><TD>Port number of LDAP server (blank to disable)</TD><TD>");
				wprintf("<INPUT TYPE=\"text\" NAME=\"c_ldap_port\" MAXLENGTH=\"127\" VALUE=\"%d\">", atoi(buf));
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"c_ldap_port\" VALUE=\"%d\">", atoi(buf));
			}
			break;
		case 35:
			if ( (serv_info.serv_supports_ldap) && (!strcasecmp(whichmenu, "ldap")) ) {
				wprintf("<TR><TD>Base DN</TD><TD>");
				wprintf("<INPUT TYPE=\"text\" NAME=\"c_ldap_base_dn\" MAXLENGTH=\"255\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"c_ldap_base_dn\" VALUE=\"%s\">", buf);
			}
			break;
		case 36:
			if ( (serv_info.serv_supports_ldap) && (!strcasecmp(whichmenu, "ldap")) ) {
				wprintf("<TR><TD>Bind DN</TD><TD>");
				wprintf("<INPUT TYPE=\"text\" NAME=\"c_ldap_bind_dn\" MAXLENGTH=\"255\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"c_ldap_bind_dn\" VALUE=\"%s\">", buf);
			}
			break;
		case 37:
			if ( (serv_info.serv_supports_ldap) && (!strcasecmp(whichmenu, "ldap")) ) {
				wprintf("<TR><TD>Password for bind DN</TD><TD>");
				wprintf("<INPUT TYPE=\"password\" NAME=\"c_ldap_bind_pw\" MAXLENGTH=\"255\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"c_ldap_bind_pw\" VALUE=\"%s\">", buf);
			}
			break;
		case 38:
			if (!strcasecmp(whichmenu, "network")) {
				wprintf("<TR><TD>Server IP address (0.0.0.0 for 'any')</TD><TD>");
				wprintf("<INPUT TYPE=\"text\" NAME=\"c_ip_addr\" MAXLENGTH=\"15\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"c_ip_addr\" VALUE=\"%s\">", buf);
			}
			break;
		case 39:
			if (!strcasecmp(whichmenu, "network")) {
				wprintf("<TR><TD>SMTP MSA port (-1 to disable)</TD><TD>");
				wprintf("<INPUT TYPE=\"text\" NAME=\"c_msa_port\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"c_msa_port\" VALUE=\"%s\">", buf);
			}
			break;
		case 40:
			if (!strcasecmp(whichmenu, "network")) {
				wprintf("<TR><TD>IMAP over SSL port (-1 to disable)</TD><TD>");
				wprintf("<INPUT TYPE=\"text\" NAME=\"c_imaps_port\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"c_imaps_port\" VALUE=\"%s\">", buf);
			}
			break;
		case 41:
			if (!strcasecmp(whichmenu, "network")) {
				wprintf("<TR><TD>POP3 over SSL port (-1 to disable)</TD><TD>");
				wprintf("<INPUT TYPE=\"text\" NAME=\"c_pop3s_port\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"c_pop3s_port\" VALUE=\"%s\">", buf);
			}
			break;
		case 42:
			if (!strcasecmp(whichmenu, "network")) {
				wprintf("<TR><TD>SMTP over SSL port (-1 to disable)</TD><TD>");
				wprintf("<INPUT TYPE=\"text\" NAME=\"c_smtps_port\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"c_smtps_port\" VALUE=\"%s\">", buf);
			}
			break;
		}
	}

	serv_puts("GPEX site");
	serv_gets(buf);
	if (buf[0] == '2') {
		sitepolicy = extract_int(&buf[4], 0);
		sitevalue = extract_int(&buf[4], 1);
	}

	serv_puts("GPEX mailboxes");
	serv_gets(buf);
	if (buf[0] == '2') {
		mboxpolicy = extract_int(&buf[4], 0);
		mboxvalue = extract_int(&buf[4], 1);
	}

	if (!strcasecmp(whichmenu, "purger")) {

		wprintf("<TR><TD COLSPAN=2><hr /></TD></TR>\n");

		wprintf("<TR><TD>Default message expire policy for public rooms</TD><TD>");
		wprintf("<INPUT TYPE=\"radio\" NAME=\"sitepolicy\" VALUE=\"1\" %s>",
			((sitepolicy == 1) ? "CHECKED" : "") );
		wprintf("Never automatically expire messages<br />\n");
		wprintf("<INPUT TYPE=\"radio\" NAME=\"sitepolicy\" VALUE=\"2\" %s>",
			((sitepolicy == 2) ? "CHECKED" : "") );
		wprintf("Expire by message count<br />\n");
		wprintf("<INPUT TYPE=\"radio\" NAME=\"sitepolicy\" VALUE=\"3\" %s>",
			((sitepolicy == 3) ? "CHECKED" : "") );
		wprintf("Expire by message age<br />");
		wprintf("Number of messages or days: ");
		wprintf("<INPUT TYPE=\"text\" NAME=\"sitevalue\" MAXLENGTH=\"5\" VALUE=\"%d\">", sitevalue);
		wprintf("</TD></TR>\n");

		wprintf("<TR><TD COLSPAN=2><hr /></TD></TR>\n");

		wprintf("<TR><TD>Default message expire policy for private mailboxes</TD><TD>");
		wprintf("<INPUT TYPE=\"radio\" NAME=\"mboxpolicy\" VALUE=\"0\" %s>",
			((mboxpolicy == 0) ? "CHECKED" : "") );
		wprintf("Same policy as public rooms<br />\n");
		wprintf("<INPUT TYPE=\"radio\" NAME=\"mboxpolicy\" VALUE=\"1\" %s>",
			((mboxpolicy == 1) ? "CHECKED" : "") );
		wprintf("Never automatically expire messages<br />\n");
		wprintf("<INPUT TYPE=\"radio\" NAME=\"mboxpolicy\" VALUE=\"2\" %s>",
			((mboxpolicy == 2) ? "CHECKED" : "") );
		wprintf("Expire by message count<br />\n");
		wprintf("<INPUT TYPE=\"radio\" NAME=\"mboxpolicy\" VALUE=\"3\" %s>",
			((mboxpolicy == 3) ? "CHECKED" : "") );
		wprintf("Expire by message age<br />");
		wprintf("Number of messages or days: ");
		wprintf("<INPUT TYPE=\"text\" NAME=\"mboxvalue\" MAXLENGTH=\"5\" VALUE=\"%d\">", mboxvalue);
		wprintf("</TD></TR>\n");

		wprintf("<TR><TD COLSPAN=2><hr /></TD></TR>\n");

	}
	else {
		wprintf("<INPUT TYPE=\"hidden\" NAME=\"sitepolicy\" VALUE=\"%d\">\n", sitepolicy);
		wprintf("<INPUT TYPE=\"hidden\" NAME=\"sitevalue\" VALUE=\"%d\">\n", sitevalue);
		wprintf("<INPUT TYPE=\"hidden\" NAME=\"mboxpolicy\" VALUE=\"%d\">\n", mboxpolicy);
		wprintf("<INPUT TYPE=\"hidden\" NAME=\"mboxvalue\" VALUE=\"%d\">\n", mboxvalue);
	}

	wprintf("</TABLE><CENTER>");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"OK\">");
	wprintf("&nbsp;");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Cancel\">\n");
	wprintf("</CENTER></FORM>\n");
	wprintf("</td></tr></table></center>\n");
	wDumpContent(1);
}


void siteconfig(void)
{
	char buf[SIZ];

	if (strcasecmp(bstr("sc"), "OK")) {
		display_siteconfig();
		return;
	}
	serv_printf("CONF set");
	serv_gets(buf);
	if (buf[0] != '4') {
		strcpy(WC->ImportantMessage, &buf[4]);
		display_siteconfig();
		return;
	}
	serv_printf("%s", bstr("c_nodename"));
	serv_printf("%s", bstr("c_fqdn"));
	serv_printf("%s", bstr("c_humannode"));
	serv_printf("%s", bstr("c_phonenum"));
	serv_printf("%s", ((!strcasecmp(bstr("c_creataide"), "yes") ? "1" : "0")));
	serv_printf("%s", bstr("c_sleeping"));
	serv_printf("%s", bstr("c_initax"));
	serv_printf("%s", ((!strcasecmp(bstr("c_regiscall"), "yes") ? "1" : "0")));
	serv_printf("%s", ((!strcasecmp(bstr("c_twitdetect"), "yes") ? "1" : "0")));
	serv_printf("%s", bstr("c_twitroom"));
	serv_printf("%s", bstr("c_moreprompt"));
	serv_printf("%s", ((!strcasecmp(bstr("c_restrict"), "yes") ? "1" : "0")));
	serv_printf("%s", bstr("c_bbs_city"));
	serv_printf("%s", bstr("c_sysadm"));
	serv_printf("%s", bstr("c_maxsessions"));
	serv_printf("");  /* networking password -- deprecated */
	serv_printf("%s", bstr("c_userpurge"));
	serv_printf("%s", bstr("c_roompurge"));
	serv_printf("%s", bstr("c_logpages"));
	serv_printf("%s", bstr("c_createax"));
	serv_printf("%s", bstr("c_maxmsglen"));
	serv_printf("%s", bstr("c_min_workers"));
	serv_printf("%s", bstr("c_max_workers"));
	serv_printf("%s", bstr("c_pop3_port"));
	serv_printf("%s", bstr("c_smtp_port"));
	serv_printf("");  /* moderation filter level -- not yet implemented */
	serv_printf("%s", ((!strcasecmp(bstr("c_aide_zap"), "yes") ? "1" : "0")));
	serv_printf("%s", bstr("c_imap_port"));
	serv_printf("%s", bstr("c_net_freq"));
	serv_printf("%s", ((!strcasecmp(bstr("c_disable_newu"), "yes") ? "1" : "0")));
	serv_printf("1");	/* placeholder -- deprecated */
	serv_printf("%s", bstr("c_purge_hour"));
	serv_printf("%s", bstr("c_ldap_host"));
	serv_printf("%s", bstr("c_ldap_port"));
	serv_printf("%s", bstr("c_ldap_base_dn"));
	serv_printf("%s", bstr("c_ldap_bind_dn"));
	serv_printf("%s", bstr("c_ldap_bind_pw"));
	serv_printf("%s", bstr("c_ip_addr"));
	serv_printf("%s", bstr("c_msa_port"));
	serv_printf("%s", bstr("c_imaps_port"));
	serv_printf("%s", bstr("c_pop3s_port"));
	serv_printf("%s", bstr("c_smtps_port"));
	serv_printf("000");

	serv_printf("SPEX site|%d|%d", atoi(bstr("sitepolicy")), atoi(bstr("sitevalue")));
	serv_gets(buf);
	serv_printf("SPEX mailboxes|%d|%d", atoi(bstr("mboxpolicy")), atoi(bstr("mboxvalue")));
	serv_gets(buf);

	strcpy(WC->ImportantMessage, "System configuration has been updated.");
	display_siteconfig();
}
