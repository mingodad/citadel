/* $Id$ */

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include "webcit.h"
#include "child.h"


/* 
 * menu of commands (just the menu html itself)
 */

void embed_main_menu(void) {
	wprintf("<CENTER><TABLE border=0><TR>");

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
	
	if ( (strlen(ugname)>0) && (strcasecmp(ugname,wc_roomname)) ) {
		wprintf("<LI><B><A HREF=\"/ungoto\">\n");
		wprintf("Ungoto</B></A><BR>\n");
		wprintf("(oops! Back to %s)</LI>\n",ugname);
		}
	
	wprintf("</UL>\n");
	
	wprintf("</TD><TD>\n"); /* start of second column */
	
	wprintf("<UL>");
	wprintf("<LI><B><A HREF=\"/readnew\">\n");
	wprintf("Read new messages</B></A><BR>...in this room</LI>\n");
	
	wprintf("<LI><B><A HREF=\"/readfwd\">\n");
	wprintf("Read all messages</B></A><BR>...old <EM>and</EM> new</LI>\n");

	wprintf("<LI><B><A HREF=\"/display_enter\">\n");
	wprintf("Enter a message</B></A><BR>(post in this room)</LI>");
	wprintf("</UL>\n");

	wprintf("</TD><TD>"); /* start of third column */

	wprintf("<UL>");
	wprintf("<LI><B><A HREF=\"/whobbs\">\n");
	wprintf("Who is online?</B></A><BR>(users <EM>currently</EM> logged on)</LI>\n");
	
	wprintf("<LI><B><A HREF=\"/userlist\">\n");
	wprintf("User list</B></A><BR>(all registered users)</LI>\n");

	wprintf("<LI><B><A HREF=\"/advanced\">\n");
	wprintf("Advanced options</B></A><BR>...and maintenance</LI>\n");

	wprintf("<LI><B><A HREF=\"/termquit\">\n");
	wprintf("Log off</B></A><BR>Bye!</LI>\n");
	wprintf("</UL>\n");

	wprintf("</TR></TABLE>\n");

	wprintf("</CENTER>\n");
	}

/*
 * advanced options
 */
void embed_advanced_menu(void) {

wprintf("<TABLE WIDTH=100%><TR VALIGN=TOP><TD>");


wprintf("<TABLE WIDTH=100% BORDER=0 BGCOLOR=770000><TR><TD>");
wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"");
wprintf("<B>Interaction</B>\n");
wprintf("</FONT></TD></TR></TABLE>\n");

wprintf("<UL>");
wprintf("<LI><A HREF=\"/display_page\">\n");
wprintf("Page another user</A>\n");

wprintf("<LI><A HREF=\"/chat\">");
wprintf("Chat with other online users</A>\n");

wprintf("<LI><A HREF=\"/display_generic\">\n");
wprintf("<FONT SIZE=-2>Generic server command</FONT></A>\n");

wprintf("</UL>\n");

wprintf("</TD><TD>");

wprintf("<TABLE WIDTH=100% BORDER=0 BGCOLOR=007700><TR><TD>");
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

wprintf("<TABLE WIDTH=100% BORDER=0 BGCOLOR=000077><TR><TD>");
wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"");
wprintf("<B>Advanced room commands</B>\n");
wprintf("</FONT></TD></TR></TABLE>\n");

wprintf("<UL>");
wprintf("<LI><A HREF=\"/display_private\">\n");
wprintf("Go to a 'hidden' room</A>\n");

wprintf("<LI><A HREF=\"/display_entroom\">");
wprintf("Create a new room</A>\n");

wprintf("<LI><A HREF=\"/display_zap\">");
wprintf("Zap (forget) this room (%s)</A>\n", wc_roomname);

wprintf("<LI><A HREF=\"/zapped_list\">");
wprintf("List all forgotten rooms</A>\n");

wprintf("</UL>\n");

wprintf("</TD><TD>");

