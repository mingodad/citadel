/* $Id$ */




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



/*
 * The Main Menu
 */
void display_main_menu(void)
{
	output_headers(1);

	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=770077><TR><TD>"
		"<FONT SIZE=+1 COLOR=\"FFFFFF\""
		"<B>Basic commands</B>\n"
		"</FONT></TD>"
		"</TD></TR></TABLE>\n"
		"<CENTER><TABLE border=0><TR>"
	);

	wprintf("<TD>");	/* start of first column */

	wprintf("<UL>");
	wprintf("<LI><B><A HREF=\"/knrooms\">\n");
	wprintf("List known rooms</B></A><BR>\n");
	wprintf("Where can I go from here?</LI>\n");

	wprintf("<LI><B><A HREF=\"/gotonext\">\n");
	wprintf("Goto next room</B></A><BR>\n");
	wprintf("...with <EM>unread</EM> messages</LI>\n");

	wprintf("<LI><B><A HREF=\"/skip\">\n");
	wprintf("Skip to next room</B></A><BR>\n");
	wprintf("(come back here later)</LI>\n");

	if ((strlen(WC->ugname) > 0) && (strcasecmp(WC->ugname, WC->wc_roomname))) {
		wprintf("<LI><B><A HREF=\"/ungoto\">\n");
		wprintf("Ungoto</B></A><BR>\n");
		wprintf("(oops! Back to %s)</LI>\n", WC->ugname);
	}
	wprintf("</UL>\n");

	wprintf("</TD><TD>\n");	/* start of second column */

	wprintf("<UL>");
	wprintf("<LI><B><A HREF=\"/readnew\">\n");
	wprintf("Read new messages</B></A><BR>...in this room</LI>\n");

	wprintf("<LI><B><A HREF=\"/readfwd\">\n");
	wprintf("Read all messages</B></A><BR>...old <EM>and</EM> new</LI>\n");

	wprintf("<LI><B><A HREF=\"/display_enter\">\n");
	wprintf("Enter a message</B></A><BR>(post in this room)</LI>");
	wprintf("</UL>\n");

	wprintf("</TD><TD>");	/* start of third column */

	wprintf("<UL>");
	wprintf("<LI><B><A HREF=\"/whobbs\">\n");
	wprintf("Who is online?</B></A><BR>(users <EM>currently</EM> logged on)</LI>\n");

	wprintf("<LI><B><A HREF=\"/userlist\">\n");
	wprintf("User list</B></A><BR>(all registered users)</LI>\n");

	wprintf("<LI><B><A HREF=\"/termquit\" TARGET=\"_top\">\n");
	wprintf("Log off</B></A><BR>Bye!</LI>\n");
	wprintf("</UL>\n");

	wprintf("</TR></TABLE>\n");

	wprintf("<TABLE WIDTH=100%%><TR VALIGN=TOP><TD>");

	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=770000><TR><TD>");
	wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"");
	wprintf("<B>Interaction</B>\n");
	wprintf("</FONT></TD></TR></TABLE>\n");

	wprintf("<UL>");
	wprintf("<LI><A HREF=\"/chat\">");
	wprintf("Chat with other online users</A>\n");

	wprintf("<LI><A HREF=\"/display_generic\">\n");
	wprintf("<FONT SIZE=-2>Generic server command</FONT></A>\n");

	wprintf("</UL>\n");

	wprintf("</TD><TD>");

	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=007700><TR><TD>");
	wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"");
	wprintf("<B>Your info</B>\n");
	wprintf("</FONT></TD></TR></TABLE>\n");

	wprintf("<UL>");
	wprintf("<LI><A HREF=\"/display_editbio\">\n");
	wprintf("Enter your 'bio' (a few words about yourself)</A>\n");

	wprintf("<LI><A HREF=\"/display_editpic\">\n");
	wprintf("Edit your online photo</A>\n");

	wprintf("<LI><A HREF=\"/display_reg\">\n");
	wprintf("Re-enter your registration info (name, address, etc.)</A>\n");

	wprintf("<LI><A HREF=\"/display_changepw\">\n");
	wprintf("Change your password</A>\n");

	wprintf("</UL>\n");


	wprintf("</TD></TR><TR VALIGN=TOP><TD>");

	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=000077><TR><TD>");
	wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"");
	wprintf("<B>Advanced room commands</B>\n");
	wprintf("</FONT></TD></TR></TABLE>\n");

	wprintf("<UL>");
	wprintf("<LI><A HREF=\"/display_private\">\n");
	wprintf("Go to a 'hidden' room</A>\n");

	wprintf("<LI><A HREF=\"/display_entroom\">");
	wprintf("Create a new room</A>\n");

	wprintf("<LI><A HREF=\"/display_zap\">");
	wprintf("Zap (forget) this room (%s)</A>\n", WC->wc_roomname);

        wprintf("<LI><A HREF=\"/display_whok\">\n");
        wprintf("Access controls for this room</A>\n");

	wprintf("<LI><A HREF=\"/zapped_list\">");
	wprintf("List all forgotten rooms</A>\n");

	wprintf("</UL>\n");

	wprintf("</TD><TD>");

	if ((WC->axlevel >= 6) || (WC->is_room_aide)) {
		wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=007777><TR><TD>");
		wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"");
		wprintf("<B>Administrative functions</B>\n");
		wprintf("</FONT></TD></TR></TABLE>\n");

		wprintf("<UL>");
		wprintf("<LI><A HREF=\"/display_editroom\">\n");
		wprintf("Edit or delete this room</A>\n");

		wprintf("<LI><A HREF=\"/display_siteconfig\">\n");
		wprintf("Edit site-wide configuration</A>\n");

		if (WC->axlevel >= 6) {
			wprintf("<LI><A HREF=\"/select_user_to_edit\">\n");
			wprintf("Add, change, delete user accounts</A>\n");

			wprintf("<LI><A HREF=\"/validate\">\n");
			wprintf("Validate new users</A>\n");

			wprintf("<LI><A HREF=\"/select_floor_to_edit_pic\">\n");
			wprintf("Set or change a floor label graphic</A>\n");

			wprintf("<LI><A HREF=\"/display_netconf\">\n");
			wprintf("Configure networking with other systems</A>\n");
		}
		wprintf("</UL>\n");
	}
	wprintf("</TD></TR></TABLE>");
	wDumpContent(2);
}





