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
	
	wprintf("<LI><B><A HREF=\"/gotonext\" TARGET=\"top\">\n");
	wprintf("Goto next room</B></A><BR>\n");
	wprintf("...with <EM>unread</EM> messages</LI>\n");
	
	wprintf("<LI><B><A HREF=\"/skip\" TARGET=\"top\">\n");
	wprintf("Skip to next room</B></A><BR>\n");
	wprintf("(come back here later)</LI>\n");
	
	if ( (strlen(ugname)>0) && (strcasecmp(ugname,wc_roomname)) ) {
		wprintf("<LI><B><A HREF=\"/ungoto\" TARGET=\"top\">\n");
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

	wprintf("<LI><B><A HREF=\"/termquit\" TARGET=\"_top\">\n");
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

	if (axlevel>=6) {
		wprintf("<LI><A HREF=\"/validate\">\n");
		wprintf("Validate new users</A>\n");

		wprintf("<LI><A HREF=\"/display_editfloorpic\">\n");
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
	output_headers(1);
	embed_main_menu();
	wprintf("</BODY></HTML>\n");
	wDumpContent();
	}


void display_advanced_menu(void) {
	printf("HTTP/1.0 200 OK\n");
	output_headers(1);
	embed_advanced_menu();
	embed_main_menu();
	wprintf("</BODY></HTML>\n");
	wDumpContent();
	}
