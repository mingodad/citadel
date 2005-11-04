/*
 * $Id$
 *
 * Administrative screen for site-wide configuration
 *
 */


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

	output_headers(1, 1, 2, 0, 0, 0);
	wprintf("<div id=\"banner\">\n"
		"<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>"
		"<SPAN CLASS=\"titlebar\">");
	wprintf(_("Site configuration"));
	wprintf("</SPAN>"
		"</TD></TR></TABLE>\n"
		"</div>\n<div id=\"content\">\n"
	);

	wprintf("<div id=\"fix_scrollbar_bug\">"
		"<table border=0 width=100%% bgcolor=\"#ffffff\"><tr><td>");

	whichmenu = bstr("whichmenu");

	if (!strcmp(whichmenu, "")) {
		wprintf("<TABLE border=0 cellspacing=0 cellpadding=3 width=100%%>\n");

		wprintf("<TR BGCOLOR=\"#CCCCCC\"><TD>"
			"<a href=\"/display_siteconfig?whichmenu=general\">"
			"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
			"src=\"/static/advanpage2_48x.gif\" ALT=\"&nbsp;\">"
			"</TD><TD>"
			"<a href=\"/display_siteconfig?whichmenu=general\">"
			"<B>%s</B><br />"
			"%s"
			"</A></TD></TR>\n",
			_("General"),
			_("General site configuration items")
		);

		wprintf("<TR><TD>"
			"<a href=\"/display_siteconfig?whichmenu=access\">"
			"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
			"src=\"/static/advanpage2_48x.gif\" ALT=\"&nbsp;\">"
			"</TD><TD>"
			"<a href=\"/display_siteconfig?whichmenu=access\">"
			"<B>%s</B><br />"
			"%s"
			"</A></TD></TR>\n",
			_("Access"),
			_("Access controls and site policy settings")
		);

		wprintf("<TR BGCOLOR=\"#CCCCCC\"><TD>"
			"<a href=\"/display_siteconfig?whichmenu=network\">"
			"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
			"src=\"/static/advanpage2_48x.gif\" ALT=\"&nbsp;\">"
			"</TD><TD>"
			"<a href=\"/display_siteconfig?whichmenu=network\">"
			"<B>%s</B><br />"
			"%s"
			"</A></TD></TR>\n",
			_("Network"),
			_("Network services")
		);

		wprintf("<TR><TD>"
			"<a href=\"/display_siteconfig?whichmenu=tuning\">"
			"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
			"src=\"/static/advanpage2_48x.gif\" ALT=\"&nbsp;\">"
			"</TD><TD>"
			"<a href=\"/display_siteconfig?whichmenu=tuning\">"
			"<B>%s</B><br />"
			"%s"
			"</A></TD></TR>\n",
			_("Tuning"),
			_("Advanced server fine-tuning controls")
		);

		wprintf("<TR BGCOLOR=\"#CCCCCC\"><TD>"
			"<a href=\"/display_siteconfig?whichmenu=ldap\">"
			"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
			"src=\"/static/advanpage2_48x.gif\" ALT=\"&nbsp;\">"
			"</TD><TD>"
			"<a href=\"/display_siteconfig?whichmenu=ldap\">"
			"<B>%s</B><br />"
			"%s"
			"</A></TD></TR>\n",
			_("Directory"),
			_("Configure the LDAP connector for Citadel")
		);

		wprintf("<TR><TD>"
			"<a href=\"/display_siteconfig?whichmenu=purger\">"
			"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
			"src=\"/static/advanpage2_48x.gif\" ALT=\"&nbsp;\">"
			"</TD><TD>"
			"<a href=\"/display_siteconfig?whichmenu=purger\">"
			"<B>%s</B><br />"
			"%s"
			"</A></TD></TR>\n",
			_("Auto-purger"),
			_("Configure automatic expiry of old messages")
		);

		wprintf("</TABLE>");
		wprintf("</td></tr></table></center>\n");
		wDumpContent(1);
		return;
	}

	if (!strcasecmp(whichmenu, "general")) {
		wprintf("<div align=\"center\"><h2>");
		wprintf(_("General site configuration items"));
		wprintf("</h2></div>\n");
	}

	if (!strcasecmp(whichmenu, "access")) {
		wprintf("<div align=\"center\"><h2>");
		wprintf(_("Access controls and site policy settings"));
		wprintf("</h2></div>\n");
	}

	if (!strcasecmp(whichmenu, "network")) {
		wprintf("<div align=\"center\"><h2>");
		wprintf(_("Network services"));
		wprintf("</h2>");
		wprintf(_("Changes made on this screen will not take effect "
			"until you restart the Citadel server."));
		wprintf("</div>\n");
	}

	if (!strcasecmp(whichmenu, "tuning")) {
		wprintf("<div align=\"center\"><h2>");
		wprintf(_("Advanced server fine-tuning controls"));
		wprintf("</h2></div>\n");
	}

	if (!strcasecmp(whichmenu, "ldap")) {
		wprintf("<div align=\"center\"><h2>");
		wprintf(_("Citadel LDAP connector configuration"));
		wprintf("</h2>");
		wprintf(_("Changes made on this screen will not take effect "
			"until you restart the Citadel server."));
		wprintf("</div>\n");
	}

	if (!strcasecmp(whichmenu, "purger")) {
		wprintf("<div align=\"center\"><h2>");
		wprintf(_("Message auto-purger settings"));
		wprintf("</h2>");
		wprintf(_("These settings may be overridden on a per-floor or per-room basis."));
		wprintf("</div>\n");
	}

	serv_printf("CONF get");
	serv_getln(buf, sizeof buf);
	if (buf[0] != '1') {
        	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>");
        	wprintf("<SPAN CLASS=\"titlebar\">");
		wprintf(_("Error"));
		wprintf("</SPAN>\n");
        	wprintf("</TD></TR></TABLE><br />\n");
        	wprintf("%s<br />\n", &buf[4]);
		do_template("endbox");
		wDumpContent(1);
		return;
	}

	wprintf("<FORM METHOD=\"POST\" action=\"/siteconfig\">\n");
	wprintf("<TABLE border=0>\n");

	i = 0;
	while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		switch (++i) {
		case 1:
			if (!strcasecmp(whichmenu, "general")) {
				wprintf("<TR><TD>");
				wprintf(_("Node name"));
				wprintf("</TD><TD>");
				wprintf("<input type=\"text\" NAME=\"c_nodename\" MAXLENGTH=\"15\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<input type=\"hidden\" NAME=\"c_nodename\" VALUE=\"%s\">", buf);
			}
			break;
		case 2:
			if (!strcasecmp(whichmenu, "general")) {
				wprintf("<TR><TD>");
				wprintf(_("Fully qualified domain name"));
				wprintf("</TD><TD>");
				wprintf("<input type=\"text\" NAME=\"c_fqdn\" MAXLENGTH=\"63\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<input type=\"hidden\" NAME=\"c_fqdn\" VALUE=\"%s\">", buf);
			}
			break;
		case 3:
			if (!strcasecmp(whichmenu, "general")) {
				wprintf("<TR><TD>");
				wprintf(_("Human-readable node name"));
				wprintf("</TD><TD>");
				wprintf("<input type=\"text\" NAME=\"c_humannode\" MAXLENGTH=\"20\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<input type=\"hidden\" NAME=\"c_humannode\" VALUE=\"%s\">", buf);
			}
			break;
		case 4:
			if (!strcasecmp(whichmenu, "general")) {
				wprintf("<TR><TD>");
				wprintf(_("Telephone number"));
				wprintf("</TD><TD>");
				wprintf("<input type=\"text\" NAME=\"c_phonenum\" MAXLENGTH=\"15\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<input type=\"hidden\" NAME=\"c_phonenum\" VALUE=\"%s\">", buf);
			}
			break;
		case 5:
			if (!strcasecmp(whichmenu, "access")) {
				wprintf("<TR><TD>");
				wprintf(_("Automatically grant room-aide status to users who create private rooms"));
				wprintf("</TD><TD>");
				wprintf("<input type=\"checkbox\" NAME=\"c_creataide\" VALUE=\"yes\" %s>",
					((atoi(buf) != 0) ? "CHECKED" : ""));
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<input type=\"hidden\" NAME=\"c_creataide\" VALUE=\"%s\">", buf);
			}
			break;
		case 6:
			if (!strcasecmp(whichmenu, "tuning")) {
				wprintf("<TR><TD>");
				wprintf(_("Server connection idle timeout (in seconds)"));
				wprintf("</TD><TD>");
				wprintf("<input type=\"text\" NAME=\"c_sleeping\" MAXLENGTH=\"15\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<input type=\"hidden\" NAME=\"c_sleeping\" VALUE=\"%s\">", buf);
			}
			break;
		case 7:
			if (!strcasecmp(whichmenu, "access")) {
				wprintf("<TR><TD>");
				wprintf(_("Initial access level for new users"));
				wprintf("</TD><TD>");
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
				wprintf("<input type=\"hidden\" NAME=\"c_initax\" VALUE=\"%s\">", buf);
			}
			break;
		case 8:
			if (!strcasecmp(whichmenu, "access")) {
				wprintf("<TR><TD>");
				wprintf(_("Require registration for new users"));
				wprintf("</TD><TD>");
				wprintf("<input type=\"checkbox\" NAME=\"c_regiscall\" VALUE=\"yes\" %s>",
					((atoi(buf) != 0) ? "CHECKED" : ""));
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<input type=\"hidden\" NAME=\"c_regiscall\" VALUE=\"%s\">", buf);
			}
			break;
		case 9:
			if (!strcasecmp(whichmenu, "access")) {
				wprintf("<TR><TD>");
				wprintf(_("Quarantine messages from problem users"));
				wprintf("</TD><TD>");
				wprintf("<input type=\"checkbox\" NAME=\"c_twitdetect\" VALUE=\"yes\" %s>",
					((atoi(buf) != 0) ? "CHECKED" : ""));
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<input type=\"hidden\" NAME=\"c_twitdetect\" VALUE=\"%s\">", buf);
			}
			break;
		case 10:
			if (!strcasecmp(whichmenu, "access")) {
				wprintf("<TR><TD>");
				wprintf(_("Name of quarantine room"));
				wprintf("</TD><TD>");
				wprintf("<input type=\"text\" NAME=\"c_twitroom\" MAXLENGTH=\"63\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<input type=\"hidden\" NAME=\"c_twitroom\" VALUE=\"%s\">", buf);
			}
			break;
		case 11:
			if (!strcasecmp(whichmenu, "general")) {
				wprintf("<TR><TD>");
				wprintf(_("Paginator prompt (for text mode clients)"));
				wprintf("</TD><TD>");
				wprintf("<input type=\"text\" NAME=\"c_moreprompt\" MAXLENGTH=\"79\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<input type=\"hidden\" NAME=\"c_moreprompt\" VALUE=\"%s\">", buf);
			}
			break;
		case 12:
			if (!strcasecmp(whichmenu, "access")) {
				wprintf("<TR><TD>");
				wprintf(_("Restrict access to Internet mail"));
				wprintf("</TD><TD>");
				wprintf("<input type=\"checkbox\" NAME=\"c_restrict\" VALUE=\"yes\" %s>",
					((atoi(buf) != 0) ? "CHECKED" : ""));
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<input type=\"hidden\" NAME=\"c_restrict\" VALUE=\"%s\">", buf);
			}
			break;
		case 13:
			if (!strcasecmp(whichmenu, "general")) {
				wprintf("<TR><TD>");
				wprintf(_("Geographic location of this system"));
				wprintf("</TD><TD>");
				wprintf("<input type=\"text\" NAME=\"c_bbs_city\" MAXLENGTH=\"31\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<input type=\"hidden\" NAME=\"c_bbs_city\" VALUE=\"%s\">", buf);
			}
			break;
		case 14:
			if (!strcasecmp(whichmenu, "general")) {
				wprintf("<TR><TD>");
				wprintf(_("Name of system administrator"));
				wprintf("</TD><TD>");
				wprintf("<input type=\"text\" NAME=\"c_sysadm\" MAXLENGTH=\"25\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<input type=\"hidden\" NAME=\"c_sysadm\" VALUE=\"%s\">", buf);
			}
			break;
		case 15:
			if (!strcasecmp(whichmenu, "tuning")) {
				wprintf("<TR><TD>");
				wprintf(_("Maximum concurrent sessions (0 = no limit)"));
				wprintf("</TD><TD>");
				wprintf("<input type=\"text\" NAME=\"c_maxsessions\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<input type=\"hidden\" NAME=\"c_maxsessions\" VALUE=\"%s\">", buf);
			}
			break;
		case 17:
			if (!strcasecmp(whichmenu, "tuning")) {
				wprintf("<TR><TD>");
				wprintf(_("Default user purge time (days)"));
				wprintf("</TD><TD>");
				wprintf("<input type=\"text\" NAME=\"c_userpurge\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<input type=\"hidden\" NAME=\"c_userpurge\" VALUE=\"%s\">", buf);
			}
			break;
		case 18:
			if (!strcasecmp(whichmenu, "tuning")) {
				wprintf("<TR><TD>");
				wprintf(_("Default room purge time (days)"));
				wprintf("</TD><TD>");
				wprintf("<input type=\"text\" NAME=\"c_roompurge\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<input type=\"hidden\" NAME=\"c_roompurge\" VALUE=\"%s\">", buf);
			}
			break;
		case 19:
			if (!strcasecmp(whichmenu, "access")) {
				wprintf("<TR><TD>");
				wprintf(_("Name of room to log pages"));
				wprintf("</TD><TD>");
				wprintf("<input type=\"text\" NAME=\"c_logpages\" MAXLENGTH=\"63\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<input type=\"hidden\" NAME=\"c_logpages\" VALUE=\"%s\">", buf);
			}
			break;
		case 20:
			if (!strcasecmp(whichmenu, "access")) {
				wprintf("<TR><TD>");
				wprintf(_("Access level required to create rooms"));
				wprintf("</TD><TD>");
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
				wprintf("<input type=\"hidden\" NAME=\"c_createax\" VALUE=\"%s\">", buf);
			}
			break;
		case 21:
			if (!strcasecmp(whichmenu, "tuning")) {
				wprintf("<TR><TD>");
				wprintf(_("Maximum message length"));
				wprintf("</TD><TD>");
				wprintf("<input type=\"text\" NAME=\"c_maxmsglen\" MAXLENGTH=\"20\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<input type=\"hidden\" NAME=\"c_maxmsglen\" VALUE=\"%s\">", buf);
			}
			break;
		case 22:
			if (!strcasecmp(whichmenu, "tuning")) {
				wprintf("<TR><TD>");
				wprintf(_("Minimum number of worker threads"));
				wprintf("</TD><TD>");
				wprintf("<input type=\"text\" NAME=\"c_min_workers\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<input type=\"hidden\" NAME=\"c_min_workers\" VALUE=\"%s\">", buf);
			}
			break;
		case 23:
			if (!strcasecmp(whichmenu, "tuning")) {
				wprintf("<TR><TD>");
				wprintf(_("Maximum number of worker threads"));
				wprintf("</TD><TD>");
				wprintf("<input type=\"text\" NAME=\"c_max_workers\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<input type=\"hidden\" NAME=\"c_max_workers\" VALUE=\"%s\">", buf);
			}
			break;
		case 24:
			if (!strcasecmp(whichmenu, "network")) {
				wprintf("<TR><TD>");
				wprintf(_("POP3 listener port (-1 to disable)"));
				wprintf("</TD><TD>");
				wprintf("<input type=\"text\" NAME=\"c_pop3_port\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<input type=\"hidden\" NAME=\"c_pop3_port\" VALUE=\"%s\">", buf);
			}
			break;
		case 25:
			if (!strcasecmp(whichmenu, "network")) {
				wprintf("<TR><TD>");
				wprintf(_("SMTP MTA port (-1 to disable)"));
				wprintf("</TD><TD>");
				wprintf("<input type=\"text\" NAME=\"c_smtp_port\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<input type=\"hidden\" NAME=\"c_smtp_port\" VALUE=\"%s\">", buf);
			}
			break;
		case 27:
			if (!strcasecmp(whichmenu, "access")) {
				wprintf("<TR><TD>");
				wprintf(_("Allow aides to zap (forget) rooms"));
				wprintf("</TD><TD>");
				wprintf("<input type=\"checkbox\" NAME=\"c_aide_zap\" VALUE=\"yes\" %s>",
					((atoi(buf) != 0) ? "CHECKED" : ""));
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<input type=\"hidden\" NAME=\"c_aide_zap\" VALUE=\"%s\">", buf);
			}
			break;
		case 28:
			if (!strcasecmp(whichmenu, "network")) {
				wprintf("<TR><TD>");
				wprintf(_("IMAP listener port (-1 to disable)"));
				wprintf("</TD><TD>");
				wprintf("<input type=\"text\" NAME=\"c_imap_port\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<input type=\"hidden\" NAME=\"c_imap_port\" VALUE=\"%s\">", buf);
			}
			break;
		case 29:
			if (!strcasecmp(whichmenu, "network")) {
				wprintf("<TR><TD>");
				wprintf(_("Network run frequency (in seconds)"));
				wprintf("</TD><TD>");
				wprintf("<input type=\"text\" NAME=\"c_net_freq\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<input type=\"hidden\" NAME=\"c_net_freq\" VALUE=\"%s\">", buf);
			}
			break;
		case 30:
			if (!strcasecmp(whichmenu, "access")) {
				wprintf("<TR><TD>");
				wprintf(_("Disable self-service user account creation"));
				wprintf("</TD><TD>");
				wprintf("<input type=\"checkbox\" NAME=\"c_disable_newu\" VALUE=\"yes\" %s>",
					((atoi(buf) != 0) ? "CHECKED" : ""));
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<input type=\"hidden\" NAME=\"c_disable_newu\" VALUE=\"%s\">", buf);
			}
			break;
		case 31:
			/* position 31 is no longer in use */
			break;
		case 32:
			if (!strcasecmp(whichmenu, "purger")) {
				wprintf("<TR><TD>");
				wprintf(_("Hour to run database auto-purge"));
				wprintf("</TD><TD>");
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
				wprintf("<input type=\"hidden\" NAME=\"c_purge_hour\" VALUE=\"%s\">", buf);
			}
			break;
		case 33:
			if ( (serv_info.serv_supports_ldap) && (!strcasecmp(whichmenu, "ldap")) ) {
				wprintf("<TR><TD>");
				wprintf(_("Host name of LDAP server (blank to disable)"));
				wprintf("</TD><TD>");
				wprintf("<input type=\"text\" NAME=\"c_ldap_host\" MAXLENGTH=\"127\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<input type=\"hidden\" NAME=\"c_ldap_host\" VALUE=\"%s\">", buf);
			}
			break;
		case 34:
			if ( (serv_info.serv_supports_ldap) && (!strcasecmp(whichmenu, "ldap")) ) {
				wprintf("<TR><TD>");
				wprintf(_("Port number of LDAP server (blank to disable)"));
				wprintf("</TD><TD>");
				wprintf("<input type=\"text\" NAME=\"c_ldap_port\" MAXLENGTH=\"127\" VALUE=\"%d\">", atoi(buf));
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<input type=\"hidden\" NAME=\"c_ldap_port\" VALUE=\"%d\">", atoi(buf));
			}
			break;
		case 35:
			if ( (serv_info.serv_supports_ldap) && (!strcasecmp(whichmenu, "ldap")) ) {
				wprintf("<TR><TD>");
				wprintf(_("Base DN"));
				wprintf("</TD><TD>");
				wprintf("<input type=\"text\" NAME=\"c_ldap_base_dn\" MAXLENGTH=\"255\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<input type=\"hidden\" NAME=\"c_ldap_base_dn\" VALUE=\"%s\">", buf);
			}
			break;
		case 36:
			if ( (serv_info.serv_supports_ldap) && (!strcasecmp(whichmenu, "ldap")) ) {
				wprintf("<TR><TD>");
				wprintf(_("Bind DN"));
				wprintf("</TD><TD>");
				wprintf("<input type=\"text\" NAME=\"c_ldap_bind_dn\" MAXLENGTH=\"255\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<input type=\"hidden\" NAME=\"c_ldap_bind_dn\" VALUE=\"%s\">", buf);
			}
			break;
		case 37:
			if ( (serv_info.serv_supports_ldap) && (!strcasecmp(whichmenu, "ldap")) ) {
				wprintf("<TR><TD>");
				wprintf(_("Password for bind DN"));
				wprintf("</TD><TD>");
				wprintf("<input type=\"password\" NAME=\"c_ldap_bind_pw\" MAXLENGTH=\"255\" VALUE=\"%s\">",
					buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<input type=\"hidden\" NAME=\"c_ldap_bind_pw\" VALUE=\"%s\">", buf);
			}
			break;
		case 38:
			if (!strcasecmp(whichmenu, "network")) {
				wprintf("<TR><TD>");
				wprintf(_("Server IP address (0.0.0.0 for 'any')"));
				wprintf("</TD><TD>");
				wprintf("<input type=\"text\" NAME=\"c_ip_addr\" MAXLENGTH=\"15\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<input type=\"hidden\" NAME=\"c_ip_addr\" VALUE=\"%s\">", buf);
			}
			break;
		case 39:
			if (!strcasecmp(whichmenu, "network")) {
				wprintf("<TR><TD>");
				wprintf(_("SMTP MSA port (-1 to disable)"));
				wprintf("</TD><TD>");
				wprintf("<input type=\"text\" NAME=\"c_msa_port\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<input type=\"hidden\" NAME=\"c_msa_port\" VALUE=\"%s\">", buf);
			}
			break;
		case 40:
			if (!strcasecmp(whichmenu, "network")) {
				wprintf("<TR><TD>");
				wprintf(_("IMAP over SSL port (-1 to disable)"));
				wprintf("</TD><TD>");
				wprintf("<input type=\"text\" NAME=\"c_imaps_port\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<input type=\"hidden\" NAME=\"c_imaps_port\" VALUE=\"%s\">", buf);
			}
			break;
		case 41:
			if (!strcasecmp(whichmenu, "network")) {
				wprintf("<TR><TD>");
				wprintf(_("POP3 over SSL port (-1 to disable)"));
				wprintf("</TD><TD>");
				wprintf("<input type=\"text\" NAME=\"c_pop3s_port\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<input type=\"hidden\" NAME=\"c_pop3s_port\" VALUE=\"%s\">", buf);
			}
			break;
		case 42:
			if (!strcasecmp(whichmenu, "network")) {
				wprintf("<TR><TD>");
				wprintf(_("SMTP over SSL port (-1 to disable)"));
				wprintf("</TD><TD>");
				wprintf("<input type=\"text\" NAME=\"c_smtps_port\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<input type=\"hidden\" NAME=\"c_smtps_port\" VALUE=\"%s\">", buf);
			}
			break;
		case 43:
			if (!strcasecmp(whichmenu, "general")) {
				wprintf("<TR><TD>");
				wprintf(_("Enable full text index (warning: resource intensive)"));
				wprintf("</TD><TD>");
				wprintf("<input type=\"checkbox\" NAME=\"c_enable_fulltext\" VALUE=\"yes\" %s>",
					((atoi(buf) != 0) ? "CHECKED" : ""));
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<input type=\"hidden\" NAME=\"c_enable_fulltext\" VALUE=\"%s\">", buf);
			}
			break;
		case 44:
			if (!strcasecmp(whichmenu, "tuning")) {
				wprintf("<TR><TD>");
				wprintf(_("Automatically delete committed database logs"));
				wprintf("</TD><TD>");
				wprintf("<input type=\"checkbox\" NAME=\"c_auto_cull\" VALUE=\"yes\" %s>",
					((atoi(buf) != 0) ? "CHECKED" : ""));
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<input type=\"hidden\" NAME=\"c_auto_cull\" VALUE=\"%s\">", buf);
			}
			break;
		case 45:
			if (!strcasecmp(whichmenu, "network")) {
				wprintf("<TR><TD>");
				wprintf(_("Instantly expunge deleted messages in IMAP"));
				wprintf("</TD><TD>");
				wprintf("<input type=\"checkbox\" NAME=\"c_instant_expunge\" VALUE=\"yes\" %s>",
					((atoi(buf) != 0) ? "CHECKED" : ""));
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<input type=\"hidden\" NAME=\"c_instant_expunge\" VALUE=\"%s\">", buf);
			}
			break;
		case 46:
			if (!strcasecmp(whichmenu, "network")) {
				wprintf("<TR><TD>");
				wprintf(_("Allow unauthenticated SMTP clients to spoof this site's domains"));
				wprintf("</TD><TD>");
				wprintf("<input type=\"checkbox\" NAME=\"c_allow_spoofing\" VALUE=\"yes\" %s>",
					((atoi(buf) != 0) ? "CHECKED" : ""));
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<input type=\"hidden\" NAME=\"c_allow_spoofing\" VALUE=\"%s\">", buf);
			}
			break;
		}
	}

	serv_puts("GPEX site");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '2') {
		sitepolicy = extract_int(&buf[4], 0);
		sitevalue = extract_int(&buf[4], 1);
	}

	serv_puts("GPEX mailboxes");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '2') {
		mboxpolicy = extract_int(&buf[4], 0);
		mboxvalue = extract_int(&buf[4], 1);
	}

	if (!strcasecmp(whichmenu, "purger")) {

		wprintf("<TR><TD COLSPAN=2><hr /></TD></TR>\n");

		wprintf("<TR><TD>");
		wprintf(_("Default message expire policy for public rooms"));
		wprintf("</TD><TD>");
		wprintf("<input type=\"radio\" NAME=\"sitepolicy\" VALUE=\"1\" %s>",
			((sitepolicy == 1) ? "CHECKED" : "") );
		wprintf(_("Never automatically expire messages"));
		wprintf("<br />\n");
		wprintf("<input type=\"radio\" NAME=\"sitepolicy\" VALUE=\"2\" %s>",
			((sitepolicy == 2) ? "CHECKED" : "") );
		wprintf(_("Expire by message count"));
		wprintf("<br />\n");
		wprintf("<input type=\"radio\" NAME=\"sitepolicy\" VALUE=\"3\" %s>",
			((sitepolicy == 3) ? "CHECKED" : "") );
		wprintf(_("Expire by message age"));
		wprintf("<br />");
		wprintf(_("Number of messages or days: "));
		wprintf("<input type=\"text\" NAME=\"sitevalue\" MAXLENGTH=\"5\" VALUE=\"%d\">", sitevalue);
		wprintf("</TD></TR>\n");

		wprintf("<TR><TD COLSPAN=2><hr /></TD></TR>\n");

		wprintf("<TR><TD>");
		wprintf(_("Default message expire policy for private mailboxes"));
		wprintf("</TD><TD>");
		wprintf("<input type=\"radio\" NAME=\"mboxpolicy\" VALUE=\"0\" %s>",
			((mboxpolicy == 0) ? "CHECKED" : "") );
		wprintf(_("Same policy as public rooms"));
		wprintf("<br />\n");
		wprintf("<input type=\"radio\" NAME=\"mboxpolicy\" VALUE=\"1\" %s>",
			((mboxpolicy == 1) ? "CHECKED" : "") );
		wprintf(_("Never automatically expire messages"));
		wprintf("<br />\n");
		wprintf("<input type=\"radio\" NAME=\"mboxpolicy\" VALUE=\"2\" %s>",
			((mboxpolicy == 2) ? "CHECKED" : "") );
		wprintf(_("Expire by message count"));
		wprintf("<br />\n");
		wprintf("<input type=\"radio\" NAME=\"mboxpolicy\" VALUE=\"3\" %s>",
			((mboxpolicy == 3) ? "CHECKED" : "") );
		wprintf(_("Expire by message age"));
		wprintf("<br />");
		wprintf(_("Number of messages or days: "));
		wprintf("<input type=\"text\" NAME=\"mboxvalue\" MAXLENGTH=\"5\" VALUE=\"%d\">", mboxvalue);
		wprintf("</TD></TR>\n");

		wprintf("<TR><TD COLSPAN=2><hr /></TD></TR>\n");

	}
	else {
		wprintf("<input type=\"hidden\" NAME=\"sitepolicy\" VALUE=\"%d\">\n", sitepolicy);
		wprintf("<input type=\"hidden\" NAME=\"sitevalue\" VALUE=\"%d\">\n", sitevalue);
		wprintf("<input type=\"hidden\" NAME=\"mboxpolicy\" VALUE=\"%d\">\n", mboxpolicy);
		wprintf("<input type=\"hidden\" NAME=\"mboxvalue\" VALUE=\"%d\">\n", mboxvalue);
	}

	wprintf("</TABLE><div align=\"center\">");
	wprintf("<input type=\"submit\" NAME=\"ok_button\" VALUE=\"%s\">", _("Save changes"));
	wprintf("&nbsp;");
	wprintf("<input type=\"submit\" NAME=\"cancel_button\" VALUE=\"%s\">\n", _("Cancel"));
	wprintf("</div></FORM>\n");
	wprintf("</td></tr></table></div>\n");
	wDumpContent(1);
}