/*
 * Display the screen to enter a generic server command
 */
void display_generic(void)
{
	output_headers(1);

	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=770077><TR><TD>");
	wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"");
	wprintf("<B>Enter a server command</B>\n");
	wprintf("</FONT></TD></TR></TABLE>\n");

	wprintf("<CENTER>");
	wprintf("This screen allows you to enter Citadel server commands which are\n");
	wprintf("not supported by WebCit.  If you do not know what that means,\n");
	wprintf("then this screen will not be of much use to you.<BR>\n");

	wprintf("<FORM METHOD=\"POST\" ACTION=\"/do_generic\">\n");

	wprintf("Enter command:<BR>\n");
	wprintf("<INPUT TYPE=\"text\" NAME=\"g_cmd\" SIZE=80 MAXLENGTH=\"250\"><BR>\n");

	wprintf("Command input (if requesting SEND_LISTING transfer mode):<BR>\n");
	wprintf("<TEXTAREA NAME=\"g_input\" ROWS=10 COLS=80 WIDTH=80></TEXTAREA>\n");

	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Send command\">");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Cancel\"><BR>\n");

	wprintf("</FORM></CENTER>\n");
	wDumpContent(1);
}

void do_generic(void)
{
	char buf[SIZ];
	char gcontent[SIZ];
	char *junk;
	size_t len;

	if (strcasecmp(bstr("sc"), "Send command")) {
		display_main_menu();
		return;
	}

	output_headers(1);

	serv_printf("%s", bstr("g_cmd"));
	serv_gets(buf);

	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=770077><TR><TD>");
	wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"");
	wprintf("<B>Server command results</B>\n");
	wprintf("</FONT></TD></TR></TABLE>\n");

	wprintf("<TABLE border=0><TR><TD>Command:</TD><TD><TT>");
	escputs(bstr("g_cmd"));
	wprintf("</TT></TD></TR><TR><TD>Result:</TD><TD><TT>");
	escputs(buf);
	wprintf("</TT></TD></TR></TABLE><BR>\n");

	if (buf[0] == '8') {
		serv_printf("\n\n000");
	}
	if ((buf[0] == '1') || (buf[0] == '8')) {
		while (serv_gets(gcontent), strcmp(gcontent, "000")) {
			escputs(gcontent);
			wprintf("<BR>\n");
		}
		wprintf("000");
	}
	if (buf[0] == '4') {
		text_to_server(bstr("g_input"));
		serv_puts("000");
	}
	if (buf[0] == '6') {
		len = atol(&buf[4]);
		junk = malloc(len);
		serv_read(junk, len);
		free(junk);
	}
	if (buf[0] == '7') {
		len = atol(&buf[4]);
		junk = malloc(len);
		memset(junk, 0, len);
		serv_write(junk, len);
		free(junk);
	}
	wprintf("<HR>");
	wprintf("<A HREF=\"/display_generic\">Enter another command</A><BR>\n");
	wprintf("<A HREF=\"/display_advanced\">Return to menu</A>\n");
	wDumpContent(1);
}




/*
 * Display the menubar.  Set as_single_page to
 * display HTML headers and footers -- otherwise it's assumed
 * that the menubar is being embedded in another page.
 */
void display_menubar(int as_single_page) {

	if (as_single_page) {
		output_headers(0);
		wprintf("<HTML>\n"
			"<HEAD>\n"
			"<TITLE>MenuBar</TITLE>\n"
			"<STYLE TYPE=\"text/css\">\n"
			"BODY	{ text-decoration: none; }\n"
			"</STYLE>\n"
			"</HEAD>\n");
		do_template("background");
	}

	do_template("menubar");

	if (as_single_page) {
		wDumpContent(2);
	}


}
