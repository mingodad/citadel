/*
 * Administrative screen for site-wide configuration
 *
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <ctype.h>
#include <string.h>
#include "webcit.h"
#include "child.h"


void display_siteconfig(void)
{
	char buf[256];
	int i;

	printf("HTTP/1.0 200 OK\n");
	output_headers(1);

	serv_printf("CONF get");
	serv_gets(buf);
	if (buf[0] != '1') {
        	wprintf("<TABLE WIDTH=100% BORDER=0 BGCOLOR=770000><TR><TD>");
        	wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"");
        	wprintf("<B>Error</B>\n");
        	wprintf("</FONT></TD></TR></TABLE><BR>\n");
        	wprintf("%s<BR>\n", &buf[4]);
		wDumpContent(1);
		return;
	}

	wprintf("<TABLE WIDTH=100% BORDER=0 BGCOLOR=007700><TR><TD>");
	wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"<B>Site configuration");
	wprintf("</B></FONT></TD></TR></TABLE>\n");

	wprintf("<FORM METHOD=\"POST\" ACTION=\"/siteconfig\">\n");
	wprintf("<TABLE border=0>\n");

	i = 0;
	while (serv_gets(buf), strcmp(buf, "000")) {
		switch (++i) {
		case 1:
			wprintf("<TR><TD>Node name</TD><TD>");
			wprintf("<INPUT TYPE=\"text\" NAME=\"c_nodename\" MAXLENGTH=\"15\" VALUE=\"%s\">", buf);
			wprintf("</TD></TR>\n");
			break;
		case 2:
			wprintf("<TR><TD>Fully qualified domain name</TD><TD>");
			wprintf("<INPUT TYPE=\"text\" NAME=\"c_fqdn\" MAXLENGTH=\"63\" VALUE=\"%s\">", buf);
			wprintf("</TD></TR>\n");
			break;
		case 3:
			wprintf("<TR><TD>Human-readable node name</TD><TD>");
			wprintf("<INPUT TYPE=\"text\" NAME=\"c_humannode\" MAXLENGTH=\"20\" VALUE=\"%s\">", buf);
			wprintf("</TD></TR>\n");
			break;
		case 4:
			wprintf("<TR><TD>Landline telephone number</TD><TD>");
			wprintf("<INPUT TYPE=\"text\" NAME=\"c_phonenum\" MAXLENGTH=\"15\" VALUE=\"%s\">", buf);
			wprintf("</TD></TR>\n");
			break;
		case 5:
			wprintf("<TR><TD>Automatically grant room-aide status to users who create private rooms</TD><TD>");
			wprintf("<INPUT TYPE=\"checkbox\" NAME=\"c_creataide\" VALUE=\"yes\" %s>", ((atoi(buf) != 0) ? "CHECKED" : ""));
			wprintf("</TD></TR>\n");
			break;
		case 6:
			wprintf("<TR><TD>Server connection idle timeout (in seconds)</TD><TD>");
			wprintf("<INPUT TYPE=\"text\" NAME=\"c_sleeping\" MAXLENGTH=\"15\" VALUE=\"%s\">", buf);
			wprintf("</TD></TR>\n");
			break;
		case 7:
			wprintf("<TR><TD>Initial access level for new users</TD><TD>");
			wprintf("<INPUT TYPE=\"text\" NAME=\"c_initax\" MAXLENGTH=\"1\" VALUE=\"%s\">", buf);
			wprintf("</TD></TR>\n");
			break;
		case 8:
			wprintf("<TR><TD>Require registration for new users</TD><TD>");
			wprintf("<INPUT TYPE=\"checkbox\" NAME=\"c_regiscall\" VALUE=\"yes\" %s>", ((atoi(buf) != 0) ? "CHECKED" : ""));
			wprintf("</TD></TR>\n");
			break;
		case 9:
			wprintf("<TR><TD>Move problem user messages to twitroom</TD><TD>");
			wprintf("<INPUT TYPE=\"checkbox\" NAME=\"c_twitdetect\" VALUE=\"yes\" %s>", ((atoi(buf) != 0) ? "CHECKED" : ""));
			wprintf("</TD></TR>\n");
			break;
		case 10:
			wprintf("<TR><TD>Name of twitroom</TD><TD>");
			wprintf("<INPUT TYPE=\"text\" NAME=\"c_twitroom\" MAXLENGTH=\"63\" VALUE=\"%s\">", buf);
			wprintf("</TD></TR>\n");
			break;
		case 11:
			wprintf("<TR><TD>Paginator prompt</TD><TD>");
			wprintf("<INPUT TYPE=\"text\" NAME=\"c_moreprompt\" MAXLENGTH=\"79\" VALUE=\"%s\">", buf);
			wprintf("</TD></TR>\n");
			break;
		case 12:
			wprintf("<TR><TD>Restrict access to Internet mail</TD><TD>");
			wprintf("<INPUT TYPE=\"checkbox\" NAME=\"c_restrict\" VALUE=\"yes\" %s>", ((atoi(buf) != 0) ? "CHECKED" : ""));
			wprintf("</TD></TR>\n");
			break;
		case 13:
			wprintf("<TR><TD>Geographic location of this system</TD><TD>");
			wprintf("<INPUT TYPE=\"text\" NAME=\"c_bbs_city\" MAXLENGTH=\"31\" VALUE=\"%s\">", buf);
			wprintf("</TD></TR>\n");
			break;
		case 14:
			wprintf("<TR><TD>Name of system administrator</TD><TD>");
			wprintf("<INPUT TYPE=\"text\" NAME=\"c_sysadm\" MAXLENGTH=\"25\" VALUE=\"%s\">", buf);
			wprintf("</TD></TR>\n");
			break;
		case 15:
			wprintf("<TR><TD>Maximum concurrent sessions</TD><TD>");
			wprintf("<INPUT TYPE=\"text\" NAME=\"c_maxsessions\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
			wprintf("</TD></TR>\n");
			break;
		case 16:
			wprintf("<TR><TD>Server-to-server networking password</TD><TD>");
			wprintf("<INPUT TYPE=\"password\" NAME=\"c_net_password\" MAXLENGTH=\"19\" VALUE=\"%s\">", buf);
			wprintf("</TD></TR>\n");
			break;
		case 17:
			wprintf("<TR><TD>Default user purge time (days)</TD><TD>");
			wprintf("<INPUT TYPE=\"text\" NAME=\"c_userpurge\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
			wprintf("</TD></TR>\n");
			break;
		case 18:
			wprintf("<TR><TD>Default room purge time (days)</TD><TD>");
			wprintf("<INPUT TYPE=\"text\" NAME=\"c_roompurge\" MAXLENGTH=\"5\" VALUE=\"%s\">", buf);
			wprintf("</TD></TR>\n");
			break;
		case 19:
			wprintf("<TR><TD>Name of room to log pages</TD><TD>");
			wprintf("<INPUT TYPE=\"text\" NAME=\"c_logpages\" MAXLENGTH=\"63\" VALUE=\"%s\">", buf);
			wprintf("</TD></TR>\n");
			break;
		case 20:
			wprintf("<TR><TD>Access level required to create rooms</TD><TD>");
			wprintf("<INPUT TYPE=\"text\" NAME=\"c_createax\" MAXLENGTH=\"1\" VALUE=\"%s\">", buf);
			wprintf("</TD></TR>\n");
			break;
		case 21:
			wprintf("<TR><TD>Maximum message length</TD><TD>");
			wprintf("<INPUT TYPE=\"text\" NAME=\"c_maxmsglen\" MAXLENGTH=\"20\" VALUE=\"%s\">", buf);
			wprintf("</TD></TR>\n");
			break;
		}
	}

	wprintf("</TABLE><CENTER>");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"OK\">");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Cancel\">\n");
	wprintf("</CENTER></FORM>\n");
	wDumpContent(1);
}


void siteconfig(void)
{
	char buf[256];

	if (strcasecmp(bstr("sc"), "OK")) {
		display_main_menu();
		return;
	}
	serv_printf("CONF set");
	serv_gets(buf);
	if (buf[0] != '4') {
		display_error(&buf[4]);
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
	serv_printf("%s", bstr("c_net_password"));
	serv_printf("%s", bstr("c_userpurge"));
	serv_printf("%s", bstr("c_roompurge"));
	serv_printf("%s", bstr("c_logpages"));
	serv_printf("%d", bstr("c_createax"));
	serv_printf("%d", bstr("c_maxmsglen"));
	serv_printf("000");
	display_success("System configuration has been updated.");
}
