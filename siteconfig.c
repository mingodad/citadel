/*
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
	int i;

	output_headers(3);

	whichmenu = bstr("whichmenu");

	svprintf("BOXTITLE", WCS_STRING, "Site configuration");
	do_template("beginbox");

	if (!strcmp(whichmenu, "")) {
		wprintf("<TABLE border=0 cellspacing=0 cellpadding=3 width=100%%>\n");

		wprintf("<TR BGCOLOR=\"#CCCCCC\"><TD>"
			"</TD><TD>"
			"<A HREF=\"/display_siteconfig?whichmenu=general\">"
			"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
			"SRC=\"/static/advanced-icon.gif\" ALT=\"&nbsp;\">"
			"</TD><TD>"
			"<A HREF=\"/display_siteconfig?whichmenu=general\">"
			"<B>General</B><BR>"
			"General site configuration items"
			"</A></TD></TR>\n"
		);

		wprintf("<TR><TD>"
			"</TD><TD>"
			"<A HREF=\"/display_siteconfig?whichmenu=access\">"
			"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
			"SRC=\"/static/advanced-icon.gif\" ALT=\"&nbsp;\">"
			"</TD><TD>"
			"<A HREF=\"/display_siteconfig?whichmenu=access\">"
			"<B>Access</B><BR>"
			"Access controls and site policy settings"
			"</A></TD></TR>\n"
		);

		wprintf("<TR BGCOLOR=\"#CCCCCC\"><TD>"
			"</TD><TD>"
			"<A HREF=\"/display_siteconfig?whichmenu=network\">"
			"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
			"SRC=\"/static/advanced-icon.gif\" ALT=\"&nbsp;\">"
			"</TD><TD>"
			"<A HREF=\"/display_siteconfig?whichmenu=network\">"
			"<B>Network</B><BR>"
			"Network services"
			"</A></TD></TR>\n"
		);

		wprintf("<TR><TD>"
			"</TD><TD>"
			"<A HREF=\"/display_siteconfig?whichmenu=tuning\">"
			"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
			"SRC=\"/static/advanced-icon.gif\" ALT=\"&nbsp;\">"
			"</TD><TD>"
			"<A HREF=\"/display_siteconfig?whichmenu=tuning\">"
			"<B>Tuning</B><BR>"
			"Advanced server fine-tuning controls"
			"</A></TD></TR>\n"
		);

		wprintf("</TABLE>");

		do_template("endbox");
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
		wprintf("<CENTER><H2>Network services</H2></CENTER>\n");
	}

	if (!strcasecmp(whichmenu, "tuning")) {
		wprintf("<CENTER><H2>Advanced server fine-tuning controls</H2></CENTER>\n");
	}

	serv_printf("CONF get");
	serv_gets(buf);
	if (buf[0] != '1') {
        	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#770000\"><TR><TD>");
        	wprintf("<SPAN CLASS=\"titlebar\">Error</SPAN>\n");
        	wprintf("</TD></TR></TABLE><BR>\n");
        	wprintf("%s<BR>\n", &buf[4]);
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
				wprintf("<INPUT TYPE=\"text\" NAME=\"c_initax\" MAXLENGTH=\"1\" VALUE=\"%s\">", buf);
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
				wprintf("<INPUT TYPE=\"text\" NAME=\"c_createax\" MAXLENGTH=\"1\" VALUE=\"%s\">", buf);
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
				wprintf("<TR><TD>SMTP listener port (-1 to disable)</TD><TD>");
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
			if (!strcasecmp(whichmenu, "access")) {
				wprintf("<TR><TD>Allow system Aides access to mailboxes</TD><TD>");
				wprintf("<INPUT TYPE=\"checkbox\" NAME=\"c_aide_mailboxes\" VALUE=\"yes\" %s>", ((atoi(buf) != 0) ? "CHECKED" : ""));
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"c_aide_mailboxes\" VALUE=\"%s\">", buf);
			}
			break;
		case 32:
			if (!strcasecmp(whichmenu, "tuning")) {
				wprintf("<TR><TD>Hour to run database auto-purge (0-23)</TD><TD>");
				wprintf("<INPUT TYPE=\"text\" NAME=\"c_purge_hour\" MAXLENGTH=\"2\" VALUE=\"%s\">", buf);
				wprintf("</TD></TR>\n");
			}
			else {
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"c_purge_hour\" VALUE=\"%s\">", buf);
			}
			break;
		}
	}

	wprintf("</TABLE><CENTER>");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"OK\">");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Cancel\">\n");
	wprintf("</CENTER></FORM>\n");
	do_template("endbox");
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
	serv_printf("%s", bstr("c_disable_newu"));
	serv_printf("%s", bstr("c_aide_mailboxes"));
	serv_printf("%s", bstr("c_purge_hour"));
	serv_printf("000");
	strcpy(WC->ImportantMessage, "System configuration has been updated.");
	display_siteconfig();
}
