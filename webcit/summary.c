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
#include <time.h>
#include <pwd.h>
#include <errno.h>
#include <stdarg.h>
#include <pthread.h>
#include <signal.h>
#include "webcit.h"

/*
 * Display today's date in a friendly format
 */
void output_date(void) {
	struct tm tm;
	time_t now;

	static char *wdays[] = {
		"Sunday", "Monday", "Tuesday", "Wednesday",
		"Thursday", "Friday", "Saturday"
	};
	static char *months[] = {
		"January", "February", "March", "April", "May", "June", "July",
		"August", "September", "October", "November", "December"
	};

	time(&now);
	localtime_r(&now, &tm);

	wprintf("%s, %s %d, %d",
		wdays[tm.tm_wday],
		months[tm.tm_mon],
		tm.tm_mday,
		tm.tm_year + 1900
	);
}



/*
 * Display the title bar for a section
 */
void section_title(char *title) {

	wprintf("<TABLE width=100%% border=0 cellpadding=5 cellspacing=0>"
		"<TR><TD BGCOLOR=444455>"
		"<FONT COLOR=FFFFEE>");
	escputs(title);
	wprintf("</FONT></TD></TR></TABLE>\n");
}


/*
 * Dummy section
 */
void dummy_section(void) {
	section_title("---");
}


/*
 * New messages section
 */
void new_messages_section(void) {
	char buf[SIZ];
	char room[SIZ];
	int i;
	int number_of_rooms_to_check;
	char *rooms_to_check = "Mail|Lobby";

	section_title("Messages");

	number_of_rooms_to_check = num_tokens(rooms_to_check, '|');
	if (number_of_rooms_to_check == 0) return;

	wprintf("<TABLE BORDER=0 WIDTH=100%%>\n");
	for (i=0; i<number_of_rooms_to_check; ++i) {
		extract(room, rooms_to_check, i);

		serv_printf("GOTO %s", room);
		serv_gets(buf);
		if (buf[0] == '2') {
			extract(room, &buf[4], 0);
			wprintf("<TR><TD><A HREF=\"/dotgoto?room=");
			urlescputs(room);
			wprintf("\">");
			escputs(room);
			wprintf("</A></TD><TD>%d/%d</TD></TR>\n",
				extract_int(&buf[4], 1),
				extract_int(&buf[4], 2)
			);
		}
	}
	wprintf("</TABLE>\n");

}


/*
 * Wholist section
 */
void wholist_section(void) {
	char buf[SIZ];
	char user[SIZ];

	section_title("Who's online now");
	serv_puts("RWHO");
	serv_gets(buf);
	if (buf[0] == '1') while(serv_gets(buf), strcmp(buf, "000")) {
		extract(user, buf, 1);
		escputs(user);
		wprintf("<BR>\n");
	}
}


/*
 * Server info section (fluff, really)
 */
void server_info_section(void) {
	char buf[SIZ];
	int i = 0;

	section_title("About this server");
	serv_puts("INFO");
	serv_gets(buf);
	if (buf[0] == '1') while(serv_gets(buf), strcmp(buf, "000")) {
		switch(i) {
			case 2:
				wprintf("You are connected to ");
				escputs(buf);
				wprintf(", ");
				break;
			case 4: wprintf("running ");
				escputs(buf);
				wprintf(", ");
				break;
			case 6: wprintf("and located in ");
				escputs(buf);
				wprintf(".<BR>\n");
				break;
			case 7: wprintf("Your system administrator is ");
				escputs(buf);
				wprintf(".\n");
				break;
		}
		++i;
	}
}
	



/*
 * Display this user's summary page
 */
void summary(void) {
	output_headers(7);

	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=007700><TR><TD>"
		"<FONT SIZE=+1 COLOR=\"FFFFFF\""
		"<B>Summary page for ");
	escputs(WC->wc_username);
	wprintf("</B><FONT></TD><TD>\n");
	offer_start_page();
	wprintf("</TD></TR></TABLE>\n");

	wprintf("<DIV ALIGN=RIGHT>");
	output_date();
	wprintf("</DIV>\n");

	/*
	 * Now let's do three columns of crap.  All portals and all groupware
	 * clients seem to want to do three columns, so we'll do three
	 * columns too.  Conformity is not inherently a virtue, but there are
	 * a lot of really shallow people out there, and even though they're
	 * not people I consider worthwhile, I still want them to use WebCit.
	 */

	wprintf("<TABLE WIDTH=100%% BORDER=0 CELLPADDING=10><TR VALIGN=TOP>");

	/*
	 * Column One
	 */
	wprintf("<TD>");
	wholist_section();

	/*
	 * Column Two
	 */
	wprintf("</TD><TD>");
	server_info_section();

	/*
	 * Column Three
	 */
	wprintf("</TD><TD>");
	new_messages_section();

	/*
	 * End of columns
	 */
	wprintf("</TD></TR></TABLE>\n");

	wDumpContent(1);
}
