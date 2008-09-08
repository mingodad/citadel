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
	char this_zone[128];
	char *ZName;
	
	ZoneHash = NewHash(1, NULL);
	len = sizeof("UTC") + 1;
	ZName = malloc(len + 1);
	memcpy(ZName, "UTC", len + 1);
	Put(ZoneHash, ZName, len, ZName, NULL);
	zones = icaltimezone_get_builtin_timezones();
	for (z = 0; z < zones->num_elements; ++z) {
		strcpy(this_zone, icaltimezone_get_location(icalarray_element_at(zones, z)));
		len = strlen(this_zone);
		ZName = (char*)malloc(len +1);
		memcpy(ZName, this_zone, len + 1);
		Put(ZoneHash, ZName, len, ZName, NULL);
	}
	SortByHashKey(ZoneHash, 0);
}




/*
 * \brief display all configuration items
 */
void display_siteconfig(void)
{
	char buf[SIZ];
	int i, j;
	struct wcsession *WCC = WC;
	const char *VCZname;

	char general[65536];
	char access[SIZ];
	char network[SIZ];
	char tuning[SIZ];
	char directory[SIZ];
	char purger[SIZ];
	char idxjnl[SIZ];
	char funambol[SIZ];
	char pop3[SIZ];
	
	/** expire policy settings */
	int sitepolicy = 0;
	int sitevalue = 0;
	int mboxpolicy = 0;
	int mboxvalue = 0;

	output_headers(1, 1, 2, 0, 0, 0);
	wprintf("<div id=\"banner\">\n");
	wprintf("<h1>");
	wprintf(_("Site configuration"));
	wprintf("</h1>");
	wprintf("</div>\n");

	wprintf("<div id=\"content\" class=\"service fix_scrollbar_bug\">\n");

	serv_printf("CONF get");
	serv_getln(buf, sizeof buf);
	if (buf[0] != '1') {
        	wprintf("<div class=\"errormsg\">");
		wprintf(_("Error"));
        	wprintf("<br />\n");
        	wprintf("%s<br />\n", &buf[4]);
		wDumpContent(1);
		wprintf("</div>\n");
		return;
	}

	wprintf("<table border=\"0\" cellspacing=\"0\" cellpadding=\"0\" ><tr><td>");

	char *tabnames[] = {
		_("General"),
		_("Access"),
		_("Network"),
		_("Tuning"),
		_("Directory"),
		_("Auto-purger"),
		_("Indexing/Journaling"),
		_("Push Email"),
		_("Pop3")
	};

	sprintf(general, "<center><h1>%s</h1><table border=\"0\">",
			_("General site configuration items")
	);

	sprintf(access, "<center><h1>%s</h1><table border=\"0\">",
			_("Access controls and site policy settings")
	);

	sprintf(network, "<center><h1>%s</h1><h2>%s</h2><table border=\"0\">",
			_("Network services"),
			_("Changes made on this screen will not take effect "
			"until you restart the Citadel server.")
	);

	sprintf(tuning, "<center><h1>%s</h1><table border=\"0\">",
			_("Advanced server fine-tuning controls")
	);

	sprintf(directory, "<center><h1>%s</h1><h2>%s</h2><table border=\"0\">",
			_("Configure the LDAP connector for Citadel"),
			(serv_info.serv_supports_ldap
			?	_("Changes made on this screen will not take effect "
				"until you restart the Citadel server.")
			:	_("NOTE: This Citadel server has been built without "
				"LDAP support.  These options will have no effect.")
			)
	);

	sprintf(purger, "<center><h1>%s</h1><h2>%s</h2><table border=\"0\">",
			_("Configure automatic expiry of old messages"),
			_("These settings may be overridden on a per-floor or per-room basis.")
	);

	sprintf(idxjnl, "<center><h1>%s</h1><h2>%s</h2><table border=\"0\">",
			_("Indexing and Journaling"),
			_("Warning: these facilities are resource intensive.")
	);
	sprintf(funambol, "<center><h1>%s</h1><table border=\"0\">",
		_("Push Email")
		);

	sprintf(pop3, "<center><h1>%s</h1><table border=\"0\">",
		_("POP3")
		);
		
	wprintf("<form method=\"post\" action=\"siteconfig\">\n");
	wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WCC->nonce);
	
	sprintf(&general[strlen(general)], "<tr><td><a href=\"display_edithello\"> %s </a></td>",           _("Change Login Logo"));
	sprintf(&general[strlen(general)],     "<td><a href=\"display_editgoodbuye\"> %s </a></td></tr>\n", _("Change Logout Logo"));

	i = 0;
	while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		switch (i++) {
		case 0:
			sprintf(&general[strlen(general)], "<tr><td>");
			sprintf(&general[strlen(general)], _("Node name"));
			sprintf(&general[strlen(general)], "</td><td>");
			sprintf(&general[strlen(general)], "<input type=\"text\" name=\"c_nodename\" maxlength=\"15\" value=\"%s\">", buf);
			sprintf(&general[strlen(general)], "</td></tr>\n");
			break;
		case 1:
			sprintf(&general[strlen(general)], "<tr><td>");
			sprintf(&general[strlen(general)], _("Fully qualified domain name"));
			sprintf(&general[strlen(general)], "</td><td>");
			sprintf(&general[strlen(general)], "<input type=\"text\" name=\"c_fqdn\" maxlength=\"63\" value=\"%s\">", buf);
			sprintf(&general[strlen(general)], "</td></tr>\n");
			break;
		case 2:
			sprintf(&general[strlen(general)], "<tr><td>");
			sprintf(&general[strlen(general)], _("Human-readable node name"));
			sprintf(&general[strlen(general)], "</td><td>");
			sprintf(&general[strlen(general)], "<input type=\"text\" name=\"c_humannode\" maxlength=\"20\" value=\"%s\">", buf);
			sprintf(&general[strlen(general)], "</td></tr>\n");
			break;
		case 3:
			sprintf(&general[strlen(general)], "<tr><td>");
			sprintf(&general[strlen(general)], _("Telephone number"));
			sprintf(&general[strlen(general)], "</td><td>");
			sprintf(&general[strlen(general)], "<input type=\"text\" name=\"c_phonenum\" maxlength=\"15\" value=\"%s\">", buf);
			sprintf(&general[strlen(general)], "</td></tr>\n");
			break;
		case 4:
			sprintf(&access[strlen(access)], "<tr><td>");
			sprintf(&access[strlen(access)], _("Automatically grant room-aide status to users who create private rooms"));
			sprintf(&access[strlen(access)], "</td><td>");
			sprintf(&access[strlen(access)], "<input type=\"checkbox\" name=\"c_creataide\" value=\"yes\" %s>",
				((atoi(buf) != 0) ? "checked" : ""));
			sprintf(&access[strlen(access)], "</td></tr>\n");
			break;
		case 5:
			sprintf(&tuning[strlen(tuning)], "<tr><td>");
			sprintf(&tuning[strlen(tuning)], _("Server connection idle timeout (in seconds)"));
			sprintf(&tuning[strlen(tuning)], "</td><td>");
			sprintf(&tuning[strlen(tuning)], "<input type=\"text\" name=\"c_sleeping\" maxlength=\"15\" value=\"%s\">", buf);
			sprintf(&tuning[strlen(tuning)], "</td></tr>\n");
			break;
		case 6:
			sprintf(&access[strlen(access)], "<tr><td>");
			sprintf(&access[strlen(access)], _("Initial access level for new users"));
			sprintf(&access[strlen(access)], "</td><td>");
			sprintf(&access[strlen(access)], "<select name=\"c_initax\" size=\"1\">\n");
			for (j=0; j<=6; ++j) {
				sprintf(&access[strlen(access)], "<option %s value=\"%d\">%d - %s</option>\n",
					((atoi(buf) == j) ? "selected" : ""),
					j, j, axdefs[j]
				);
			}
			sprintf(&access[strlen(access)], "</select>");
			sprintf(&access[strlen(access)], "</td></tr>\n");
			break;
		case 7:
			sprintf(&access[strlen(access)], "<tr><td>");
			sprintf(&access[strlen(access)], _("Require registration for new users"));
			sprintf(&access[strlen(access)], "</td><td>");
			sprintf(&access[strlen(access)], "<input type=\"checkbox\" name=\"c_regiscall\" value=\"yes\" %s>",
				((atoi(buf) != 0) ? "checked" : ""));
			sprintf(&access[strlen(access)], "</td></tr>\n");
			break;
		case 8:
			sprintf(&access[strlen(access)], "<tr><td>");
			sprintf(&access[strlen(access)], _("Quarantine messages from problem users"));
			sprintf(&access[strlen(access)], "</td><td>");
			sprintf(&access[strlen(access)], "<input type=\"checkbox\" name=\"c_twitdetect\" value=\"yes\" %s>",
				((atoi(buf) != 0) ? "checked" : ""));
			sprintf(&access[strlen(access)], "</td></tr>\n");
			break;
		case 9:
			sprintf(&access[strlen(access)], "<tr><td>");
			sprintf(&access[strlen(access)], _("Name of quarantine room"));
			sprintf(&access[strlen(access)], "</td><td>");
			sprintf(&access[strlen(access)], "<input type=\"text\" name=\"c_twitroom\" maxlength=\"63\" value=\"%s\">", buf);
			sprintf(&access[strlen(access)], "</td></tr>\n");
			break;
		case 10:
			sprintf(&general[strlen(general)], "<tr><td>");
			sprintf(&general[strlen(general)], _("Paginator prompt (for text mode clients)"));
			sprintf(&general[strlen(general)], "</td><td>");
			sprintf(&general[strlen(general)], "<input type=\"text\" name=\"c_moreprompt\" maxlength=\"79\" value=\"%s\">", buf);
			sprintf(&general[strlen(general)], "</td></tr>\n");
			break;
		case 11:
			sprintf(&access[strlen(access)], "<tr><td>");
			sprintf(&access[strlen(access)], _("Restrict access to Internet mail"));
			sprintf(&access[strlen(access)], "</td><td>");
			sprintf(&access[strlen(access)], "<input type=\"checkbox\" name=\"c_restrict\" value=\"yes\" %s>",
				((atoi(buf) != 0) ? "checked" : ""));
			sprintf(&access[strlen(access)], "</td></tr>\n");
			break;
		case 12:
			sprintf(&general[strlen(general)], "<tr><td>");
			sprintf(&general[strlen(general)], _("Geographic location of this system"));
			sprintf(&general[strlen(general)], "</td><td>");
			sprintf(&general[strlen(general)], "<input type=\"text\" name=\"c_bbs_city\" maxlength=\"31\" value=\"%s\">", buf);
			sprintf(&general[strlen(general)], "</td></tr>\n");
			break;
		case 13:
			sprintf(&general[strlen(general)], "<tr><td>");
			sprintf(&general[strlen(general)], _("Name of system administrator"));
			sprintf(&general[strlen(general)], "</td><td>");
			sprintf(&general[strlen(general)], "<input type=\"text\" name=\"c_sysadm\" MAXLENGTH=\"25\" VALUE=\"%s\">", buf);
			sprintf(&general[strlen(general)], "</td></tr>\n");
			break;
		case 14:
			sprintf(&tuning[strlen(tuning)], "<tr><td>");
			sprintf(&tuning[strlen(tuning)], _("Maximum concurrent sessions (0 = no limit)"));
			sprintf(&tuning[strlen(tuning)], "</td><td>");
			sprintf(&tuning[strlen(tuning)], "<input type=\"text\" name=\"c_maxsessions\" maxlength=\"5\" value=\"%s\">", buf);
			sprintf(&tuning[strlen(tuning)], "</td></tr>\n");
			break;
		case 16:
			sprintf(&tuning[strlen(tuning)], "<tr><td>");
			sprintf(&tuning[strlen(tuning)], _("Default user purge time (days)"));
			sprintf(&tuning[strlen(tuning)], "</td><td>");
			sprintf(&tuning[strlen(tuning)], "<input type=\"text\" name=\"c_userpurge\" maxlength=\"5\" value=\"%s\">", buf);
			sprintf(&tuning[strlen(tuning)], "</td></tr>\n");
			break;
		case 17:
			sprintf(&tuning[strlen(tuning)], "<tr><td>");
			sprintf(&tuning[strlen(tuning)], _("Default room purge time (days)"));
			sprintf(&tuning[strlen(tuning)], "</td><td>");
			sprintf(&tuning[strlen(tuning)], "<input type=\"text\" name=\"c_roompurge\" maxlength=\"5\" value=\"%s\">", buf);
			sprintf(&tuning[strlen(tuning)], "</td></tr>\n");
			break;
		case 18:
			sprintf(&access[strlen(access)], "<tr><td>");
			sprintf(&access[strlen(access)], _("Name of room to log pages"));
			sprintf(&access[strlen(access)], "</td><td>");
			sprintf(&access[strlen(access)], "<input type=\"text\" name=\"c_logpages\" maxlength=\"63\" value=\"%s\">", buf);
			sprintf(&access[strlen(access)], "</td></tr>\n");
			break;
		case 19:
			sprintf(&access[strlen(access)], "<tr><td>");
			sprintf(&access[strlen(access)], _("Access level required to create rooms"));
			sprintf(&access[strlen(access)], "</td><td>");
			sprintf(&access[strlen(access)], "<select name=\"c_createax\" size=\"1\">\n");
			for (j=0; j<=6; ++j) {
				sprintf(&access[strlen(access)], "<option %s value=\"%d\">%d - %s</option>\n",
					((atoi(buf) == j) ? "selected" : ""),
					j, j, axdefs[j]
				);
			}
			sprintf(&access[strlen(access)], "</select>");
			sprintf(&access[strlen(access)], "</td></tr>\n");
			break;
		case 20:
			sprintf(&tuning[strlen(tuning)], "<tr><td>");
			sprintf(&tuning[strlen(tuning)], _("Maximum message length"));
			sprintf(&tuning[strlen(tuning)], "</td><td>");
			sprintf(&tuning[strlen(tuning)], "<input type=\"text\" name=\"c_maxmsglen\" maxlength=\"20\" value=\"%s\">", buf);
			sprintf(&tuning[strlen(tuning)], "</td></tr>\n");
			break;
		case 21:
			sprintf(&tuning[strlen(tuning)], "<tr><td>");
			sprintf(&tuning[strlen(tuning)], _("Minimum number of worker threads"));
			sprintf(&tuning[strlen(tuning)], "</td><td>");
			sprintf(&tuning[strlen(tuning)], "<input type=\"text\" name=\"c_min_workers\" maxlength=\"5\" value=\"%s\">", buf);
			sprintf(&tuning[strlen(tuning)], "</td></tr>\n");
			break;
		case 22:
			sprintf(&tuning[strlen(tuning)], "<tr><td>");
			sprintf(&tuning[strlen(tuning)], _("Maximum number of worker threads"));
			sprintf(&tuning[strlen(tuning)], "</td><td>");
			sprintf(&tuning[strlen(tuning)], "<input type=\"text\" name=\"c_max_workers\" maxlength=\"5\" value=\"%s\">", buf);
			sprintf(&tuning[strlen(tuning)], "</td></tr>\n");
			break;
		case 23:
			sprintf(&pop3[strlen(pop3)], "<tr><td>");
			sprintf(&pop3[strlen(pop3)], _("POP3 listener port (-1 to disable)"));
			sprintf(&pop3[strlen(pop3)], "</td><td>");
			sprintf(&pop3[strlen(pop3)], "<input type=\"text\" name=\"c_pop3_port\" maxlength=\"5\" value=\"%s\">", buf);
			sprintf(&pop3[strlen(pop3)], "</TD></TR>\n");
			break;
		case 24:
			sprintf(&network[strlen(network)], "<TR><TD>");
			sprintf(&network[strlen(network)], _("SMTP MTA port (-1 to disable)"));
			sprintf(&network[strlen(network)], "</TD><TD>");
			sprintf(&network[strlen(network)], "<input type=\"text\" NAME=\"c_smtp_port\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
			sprintf(&network[strlen(network)], "</TD></TR>\n");
			break;
		case 25:	/* note: reverse bool */
			sprintf(&network[strlen(network)], "<TR><TD>");
			sprintf(&network[strlen(network)], _("Correct forged From: lines during authenticated SMTP"));
			sprintf(&network[strlen(network)], "</TD><TD>");
			sprintf(&network[strlen(network)], "<input type=\"checkbox\" NAME=\"c_rfc822_strict_from\" VALUE=\"yes\" %s>",
				((atoi(buf) == 0) ? "CHECKED" : ""));
			sprintf(&network[strlen(network)], "</TD></TR>\n");
			break;
		case 26:
			sprintf(&access[strlen(access)], "<TR><TD>");
			sprintf(&access[strlen(access)], _("Allow aides to zap (forget) rooms"));
			sprintf(&access[strlen(access)], "</TD><TD>");
			sprintf(&access[strlen(access)], "<input type=\"checkbox\" NAME=\"c_aide_zap\" VALUE=\"yes\" %s>",
				((atoi(buf) != 0) ? "CHECKED" : ""));
			sprintf(&access[strlen(access)], "</TD></TR>\n");
			break;
		case 27:
			sprintf(&network[strlen(network)], "<TR><TD>");
			sprintf(&network[strlen(network)], _("IMAP listener port (-1 to disable)"));
			sprintf(&network[strlen(network)], "</TD><TD>");
			sprintf(&network[strlen(network)], "<input type=\"text\" NAME=\"c_imap_port\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
			sprintf(&network[strlen(network)], "</TD></TR>\n");
			break;
		case 28:
			sprintf(&network[strlen(network)], "<TR><TD>");
			sprintf(&network[strlen(network)], _("Network run frequency (in seconds)"));
			sprintf(&network[strlen(network)], "</TD><TD>");
			sprintf(&network[strlen(network)], "<input type=\"text\" NAME=\"c_net_freq\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
			sprintf(&network[strlen(network)], "</TD></TR>\n");
			break;
		case 29:
			sprintf(&access[strlen(access)], "<TR><TD>");
			sprintf(&access[strlen(access)], _("Disable self-service user account creation"));
			sprintf(&access[strlen(access)], "</TD><TD>");
			sprintf(&access[strlen(access)], "<input type=\"checkbox\" NAME=\"c_disable_newu\" VALUE=\"yes\" %s>",
				((atoi(buf) != 0) ? "CHECKED" : ""));
			sprintf(&access[strlen(access)], "</TD></TR>\n");
			break;
		case 31:
			sprintf(&purger[strlen(purger)], "<TR><TD>");
			sprintf(&purger[strlen(purger)], _("Hour to run database auto-purge"));
			sprintf(&purger[strlen(purger)], "</TD><TD>");
			sprintf(&purger[strlen(purger)], "<SELECT NAME=\"c_purge_hour\" SIZE=\"1\">\n");
			for (j=0; j<=23; ++j) {
				sprintf(&purger[strlen(purger)], "<OPTION %s VALUE=\"%d\">%d:00%s</OPTION>\n",
					((atoi(buf) == j) ? "SELECTED" : ""),
					j,
					(get_time_format_cached() == WC_TIMEFORMAT_24) ? j : ((j == 0) ? 12 : ((j>12) ? j-12 : j)),
					(get_time_format_cached() == WC_TIMEFORMAT_24) ? "" : ((j >= 12) ? "pm" : "am")
				);
			}
			sprintf(&purger[strlen(purger)], "</SELECT>");
			sprintf(&purger[strlen(purger)], "</TD></TR>\n");
			break;
		case 32:
			sprintf(&directory[strlen(directory)], "<TR><TD>");
			sprintf(&directory[strlen(directory)], _("Host name of LDAP server (blank to disable)"));
			sprintf(&directory[strlen(directory)], "</TD><TD>");
			sprintf(&directory[strlen(directory)], "<input type=\"text\" NAME=\"c_ldap_host\" MAXLENGTH=\"127\" VALUE=\"%s\">", buf);
			sprintf(&directory[strlen(directory)], "</TD></TR>\n");
			break;
		case 33:
			sprintf(&directory[strlen(directory)], "<TR><TD>");
			sprintf(&directory[strlen(directory)], _("Port number of LDAP server (blank to disable)"));
			sprintf(&directory[strlen(directory)], "</TD><TD>");
			sprintf(&directory[strlen(directory)], "<input type=\"text\" NAME=\"c_ldap_port\" MAXLENGTH=\"127\" VALUE=\"%d\">", atoi(buf));
			sprintf(&directory[strlen(directory)], "</TD></TR>\n");
			break;
		case 34:
			sprintf(&directory[strlen(directory)], "<TR><TD>");
			sprintf(&directory[strlen(directory)], _("Base DN"));
			sprintf(&directory[strlen(directory)], "</TD><TD>");
			sprintf(&directory[strlen(directory)], "<input type=\"text\" NAME=\"c_ldap_base_dn\" MAXLENGTH=\"255\" VALUE=\"%s\">", buf);
			sprintf(&directory[strlen(directory)], "</TD></TR>\n");
			break;
		case 35:
			sprintf(&directory[strlen(directory)], "<TR><TD>");
			sprintf(&directory[strlen(directory)], _("Bind DN"));
			sprintf(&directory[strlen(directory)], "</TD><TD>");
			sprintf(&directory[strlen(directory)], "<input type=\"text\" NAME=\"c_ldap_bind_dn\" MAXLENGTH=\"255\" VALUE=\"%s\">", buf);
			sprintf(&directory[strlen(directory)], "</TD></TR>\n");
			break;
		case 36:
			sprintf(&directory[strlen(directory)], "<TR><TD>");
			sprintf(&directory[strlen(directory)], _("Password for bind DN"));
			sprintf(&directory[strlen(directory)], "</TD><TD>");
			sprintf(&directory[strlen(directory)], "<input type=\"password\" NAME=\"c_ldap_bind_pw\" MAXLENGTH=\"255\" VALUE=\"%s\">",
				buf);
			sprintf(&directory[strlen(directory)], "</TD></TR>\n");
			break;
		case 37:
			sprintf(&network[strlen(network)], "<TR><TD>");
			sprintf(&network[strlen(network)], _("Server IP address (0.0.0.0 for 'any')"));
			sprintf(&network[strlen(network)], "</TD><TD>");
			sprintf(&network[strlen(network)], "<input type=\"text\" NAME=\"c_ip_addr\" MAXLENGTH=\"15\" VALUE=\"%s\">", buf);
			sprintf(&network[strlen(network)], "</TD></TR>\n");
			break;
		case 38:
			sprintf(&network[strlen(network)], "<TR><TD>");
			sprintf(&network[strlen(network)], _("SMTP MSA port (-1 to disable)"));
			sprintf(&network[strlen(network)], "</TD><TD>");
			sprintf(&network[strlen(network)], "<input type=\"text\" NAME=\"c_msa_port\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
			sprintf(&network[strlen(network)], "</TD></TR>\n");
			break;
		case 39:
			sprintf(&network[strlen(network)], "<TR><TD>");
			sprintf(&network[strlen(network)], _("IMAP over SSL port (-1 to disable)"));
			sprintf(&network[strlen(network)], "</TD><TD>");
			sprintf(&network[strlen(network)], "<input type=\"text\" NAME=\"c_imaps_port\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
			sprintf(&network[strlen(network)], "</TD></TR>\n");
			break;
		case 40:
			sprintf(&pop3[strlen(pop3)], "<TR><TD>");
			sprintf(&pop3[strlen(pop3)], _("POP3 over SSL port (-1 to disable)"));
			sprintf(&pop3[strlen(pop3)], "</TD><TD>");
			sprintf(&pop3[strlen(pop3)], "<input type=\"text\" NAME=\"c_pop3s_port\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
			sprintf(&pop3[strlen(pop3)], "</TD></TR>\n");
			break;
		case 41:
			sprintf(&network[strlen(network)], "<TR><TD>");
			sprintf(&network[strlen(network)], _("SMTP over SSL port (-1 to disable)"));
			sprintf(&network[strlen(network)], "</TD><TD>");
			sprintf(&network[strlen(network)], "<input type=\"text\" NAME=\"c_smtps_port\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
			sprintf(&network[strlen(network)], "</TD></TR>\n");
			break;
		case 42:
				sprintf(&idxjnl[strlen(idxjnl)], "<TR><TD>");
				sprintf(&idxjnl[strlen(idxjnl)], _("Enable full text index"));
				sprintf(&idxjnl[strlen(idxjnl)], "</TD><TD>");
				sprintf(&idxjnl[strlen(idxjnl)], "<input type=\"checkbox\" NAME=\"c_enable_fulltext\" VALUE=\"yes\" %s>",
					((atoi(buf) != 0) ? "CHECKED" : ""));
				sprintf(&idxjnl[strlen(idxjnl)], "</TD></TR>\n");
			break;
		case 43:
			sprintf(&tuning[strlen(tuning)], "<TR><TD>");
			sprintf(&tuning[strlen(tuning)], _("Automatically delete committed database logs"));
			sprintf(&tuning[strlen(tuning)], "</TD><TD>");
			sprintf(&tuning[strlen(tuning)], "<input type=\"checkbox\" NAME=\"c_auto_cull\" VALUE=\"yes\" %s>",
				((atoi(buf) != 0) ? "CHECKED" : ""));
			sprintf(&tuning[strlen(tuning)], "</TD></TR>\n");
			break;
		case 44:
			sprintf(&network[strlen(network)], "<TR><TD>");
			sprintf(&network[strlen(network)], _("Instantly expunge deleted messages in IMAP"));
			sprintf(&network[strlen(network)], "</TD><TD>");
			sprintf(&network[strlen(network)], "<input type=\"checkbox\" NAME=\"c_instant_expunge\" VALUE=\"yes\" %s>",
				((atoi(buf) != 0) ? "CHECKED" : ""));
			sprintf(&network[strlen(network)], "</TD></TR>\n");
			break;
		case 45:
			sprintf(&network[strlen(network)], "<TR><TD>");
			sprintf(&network[strlen(network)], _("Allow unauthenticated SMTP clients to spoof this site's domains"));
			sprintf(&network[strlen(network)], "</TD><TD>");
			sprintf(&network[strlen(network)], "<input type=\"checkbox\" NAME=\"c_allow_spoofing\" VALUE=\"yes\" %s>",
				((atoi(buf) != 0) ? "CHECKED" : ""));
			sprintf(&network[strlen(network)], "</TD></TR>\n");
			break;
		case 46:
			sprintf(&idxjnl[strlen(idxjnl)], "<TR><TD>");
			sprintf(&idxjnl[strlen(idxjnl)], _("Perform journaling of email messages"));
			sprintf(&idxjnl[strlen(idxjnl)], "</TD><TD>");
			sprintf(&idxjnl[strlen(idxjnl)], "<input type=\"checkbox\" NAME=\"c_journal_email\" VALUE=\"yes\" %s>",
				((atoi(buf) != 0) ? "CHECKED" : ""));
			sprintf(&idxjnl[strlen(idxjnl)], "</TD></TR>\n");
			break;
		case 47:
			sprintf(&idxjnl[strlen(idxjnl)], "<TR><TD>");
			sprintf(&idxjnl[strlen(idxjnl)], _("Perform journaling of non-email messages"));
			sprintf(&idxjnl[strlen(idxjnl)], "</TD><TD>");
			sprintf(&idxjnl[strlen(idxjnl)], "<input type=\"checkbox\" NAME=\"c_journal_pubmsgs\" VALUE=\"yes\" %s>",
				((atoi(buf) != 0) ? "CHECKED" : ""));
			sprintf(&idxjnl[strlen(idxjnl)], "</TD></TR>\n");
			break;
		case 48:
			sprintf(&idxjnl[strlen(idxjnl)], "<TR><TD>");
			sprintf(&idxjnl[strlen(idxjnl)], _("Email destination of journalized messages"));
			sprintf(&idxjnl[strlen(idxjnl)], "</TD><TD>");
			sprintf(&idxjnl[strlen(idxjnl)], "<input type=\"text\" NAME=\"c_journal_dest\" MAXLENGTH=\"127\" VALUE=\"%s\">", buf);
			sprintf(&idxjnl[strlen(idxjnl)], "</TD></TR>\n");
			break;
		case 49:
			if (strlen(buf) == 0) {
				strcpy(buf, "UTC");
			}
			sprintf(&general[strlen(general)], "<TR><TD>");
			sprintf(&general[strlen(general)], _("Default timezone for unzoned calendar items"));
			sprintf(&general[strlen(general)], "</TD><TD>");
			sprintf(&general[strlen(general)], "<select name=\"c_default_cal_zone\" size=\"1\">\n");

			icalarray *zones;
			int z;
			long len;
			char this_zone[128];
			char *ZName;
			void *ZNamee;
			HashList *List;
			HashPos  *it;

			List = NewHash(1, NULL);
			len = sizeof("UTC") + 1;
			ZName = malloc(len + 1);
			memcpy(ZName, "UTC", len + 1);
			Put(List, ZName, len, ZName, NULL);
			zones = icaltimezone_get_builtin_timezones();
			for (z = 0; z < zones->num_elements; ++z) {
				strcpy(this_zone, icaltimezone_get_location(icalarray_element_at(zones, z)));
				len = strlen(this_zone);
				ZName = (char*)malloc(len +1);
				memcpy(ZName, this_zone, len + 1);
				Put(List, ZName, len, ZName, NULL);
			}
			SortByHashKey(List, 0);
			it = GetNewHashPos();
			while (GetNextHashPos(List, it, &len, &VCZname, &ZNamee)) {
				sprintf(&general[strlen(general)], "<option %s value=\"%s\">%s</option>\n",
					(!strcasecmp((char*)ZName, buf) ? "selected" : ""),
					ZName, ZName
				);
			}
			DeleteHashPos(&it);
			DeleteHash(&List);

			sprintf(&general[strlen(general)], "</select>");
			sprintf(&general[strlen(general)], "</TD></TR>\n");
			break;
		case 50:
			sprintf(&network[strlen(network)], "<TR><TD>");
			sprintf(&network[strlen(network)], _("<a href=\"http://www.postfix.org/tcp_table.5.html\">Postfix TCP Dictionary Port </a> (-1 to disable)"));
			sprintf(&network[strlen(network)], "</TD><TD>");
			sprintf(&network[strlen(network)], "<input type=\"text\" NAME=\"c_pftcpdict_port\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
			sprintf(&network[strlen(network)], "</TD></TR>\n");
			break;
		case 51:
			sprintf(&network[strlen(network)], "<TR><TD>");
			sprintf(&network[strlen(network)], _("ManageSieve Port (-1 to disable)"));
			sprintf(&network[strlen(network)], "</TD><TD>");
			sprintf(&network[strlen(network)], "<input type=\"text\" NAME=\"c_mgesve_port\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
			sprintf(&network[strlen(network)], "</TD></TR>\n");
			break;
		case 52:
			sprintf(&access[strlen(access)], "<TR><TD>");
			sprintf(&access[strlen(access)], _("Enable host based authentication mode"));
			sprintf(&access[strlen(access)], "</TD><TD><input type=\"hidden\" NAME=\"c_auth_mode\" VALUE=\"%s\">%s",
				buf,
				((atoi(buf) != 0) ? "Yes" : "No"));
			sprintf(&access[strlen(access)], "</TD></TR>\n");
			break;
		case 53:
			sprintf(&funambol[strlen(funambol)], "<TR><TD>");
			sprintf(&funambol[strlen(funambol)], _("Funambol server host (blank to disable)"));
			sprintf(&funambol[strlen(funambol)], "</TD><TD>");
			sprintf(&funambol[strlen(funambol)], "<input type=\"text\" NAME=\"c_funambol_host\" MAXLENGTH=\"255\" VALUE=\"%s\">", buf);
			sprintf(&funambol[strlen(funambol)], "</TD></TR>\n");
			break;
		case 54:
			sprintf(&funambol[strlen(funambol)], "<TR><TD>");
			sprintf(&funambol[strlen(funambol)], _("Funambol server port "));
			sprintf(&funambol[strlen(funambol)], "</TD><TD>");
			sprintf(&funambol[strlen(funambol)], "<input type=\"text\" NAME=\"c_funambol_port\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
			sprintf(&funambol[strlen(funambol)], "</TD></TR>\n");
			break;
		case 55:
			sprintf(&funambol[strlen(funambol)], "<TR><TD>");
			sprintf(&funambol[strlen(funambol)], _("Funambol sync source"));
			sprintf(&funambol[strlen(funambol)], "</TD><TD>");
			sprintf(&funambol[strlen(funambol)], "<input type=\"text\" NAME=\"c_funambol_source\" MAXLENGTH=\"255\" VALUE=\"%s\">", buf);
			sprintf(&funambol[strlen(funambol)], "</TD></TR>\n");
			break;
		case 56:
			sprintf(&funambol[strlen(funambol)], "<TR><TD>");
			sprintf(&funambol[strlen(funambol)], _("Funambol auth details (user:pass)"));
			sprintf(&funambol[strlen(funambol)], "</TD><TD>");
			sprintf(&funambol[strlen(funambol)], "<input type=\"text\" NAME=\"c_funambol_auth\" MAXLENGTH=\"255\" VALUE=\"%s\">", buf);
			sprintf(&funambol[strlen(funambol)], "</TD></TR>\n");
			break;
		case 57:
			sprintf(&network[strlen(network)], "<TR><TD>");
			sprintf(&network[strlen(network)], _("Perform RBL checks upon connect instead of after RCPT"));
			sprintf(&network[strlen(network)], "</TD><TD>");
			sprintf(&network[strlen(network)], "<input type=\"checkbox\" NAME=\"c_rbl_at_greeting\" VALUE=\"yes\" %s>",
				((atoi(buf) != 0) ? "CHECKED" : ""));
			sprintf(&network[strlen(network)], "</TD></TR>\n");
			break;
		case 58:
			sprintf(&access[strlen(access)], "<TR><TD>");
			sprintf(&access[strlen(access)], _("Master user name (blank to disable)"));
			sprintf(&access[strlen(access)], "</TD><TD>");
			sprintf(&access[strlen(access)], "<input type=\"text\" NAME=\"c_master_user\" MAXLENGTH=\"31\" VALUE=\"%s\">", buf);
			sprintf(&access[strlen(access)], "</TD></TR>\n");
			break;
		case 59:
			sprintf(&access[strlen(access)], "<TR><TD>");
			sprintf(&access[strlen(access)], _("Master user password"));
			sprintf(&access[strlen(access)], "</TD><TD>");
			sprintf(&access[strlen(access)], "<input type=\"password\" NAME=\"c_master_pass\" MAXLENGTH=\"31\" VALUE=\"%s\">",
			buf);
			sprintf(&directory[strlen(directory)], "</TD></TR>\n");
			break;
		case 60:
			sprintf(&funambol[strlen(funambol)], "<TR><TD>");
			sprintf(&funambol[strlen(funambol)], _("External pager tool (blank to disable)"));
			sprintf(&funambol[strlen(funambol)], "</TD><TD>");
			sprintf(&funambol[strlen(funambol)], "<input type=\"text\" NAME=\"c_pager_program\" MAXLENGTH=\"255\" VALUE=\"%s\">", buf);
			sprintf(&funambol[strlen(funambol)], "</TD></TR>\n");
			break;
		case 61:
			sprintf(&network[strlen(network)], "<TR><TD>");
			sprintf(&network[strlen(network)], _("Keep original from headers in IMAP"));
			sprintf(&network[strlen(network)], "</TD><TD>");
			sprintf(&network[strlen(network)], "<input type=\"checkbox\" NAME=\"c_imap_keep_from\" VALUE=\"yes\" %s>",
				((atoi(buf) != 0) ? "CHECKED" : ""));
			sprintf(&network[strlen(network)], "</TD></TR>\n");
			break;
		case 62:
			sprintf(&network[strlen(network)], "<tr><td>");
			sprintf(&network[strlen(network)], _("XMPP (Jabber) client to server port (-1 to disable)"));
			sprintf(&network[strlen(network)], "</td><td>");
			sprintf(&network[strlen(network)], "<input type=\"text\" name=\"c_xmpp_c2s_port\" maxlength=\"5\" value=\"%s\">", buf);
			sprintf(&network[strlen(network)], "</TD></TR>\n");
			break;
		case 63:
			sprintf(&network[strlen(network)], "<tr><td>");
			sprintf(&network[strlen(network)], _("XMPP (Jabber) server to server port (-1 to disable)"));
			sprintf(&network[strlen(network)], "</td><td>");
			sprintf(&network[strlen(network)], "<input type=\"hidden\" name=\"c_xmpp_s2s_port\" value=\"%s\">\n", buf);
			sprintf(&network[strlen(network)], "</TD></TR>\n");
			break;
		case 64:
			sprintf(&pop3[strlen(pop3)], "<tr><td>");
			sprintf(&pop3[strlen(pop3)], _("POP3 fetch frequency in seconds"));
			sprintf(&pop3[strlen(pop3)], "</td><td>");
			sprintf(&pop3[strlen(pop3)], "<input type=\"text\" name=\"c_pop3_fetch\" MAXLENGTH=\"5\" value=\"%s\">\n", buf);
			sprintf(&pop3[strlen(pop3)], "</TD></TR>\n");
			break;
		case 65:
			sprintf(&pop3[strlen(pop3)], "<tr><td>");
			sprintf(&pop3[strlen(pop3)], _("POP3 fastest fetch frequency in seconds"));
			sprintf(&pop3[strlen(pop3)], "</td><td>");
			sprintf(&pop3[strlen(pop3)], "<input type=\"text\" name=\"c_pop3_fastest\" MAXLENGTH=\"5\" value=\"%s\">\n", buf);
			sprintf(&pop3[strlen(pop3)], "</TD></TR>\n");
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


	sprintf(&purger[strlen(purger)], "<TR><TD COLSPAN=2><hr /></TD></TR>\n");

	sprintf(&purger[strlen(purger)], "<TR><TD>");
	sprintf(&purger[strlen(purger)], _("Default message expire policy for public rooms"));
	sprintf(&purger[strlen(purger)], "</TD><TD>");
	sprintf(&purger[strlen(purger)], "<input type=\"radio\" NAME=\"sitepolicy\" VALUE=\"1\" %s>",
		((sitepolicy == 1) ? "CHECKED" : "") );
	sprintf(&purger[strlen(purger)], _("Never automatically expire messages"));
	sprintf(&purger[strlen(purger)], "<br />\n");
	sprintf(&purger[strlen(purger)], "<input type=\"radio\" NAME=\"sitepolicy\" VALUE=\"2\" %s>",
		((sitepolicy == 2) ? "CHECKED" : "") );
	sprintf(&purger[strlen(purger)], _("Expire by message count"));
	sprintf(&purger[strlen(purger)], "<br />\n");
	sprintf(&purger[strlen(purger)], "<input type=\"radio\" NAME=\"sitepolicy\" VALUE=\"3\" %s>",
		((sitepolicy == 3) ? "CHECKED" : "") );
	sprintf(&purger[strlen(purger)], _("Expire by message age"));
	sprintf(&purger[strlen(purger)], "<br />");
	sprintf(&purger[strlen(purger)], _("Number of messages or days: "));
	sprintf(&purger[strlen(purger)], "<input type=\"text\" NAME=\"sitevalue\" MAXLENGTH=\"5\" VALUE=\"%d\">", sitevalue);
	sprintf(&purger[strlen(purger)], "</TD></TR>\n");

	sprintf(&purger[strlen(purger)], "<TR><TD COLSPAN=2><hr /></TD></TR>\n");

	sprintf(&purger[strlen(purger)], "<TR><TD>");
	sprintf(&purger[strlen(purger)], _("Default message expire policy for private mailboxes"));
	sprintf(&purger[strlen(purger)], "</TD><TD>");
	sprintf(&purger[strlen(purger)], "<input type=\"radio\" NAME=\"mboxpolicy\" VALUE=\"0\" %s>",
		((mboxpolicy == 0) ? "CHECKED" : "") );
	sprintf(&purger[strlen(purger)], _("Same policy as public rooms"));
	sprintf(&purger[strlen(purger)], "<br />\n");
	sprintf(&purger[strlen(purger)], "<input type=\"radio\" NAME=\"mboxpolicy\" VALUE=\"1\" %s>",
			((mboxpolicy == 1) ? "CHECKED" : "") );
	sprintf(&purger[strlen(purger)], _("Never automatically expire messages"));
	sprintf(&purger[strlen(purger)], "<br />\n");
	sprintf(&purger[strlen(purger)], "<input type=\"radio\" NAME=\"mboxpolicy\" VALUE=\"2\" %s>",
		((mboxpolicy == 2) ? "CHECKED" : "") );
	sprintf(&purger[strlen(purger)], _("Expire by message count"));
	sprintf(&purger[strlen(purger)], "<br />\n");
	sprintf(&purger[strlen(purger)], "<input type=\"radio\" NAME=\"mboxpolicy\" VALUE=\"3\" %s>",
		((mboxpolicy == 3) ? "CHECKED" : "") );
	sprintf(&purger[strlen(purger)], _("Expire by message age"));
	sprintf(&purger[strlen(purger)], "<br />");
	sprintf(&purger[strlen(purger)], _("Number of messages or days: "));
	sprintf(&purger[strlen(purger)], "<input type=\"text\" NAME=\"mboxvalue\" MAXLENGTH=\"5\" VALUE=\"%d\">", mboxvalue);
	sprintf(&purger[strlen(purger)], "</TD></TR>\n");

	sprintf(&purger[strlen(purger)], "<TR><TD COLSPAN=2><hr /></TD></TR>\n");


	sprintf(&general[strlen(general)], "</table>");
	sprintf(&access[strlen(access)], "</table>");
	sprintf(&network[strlen(network)], "</table>");
	sprintf(&tuning[strlen(tuning)], "</table>");
	sprintf(&directory[strlen(directory)], "</table>");
	sprintf(&purger[strlen(purger)], "</table>");
	sprintf(&idxjnl[strlen(idxjnl)], "</table>");
	sprintf(&funambol[strlen(funambol)], "</table>");
	sprintf(&pop3[strlen(pop3)], "</table>");

	tabbed_dialog(9, tabnames);

	begin_tab(0, 9);	StrBufAppendBufPlain(WCC->WBuf, general, strlen(general), 0);		 end_tab(0, 9);
	begin_tab(1, 9);	StrBufAppendBufPlain(WCC->WBuf, access, strlen(access), 0);		 end_tab(1, 9);
	begin_tab(2, 9);	StrBufAppendBufPlain(WCC->WBuf, network, strlen(network), 0);		 end_tab(2, 9);
	begin_tab(3, 9);	StrBufAppendBufPlain(WCC->WBuf, tuning, strlen(tuning), 0);		 end_tab(3, 9);
	begin_tab(4, 9);	StrBufAppendBufPlain(WCC->WBuf, directory, strlen(directory), 0);	 end_tab(4, 9);
	begin_tab(5, 9);	StrBufAppendBufPlain(WCC->WBuf, purger, strlen(purger), 0);		 end_tab(5, 9);
	begin_tab(6, 9);	StrBufAppendBufPlain(WCC->WBuf, idxjnl, strlen(idxjnl), 0);		 end_tab(6, 9);
	begin_tab(7, 9);	StrBufAppendBufPlain(WCC->WBuf, funambol, strlen(funambol), 0);	 end_tab(7, 9);
	begin_tab(8, 9);	StrBufAppendBufPlain(WCC->WBuf, pop3, strlen(pop3), 0);	 	 end_tab(8, 9);

	begin_tab(9, 9);
	wprintf("<input type=\"submit\" NAME=\"ok_button\" VALUE=\"%s\">", _("Save changes"));
	wprintf("&nbsp;");
	wprintf("<input type=\"submit\" NAME=\"cancel_button\" VALUE=\"%s\">\n", _("Cancel"));
	end_tab(9, 9);

	wprintf("</FORM>\n");
	wprintf("</td></tr></table>\n");
	wDumpContent(1);
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
	{CFG_STR, HKEY("c_pop3_fastest")}
};


/*
 * \brief display all configuration items
 */
void load_siteconfig(void)
{
	struct wcsession *WCC = WC;
	StrBuf *Buf, *Token;
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
	while (len = serv_getln(buf, sizeof buf), 
	       strcmp(buf, "000") && 
	       (i < sizeof(ServerConfig))) 
	{
		Put(Cfg,
		    ServerConfig[i].Key, 
		    ServerConfig[i].len, 
		    NewStrBufPlain(buf, len), 
		    HFreeStrBuf);
		i++;
	}

	serv_puts("GPEX site");
	Buf = NewStrBuf();
	Token = NULL;
	StrBuf_ServGetln(Buf);
	if (ChrPtr(Buf)[0] == '2') {
		StrBufCutLeft(Buf, 4);

		Token = NewStrBuf();
		StrBufExtract_token(Token, Buf, 0, '|');
		Put(Cfg, HKEY("sitepolicy"), Token, HFreeStrBuf);

		Token = NewStrBuf();
		StrBufExtract_token(Token, Buf, 1, '|');
		Put(Cfg, HKEY("sitevalue"), Token, HFreeStrBuf);
	}

	serv_puts("GPEX mailboxes");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '2') {
		StrBufCutLeft(Buf, 4);

		Token = NewStrBuf();
		StrBufExtract_token(Token, Buf, 0, '|');
		Put(Cfg, HKEY("mboxpolicy"), Token, HFreeStrBuf);

		Token = NewStrBuf();
		StrBufExtract_token(Token, Buf, 1, '|');
		Put(Cfg, HKEY("mboxvalue"), Token, HFreeStrBuf);
	}
}


/**
 * parse siteconfig changes 
 */
void siteconfig(void)
{
	int i;
	char buf[256];

	if (strlen(bstr("ok_button")) == 0) {
		display_aide_menu();
		return;
	}
	serv_printf("CONF set");
	serv_getln(buf, sizeof buf);
	if (buf[0] != '4') {
		safestrncpy(WC->ImportantMessage, &buf[4], sizeof WC->ImportantMessage);
		display_aide_menu();
		return;
	}

	i = sizeof(ServerConfig);
	for (i=0; i < sizeof(ServerConfig); i ++)
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

	serv_printf("SPEX site|%d|%d", ibstr("sitepolicy"), ibstr("sitevalue"));
	serv_getln(buf, sizeof buf);
	serv_printf("SPEX mailboxes|%d|%d", ibstr("mboxpolicy"), ibstr("mboxvalue"));
	serv_getln(buf, sizeof buf);

	strcpy(serv_info.serv_default_cal_zone, bstr("c_default_cal_zone"));

	safestrncpy(WC->ImportantMessage, _("Your system configuration has been updated."),
		sizeof WC->ImportantMessage);
	display_aide_menu();
}

void tmplput_servcfg(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *Context)
{
	struct wcsession *WCC = WC;
	void *vBuf;
	StrBuf *Buf;

	if (WCC->is_aide) {
		if (WCC->ServCfg == NULL)
			load_siteconfig();
		GetHash(WCC->ServCfg, 
			Tokens->Params[0]->Start,
			Tokens->Params[0]->len, 
			&vBuf);
		Buf = (StrBuf*) vBuf;
		StrBufAppendBuf(Target, Buf, 0);
	}
}

int ConditionalServCfg(WCTemplateToken *Tokens, void *Context)
{
	struct wcsession *WCC = WC;
	void *vBuf;
	StrBuf *Buf;

	if (WCC->is_aide) {
		if (WCC->ServCfg == NULL)
			load_siteconfig();
		GetHash(WCC->ServCfg, 
			Tokens->Params[2]->Start,
			Tokens->Params[2]->len, 
			&vBuf);
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

void 
InitModule_SITECONFIG
(void)
{
	WebcitAddUrlHandler(HKEY("display_siteconfig"), display_siteconfig, 0);
	WebcitAddUrlHandler(HKEY("siteconfig"), siteconfig, 0);

	RegisterNamespace("SERV:CFG", 1, 1, tmplput_servcfg);
	RegisterConditional(HKEY("COND:SERVCFG"), 3, ConditionalServCfg);
}
/*@}*/