if ((axlevel>=6) || (is_room_aide)) {
	wprintf("<TABLE WIDTH=100% BORDER=0 BGCOLOR=007777><TR><TD>");
	wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"");
	wprintf("<B>Administrative functions</B>\n");
	wprintf("</FONT></TD></TR></TABLE>\n");

	wprintf("<UL>");
	wprintf("<LI><A HREF=\"/display_editroom\">\n");
	wprintf("Edit this room</A>\n");
	
	wprintf("<LI><A HREF=\"/confirm_delete_room\">\n");
	wprintf("Delete this room</A>\n");
	
	wprintf("<LI><A HREF=\"/display_editroompic\">\n");
	wprintf("Set or change the graphic for this room's banner</A>\n");

	wprintf("<LI><A HREF=\"/display_editinfo\">\n");
	wprintf("Edit this room's Info file</A>\n");

	wprintf("<LI><A HREF=\"/display_siteconfig\">\n");
	wprintf("Edit site-wide configuration</A>\n");

	if (axlevel>=6) {
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

wprintf("<TABLE WIDTH=100% BORDER=0 BGCOLOR=770077><TR><TD>");
wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"");
wprintf("<B>Basic commands</B>\n");
wprintf("</FONT></TD></TR></TABLE>\n");

	}




/*
 * menu of commands (as a page)
 */
void display_main_menu(void) {
	printf("HTTP/1.0 200 OK\n");
	output_headers(1, "bottom");
	embed_main_menu();
	wDumpContent(1);
	}


void display_advanced_menu(void) {
	printf("HTTP/1.0 200 OK\n");
	output_headers(1, "bottom");
	embed_advanced_menu();
	embed_main_menu();
	wDumpContent(1);
	}


/*
 * Display the screen to enter a generic server command
 */
void display_generic(void) {
	printf("HTTP/1.0 200 OK\n");
	output_headers(1, "bottom");
	
	wprintf("<TABLE WIDTH=100% BORDER=0 BGCOLOR=770077><TR><TD>");
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

void do_generic(void) {
	char buf[256];
	char gcontent[256];
	char *junk;
	size_t len;

	if (strcasecmp(bstr("sc"), "Send command")) {
		display_main_menu();
		return;
		}

	serv_printf("%s", bstr("g_cmd"));
	serv_gets(buf);

	printf("HTTP/1.0 200 OK\n");
	output_headers(1, "bottom");
	wprintf("<TABLE WIDTH=100% BORDER=0 BGCOLOR=770077><TR><TD>");
	wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"");
	wprintf("<B>Server command results</B>\n");
	wprintf("</FONT></TD></TR></TABLE>\n");

	wprintf("<TABLE border=0><TR><TD>Command:</TD><TD><TT>");
	escputs(bstr("g_cmd"));
	wprintf("</TT></TD></TR><TR><TD>Result:</TD><TD><TT>");
	escputs(buf);
	wprintf("</TT></TD></TR></TABLE><BR>\n");

	if (buf[0]=='8') {
		serv_printf("\n\n000");
		}

	if ( (buf[0]=='1') || (buf[0]=='8') ) {
		while(serv_gets(gcontent), strcmp(gcontent, "000")) {
			escputs(gcontent);
			wprintf("<BR>\n");
			}
		wprintf("000");
		}

	if (buf[0]=='4') {
		text_to_server(bstr("g_input"));
		serv_puts("000");
		}

	if (buf[0]=='6') {
		len = atol(&buf[4]);
		junk = malloc(len);
		serv_read(junk, len);
		free(junk);
		}

	if (buf[0]=='7') {
		len = atol(&buf[4]);
		junk = malloc(len);
		bzero(junk, len);
		serv_write(junk, len);
		free(junk);
		}

	wprintf("<HR>");
	wprintf("<A HREF=\"/display_generic\">Enter another command</A><BR>\n");
	wprintf("<A HREF=\"/display_advanced\">Return to menu</A>\n");
	wDumpContent(1);
	}
