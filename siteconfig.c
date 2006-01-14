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
	int i, j;

	char general[SIZ];
	char access[SIZ];
	char network[SIZ];
	char tuning[SIZ];
	char directory[SIZ];
	char purger[SIZ];
	char idxjnl[SIZ];

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

	serv_printf("CONF get");
	serv_getln(buf, sizeof buf);
	if (buf[0] != '1') {
        	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>");
        	wprintf("<SPAN CLASS=\"titlebar\">");
		wprintf(_("Error"));
		wprintf("</SPAN>\n");
        	wprintf("</TD></TR></TABLE><br />\n");
        	wprintf("%s<br />\n", &buf[4]);
		wDumpContent(1);
		return;
	}

	wprintf("<div id=\"fix_scrollbar_bug\">"
		"<table border=0 width=100%% bgcolor=\"#ffffff\"><tr><td>");

	char *tabnames[] = {
		_("General"),
		_("Access"),
		_("Network"),
		_("Tuning"),
		_("Directory"),
		_("Auto-purger"),
		_("Indexing/Journaling")
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
			_("Changes made on this screen will not take effect "
			"until you restart the Citadel server.")
	);

	sprintf(purger, "<center><h1>%s</h1><h2>%s</h2><table border=\"0\">",
			_("Configure automatic expiry of old messages"),
			_("These settings may be overridden on a per-floor or per-room basis.")
	);

	sprintf(idxjnl, "<center><h1>%s</h1><h2>%s</h2><table border=\"0\">",
			_("Indexing and Journaling"),
			_("Warning: these facilities are resource intensive.")
	);


	wprintf("<form method=\"post\" action=\"siteconfig\">\n");

	i = 0;
	while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		switch (i++) {
		case 0:
			sprintf(&general[strlen(general)], "<TR><TD>");
			sprintf(&general[strlen(general)], _("Node name"));
			sprintf(&general[strlen(general)], "</TD><TD>");
			sprintf(&general[strlen(general)], "<input type=\"text\" NAME=\"c_nodename\" MAXLENGTH=\"15\" VALUE=\"%s\">", buf);
			sprintf(&general[strlen(general)], "</TD></TR>\n");
			break;
		case 1:
			sprintf(&general[strlen(general)], "<TR><TD>");
			sprintf(&general[strlen(general)], _("Fully qualified domain name"));
			sprintf(&general[strlen(general)], "</TD><TD>");
			sprintf(&general[strlen(general)], "<input type=\"text\" NAME=\"c_fqdn\" MAXLENGTH=\"63\" VALUE=\"%s\">", buf);
			sprintf(&general[strlen(general)], "</TD></TR>\n");
			break;
		case 2:
			sprintf(&general[strlen(general)], "<TR><TD>");
			sprintf(&general[strlen(general)], _("Human-readable node name"));
			sprintf(&general[strlen(general)], "</TD><TD>");
			sprintf(&general[strlen(general)], "<input type=\"text\" NAME=\"c_humannode\" MAXLENGTH=\"20\" VALUE=\"%s\">", buf);
			sprintf(&general[strlen(general)], "</TD></TR>\n");
			break;
		case 3:
			sprintf(&general[strlen(general)], "<TR><TD>");
			sprintf(&general[strlen(general)], _("Telephone number"));
			sprintf(&general[strlen(general)], "</TD><TD>");
			sprintf(&general[strlen(general)], "<input type=\"text\" NAME=\"c_phonenum\" MAXLENGTH=\"15\" VALUE=\"%s\">", buf);
			sprintf(&general[strlen(general)], "</TD></TR>\n");
			break;
		case 4:
			sprintf(&access[strlen(access)], "<TR><TD>");
			sprintf(&access[strlen(access)], _("Automatically grant room-aide status to users who create private rooms"));
			sprintf(&access[strlen(access)], "</TD><TD>");
			sprintf(&access[strlen(access)], "<input type=\"checkbox\" NAME=\"c_creataide\" VALUE=\"yes\" %s>",
				((atoi(buf) != 0) ? "CHECKED" : ""));
			sprintf(&access[strlen(access)], "</TD></TR>\n");
			break;
		case 5:
			sprintf(&tuning[strlen(tuning)], "<TR><TD>");
			sprintf(&tuning[strlen(tuning)], _("Server connection idle timeout (in seconds)"));
			sprintf(&tuning[strlen(tuning)], "</TD><TD>");
			sprintf(&tuning[strlen(tuning)], "<input type=\"text\" NAME=\"c_sleeping\" MAXLENGTH=\"15\" VALUE=\"%s\">", buf);
			sprintf(&tuning[strlen(tuning)], "</TD></TR>\n");
			break;
		case 6:
			sprintf(&access[strlen(access)], "<TR><TD>");
			sprintf(&access[strlen(access)], _("Initial access level for new users"));
			sprintf(&access[strlen(access)], "</TD><TD>");
			sprintf(&access[strlen(access)], "<SELECT NAME=\"c_initax\" SIZE=\"1\">\n");
			for (j=0; j<=6; ++j) {
				sprintf(&access[strlen(access)], "<OPTION %s VALUE=\"%d\">%d - %s</OPTION>\n",
					((atoi(buf) == j) ? "SELECTED" : ""),
					j, j, axdefs[j]
				);
			}
			sprintf(&access[strlen(access)], "</SELECT>");
			sprintf(&access[strlen(access)], "</TD></TR>\n");
			break;
		case 7:
			sprintf(&access[strlen(access)], "<TR><TD>");
			sprintf(&access[strlen(access)], _("Require registration for new users"));
			sprintf(&access[strlen(access)], "</TD><TD>");
			sprintf(&access[strlen(access)], "<input type=\"checkbox\" NAME=\"c_regiscall\" VALUE=\"yes\" %s>",
				((atoi(buf) != 0) ? "CHECKED" : ""));
			sprintf(&access[strlen(access)], "</TD></TR>\n");
			break;
		case 8:
			sprintf(&access[strlen(access)], "<TR><TD>");
			sprintf(&access[strlen(access)], _("Quarantine messages from problem users"));
			sprintf(&access[strlen(access)], "</TD><TD>");
			sprintf(&access[strlen(access)], "<input type=\"checkbox\" NAME=\"c_twitdetect\" VALUE=\"yes\" %s>",
				((atoi(buf) != 0) ? "CHECKED" : ""));
			sprintf(&access[strlen(access)], "</TD></TR>\n");
			break;
		case 9:
			sprintf(&access[strlen(access)], "<TR><TD>");
			sprintf(&access[strlen(access)], _("Name of quarantine room"));
			sprintf(&access[strlen(access)], "</TD><TD>");
			sprintf(&access[strlen(access)], "<input type=\"text\" NAME=\"c_twitroom\" MAXLENGTH=\"63\" VALUE=\"%s\">", buf);
			sprintf(&access[strlen(access)], "</TD></TR>\n");
			break;
		case 10:
			sprintf(&general[strlen(general)], "<TR><TD>");
			sprintf(&general[strlen(general)], _("Paginator prompt (for text mode clients)"));
			sprintf(&general[strlen(general)], "</TD><TD>");
			sprintf(&general[strlen(general)], "<input type=\"text\" NAME=\"c_moreprompt\" MAXLENGTH=\"79\" VALUE=\"%s\">", buf);
			sprintf(&general[strlen(general)], "</TD></TR>\n");
			break;
		case 11:
			sprintf(&access[strlen(access)], "<TR><TD>");
			sprintf(&access[strlen(access)], _("Restrict access to Internet mail"));
			sprintf(&access[strlen(access)], "</TD><TD>");
			sprintf(&access[strlen(access)], "<input type=\"checkbox\" NAME=\"c_restrict\" VALUE=\"yes\" %s>",
				((atoi(buf) != 0) ? "CHECKED" : ""));
			sprintf(&access[strlen(access)], "</TD></TR>\n");
			break;
		case 12:
			sprintf(&general[strlen(general)], "<TR><TD>");
			sprintf(&general[strlen(general)], _("Geographic location of this system"));
			sprintf(&general[strlen(general)], "</TD><TD>");
			sprintf(&general[strlen(general)], "<input type=\"text\" NAME=\"c_bbs_city\" MAXLENGTH=\"31\" VALUE=\"%s\">", buf);
			sprintf(&general[strlen(general)], "</TD></TR>\n");
			break;
		case 13:
			sprintf(&general[strlen(general)], "<TR><TD>");
			sprintf(&general[strlen(general)], _("Name of system administrator"));
			sprintf(&general[strlen(general)], "</TD><TD>");
			sprintf(&general[strlen(general)], "<input type=\"text\" NAME=\"c_sysadm\" MAXLENGTH=\"25\" VALUE=\"%s\">", buf);
			sprintf(&general[strlen(general)], "</TD></TR>\n");
			break;
		case 14:
			sprintf(&tuning[strlen(tuning)], "<TR><TD>");
			sprintf(&tuning[strlen(tuning)], _("Maximum concurrent sessions (0 = no limit)"));
			sprintf(&tuning[strlen(tuning)], "</TD><TD>");
			sprintf(&tuning[strlen(tuning)], "<input type=\"text\" NAME=\"c_maxsessions\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
			sprintf(&tuning[strlen(tuning)], "</TD></TR>\n");
			break;
		case 16:
			sprintf(&tuning[strlen(tuning)], "<TR><TD>");
			sprintf(&tuning[strlen(tuning)], _("Default user purge time (days)"));
			sprintf(&tuning[strlen(tuning)], "</TD><TD>");
			sprintf(&tuning[strlen(tuning)], "<input type=\"text\" NAME=\"c_userpurge\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
			sprintf(&tuning[strlen(tuning)], "</TD></TR>\n");
			break;
		case 17:
			sprintf(&tuning[strlen(tuning)], "<TR><TD>");
			sprintf(&tuning[strlen(tuning)], _("Default room purge time (days)"));
			sprintf(&tuning[strlen(tuning)], "</TD><TD>");
			sprintf(&tuning[strlen(tuning)], "<input type=\"text\" NAME=\"c_roompurge\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
			sprintf(&tuning[strlen(tuning)], "</TD></TR>\n");
			break;
		case 18:
			sprintf(&access[strlen(access)], "<TR><TD>");
			sprintf(&access[strlen(access)], _("Name of room to log pages"));
			sprintf(&access[strlen(access)], "</TD><TD>");
			sprintf(&access[strlen(access)], "<input type=\"text\" NAME=\"c_logpages\" MAXLENGTH=\"63\" VALUE=\"%s\">", buf);
			sprintf(&access[strlen(access)], "</TD></TR>\n");
			break;
		case 19:
			sprintf(&access[strlen(access)], "<TR><TD>");
			sprintf(&access[strlen(access)], _("Access level required to create rooms"));
			sprintf(&access[strlen(access)], "</TD><TD>");
			sprintf(&access[strlen(access)], "<SELECT NAME=\"c_createax\" SIZE=\"1\">\n");
			for (j=0; j<=6; ++j) {
				sprintf(&access[strlen(access)], "<OPTION %s VALUE=\"%d\">%d - %s</OPTION>\n",
					((atoi(buf) == j) ? "SELECTED" : ""),
					j, j, axdefs[j]
				);
			}
			sprintf(&access[strlen(access)], "</SELECT>");
			sprintf(&access[strlen(access)], "</TD></TR>\n");
			break;
		case 20:
			sprintf(&tuning[strlen(tuning)], "<TR><TD>");
			sprintf(&tuning[strlen(tuning)], _("Maximum message length"));
			sprintf(&tuning[strlen(tuning)], "</TD><TD>");
			sprintf(&tuning[strlen(tuning)], "<input type=\"text\" NAME=\"c_maxmsglen\" MAXLENGTH=\"20\" VALUE=\"%s\">", buf);
			sprintf(&tuning[strlen(tuning)], "</TD></TR>\n");
			break;
		case 21:
			sprintf(&tuning[strlen(tuning)], "<TR><TD>");
			sprintf(&tuning[strlen(tuning)], _("Minimum number of worker threads"));
			sprintf(&tuning[strlen(tuning)], "</TD><TD>");
			sprintf(&tuning[strlen(tuning)], "<input type=\"text\" NAME=\"c_min_workers\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
			sprintf(&tuning[strlen(tuning)], "</TD></TR>\n");
			break;
		case 22:
			sprintf(&tuning[strlen(tuning)], "<TR><TD>");
			sprintf(&tuning[strlen(tuning)], _("Maximum number of worker threads"));
			sprintf(&tuning[strlen(tuning)], "</TD><TD>");
			sprintf(&tuning[strlen(tuning)], "<input type=\"text\" NAME=\"c_max_workers\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
			sprintf(&tuning[strlen(tuning)], "</TD></TR>\n");
			break;
		case 23:
			sprintf(&network[strlen(network)], "<TR><TD>");
			sprintf(&network[strlen(network)], _("POP3 listener port (-1 to disable)"));
			sprintf(&network[strlen(network)], "</TD><TD>");
			sprintf(&network[strlen(network)], "<input type=\"text\" NAME=\"c_pop3_port\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
			sprintf(&network[strlen(network)], "</TD></TR>\n");
			break;
		case 24:
			sprintf(&network[strlen(network)], "<TR><TD>");
			sprintf(&network[strlen(network)], _("SMTP MTA port (-1 to disable)"));
			sprintf(&network[strlen(network)], "</TD><TD>");
			sprintf(&network[strlen(network)], "<input type=\"text\" NAME=\"c_smtp_port\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
			sprintf(&network[strlen(network)], "</TD></TR>\n");
			break;
		case 25:	/* note: reverse bool */
			sprintf(&access[strlen(access)], "<TR><TD>");
			sprintf(&access[strlen(access)], _("Correct forged From: lines during authenticated SMTP"));
			sprintf(&access[strlen(access)], "</TD><TD>");
			sprintf(&access[strlen(access)], "<input type=\"checkbox\" NAME=\"c_aide_zap\" VALUE=\"yes\" %s>",
				((atoi(buf) == 0) ? "CHECKED" : ""));
			sprintf(&access[strlen(access)], "</TD></TR>\n");
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
					((j == 0) ? 12 : ((j>12) ? j-12 : j)),
					((j >= 12) ? "pm" : "am")
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
			sprintf(&network[strlen(network)], "<TR><TD>");
			sprintf(&network[strlen(network)], _("POP3 over SSL port (-1 to disable)"));
			sprintf(&network[strlen(network)], "</TD><TD>");
			sprintf(&network[strlen(network)], "<input type=\"text\" NAME=\"c_pop3s_port\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
			sprintf(&network[strlen(network)], "</TD></TR>\n");
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

	tabbed_dialog(7, tabnames);

	begin_tab(0, 7);	wprintf("%s", general);		 end_tab(0, 7);
	begin_tab(1, 7);	wprintf("%s", access);		 end_tab(1, 7);
	begin_tab(2, 7);	wprintf("%s", network);		 end_tab(2, 7);
	begin_tab(3, 7);	wprintf("%s", tuning);		 end_tab(3, 7);
	begin_tab(4, 7);	wprintf("%s", directory);	 end_tab(4, 7);
	begin_tab(5, 7);	wprintf("%s", purger);		 end_tab(5, 7);
	begin_tab(6, 7);	wprintf("%s", idxjnl);	 end_tab(6, 7);

	wprintf("<div align=\"center\"><br>");
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
	serv_printf("");  /* placeholder - this field is not in use */
	serv_printf("%s", bstr("c_userpurge"));
	serv_printf("%s", bstr("c_roompurge"));
	serv_printf("%s", bstr("c_logpages"));
	serv_printf("%s", bstr("c_createax"));
	serv_printf("%s", bstr("c_maxmsglen"));
	serv_printf("%s", bstr("c_min_workers"));
	serv_printf("%s", bstr("c_max_workers"));
	serv_printf("%s", bstr("c_pop3_port"));
	serv_printf("%s", bstr("c_smtp_port"));
	serv_printf("%s", ((!strcasecmp(bstr("c_rfc822_strict_from"), "yes") ? "0" : "1"))); /* note: reverse bool */
	serv_printf("%s", ((!strcasecmp(bstr("c_aide_zap"), "yes") ? "1" : "0")));
	serv_printf("%s", bstr("c_imap_port"));
	serv_printf("%s", bstr("c_net_freq"));
	serv_printf("%s", ((!strcasecmp(bstr("c_disable_newu"), "yes") ? "1" : "0")));
	serv_printf("1"); /* placeholder - this field is not in use */
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
	serv_printf("%s", ((!strcasecmp(bstr("c_journal_email"), "yes") ? "1" : "0")));
	serv_printf("%s", ((!strcasecmp(bstr("c_journal_pubmsgs"), "yes") ? "1" : "0")));
	serv_printf("%s", bstr("c_journal_dest"));
	serv_printf("000");

	serv_printf("SPEX site|%d|%d", atoi(bstr("sitepolicy")), atoi(bstr("sitevalue")));
	serv_getln(buf, sizeof buf);
	serv_printf("SPEX mailboxes|%d|%d", atoi(bstr("mboxpolicy")), atoi(bstr("mboxvalue")));
	serv_getln(buf, sizeof buf);

	safestrncpy(WC->ImportantMessage, _("Your system configuration has been updated."),
		sizeof WC->ImportantMessage);
	display_aide_menu();
}
