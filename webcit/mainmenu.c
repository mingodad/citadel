#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include "webcit.h"


/* 
 * menu of commands (just the menu html itself)
 */

void embed_main_menu() {
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
	
	/* ungoto not supported yet
	if ( (strlen(ugname)>0) && (strucmp(ugname,room_name)) ) {
		wprintf("<LI><B><A HREF=\"/ungoto\">\n");
		wprintf("Ungoto</B></A><BR>\n");
		wprintf("(oops! Back to %s)</LI>\n",ugname);
		}
 	*/
	
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
 * menu of commands (as a page)
 */
void display_main_menu() {
	printf("HTTP/1.0 200 OK\n");
	output_headers();
	wprintf("<HTML><HEAD><TITLE>WebCit main menu</TITLE></HEAD><BODY>\n");
	embed_main_menu();
	wprintf("</BODY></HTML>\n");
	wDumpContent();
	}