void siteconfig(void)
{
	char buf[256];

	if (strlen(bstr("ok_button")) == 0) {
		display_siteconfig();
		return;
	}
	serv_printf("CONF set");
	serv_getln(buf, sizeof buf);
	if (buf[0] != '4') {
		safestrncpy(WC->ImportantMessage, &buf[4], sizeof WC->ImportantMessage);
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
	serv_printf("1");
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
	serv_printf("%s", ((!strcasecmp(bstr("c_enable_fulltext"), "yes") ? "1" : "0")));
	serv_printf("%s", ((!strcasecmp(bstr("c_auto_cull"), "yes") ? "1" : "0")));
	serv_printf("%s", ((!strcasecmp(bstr("c_instant_expunge"), "yes") ? "1" : "0")));
	serv_printf("%s", ((!strcasecmp(bstr("c_allow_spoofing"), "yes") ? "1" : "0")));
	serv_printf("000");

	serv_printf("SPEX site|%d|%d", atoi(bstr("sitepolicy")), atoi(bstr("sitevalue")));
	serv_getln(buf, sizeof buf);
	serv_printf("SPEX mailboxes|%d|%d", atoi(bstr("mboxpolicy")), atoi(bstr("mboxvalue")));
	serv_getln(buf, sizeof buf);

	safestrncpy(WC->ImportantMessage, _("Your system configuration has been updated."),
		sizeof WC->ImportantMessage);
	display_siteconfig();
}
