/*
 * webcit.c
 *
 * This is the actual program called by the webserver.  It maintains a
 * persistent session to the Citadel server, handling HTTP WebCit requests as
 * they arrive and presenting a user interface.
 *
 * $Id$
 */

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdarg.h>
#include "webcit.h"
#include "child.h"

int wc_session;
char wc_host[256];
char wc_port[256];
char wc_username[256];
char wc_password[256];
char wc_roomname[256];
int TransactionCount = 0;
int connected = 0;
int logged_in = 0;
int axlevel;
char *ExpressMessages = NULL;

struct webcontent *wlist = NULL;
struct webcontent *wlast = NULL;

struct urlcontent *urlstrings = NULL;

static const char *defaulthost = DEFAULT_HOST;
static const char *defaultport = DEFAULT_PORT;

int upload_length = 0;
char *upload;


void unescape_input(char *buf)
{
	int a,b;
	char hex[3];

	while ((isspace(buf[strlen(buf)-1]))&&(strlen(buf)>0))
		buf[strlen(buf)-1] = 0;

	for (a=0; a<strlen(buf); ++a) {
		if (buf[a]=='+') buf[a]=' ';	
		if (buf[a]=='%') {
			hex[0]=buf[a+1];
			hex[1]=buf[a+2];
			hex[2]=0;
			sscanf(hex,"%02x",&b);
			buf[a] = (char) b;
			strcpy(&buf[a+1],&buf[a+3]);
			}
		}

	}


void addurls(char *url) {
	char *up, *ptr;
	char buf[256];
	int a,b;
	struct urlcontent *u;

	up = url;
	while (strlen(up)>0) {
		
		/* locate the = sign */
		strncpy(buf,up,255);
		b = (-1);
		for (a=255; a>=0; --a) if (buf[a]=='=') b=a;
		if (b<0) return;
		buf[b]=0;
	
		u = (struct urlcontent *)malloc(sizeof(struct urlcontent));
		u->next = urlstrings;
		urlstrings = u;
		strcpy(u->url_key, buf);
	
		/* now chop that part off */
		for (a=0; a<=b; ++a) ++up;
	
		/* locate the & sign */
		ptr = up;
		b = strlen(up);
		for (a=0; a<strlen(up); ++a) {
			if (!strncmp(ptr,"&",1)) {
				b=a;
				break;
				}
			++ptr;
			}
		ptr = up;
		for (a=0; a<b; ++a) ++ptr;
		strcpy(ptr,"");
		
		u->url_data = malloc(strlen(up)+1);
		strcpy(u->url_data, up);
		u->url_data[b] = 0;
		unescape_input(u->url_data);
		up = ptr;
		++up;
		}
	}

void free_urls(void) {
	struct urlcontent *u;

	while (urlstrings != NULL) {
		free(urlstrings->url_data);
		u = urlstrings->next;
		free(urlstrings);
		urlstrings = u;
		}
	}

/*
 * Diagnostic function to display the contents of all variables
 */
void dump_vars(void) {
	struct urlcontent *u;

	for (u = urlstrings; u != NULL; u = u->next) {
		wprintf("%38s = %s\n", u->url_key, u->url_data);
		}
	}

char *bstr(char *key) {
	struct urlcontent *u;

	for (u = urlstrings; u != NULL; u = u->next) {
		if (!strcasecmp(u->url_key, key)) return(u->url_data);
		}
	return("");
	}


void wprintf(const char *format, ...) {   
        va_list arg_ptr;   
	struct webcontent *wptr;

	wptr = (struct webcontent *)malloc(sizeof(struct webcontent));
	wptr->next = NULL;
	if (wlist == NULL) {
		wlist = wptr;
		wlast = wptr;
		}
	else {
		wlast->next = wptr;
		wlast = wptr;
		}
  
       	va_start(arg_ptr, format);   
       	vsprintf(wptr->w_data, format, arg_ptr);   
       	va_end(arg_ptr);   
	}

int wContentLength(void) {
	struct webcontent *wptr;
	int len = 0;

	for (wptr = wlist; wptr != NULL; wptr = wptr->next) {
		len = len + strlen(wptr->w_data);
		}

	return(len);
	}

void wDumpContent(void) {
	struct webcontent *wptr;

	printf("Content-type: text/html\n");
	printf("Content-length: %d\n", wContentLength());
	printf("\n");

	while (wlist != NULL) {
		fwrite(wlist->w_data, strlen(wlist->w_data), 1, stdout);
		wptr = wlist->next;
		free(wlist);
		wlist = wptr;
		}
	wlast = NULL;
	}


void escputs1(char *strbuf, int nbsp)
{
	int a;

	for (a=0; a<strlen(strbuf); ++a) {
		if (strbuf[a]=='<') wprintf("&lt;");
		else if (strbuf[a]=='>') wprintf("&gt;");
		else if (strbuf[a]=='&') wprintf("&amp;");
		else if (strbuf[a]==34) wprintf("&quot;");
		else if (strbuf[a]==LB) wprintf("<");
		else if (strbuf[a]==RB) wprintf(">");
		else if (strbuf[a]==QU) wprintf("\"");
		else if ((strbuf[a]==32)&&(nbsp==1)) {
			wprintf("&nbsp;");
			}
		else {
			wprintf("%c", strbuf[a]);
			}
		}
	}

void escputs(char *strbuf)
{
	escputs1(strbuf,0);
	}



char *urlesc(char *strbuf)
{
	int a,b,c;
        char *ec = " #&;`'|*?-~<>^()[]{}$\\";
	static char outbuf[512];
	
	strcpy(outbuf,"");

	for (a=0; a<strlen(strbuf); ++a) {
		c = 0;
		for (b=0; b<strlen(ec); ++b) {
			if (strbuf[a]==ec[b]) c=1;
			}
		b = strlen(outbuf);
		if (c==1) sprintf(&outbuf[b],"%%%02x",strbuf[a]);
		else sprintf(&outbuf[b],"%c",strbuf[a]);
		}
	return(outbuf);
	}

void urlescputs(char *strbuf)
{
	wprintf("%s",urlesc(strbuf));
	}


void getz(char *buf) {
	bzero(buf, 256);
	if (fgets(buf, 256, stdin) == NULL) strcpy(buf, "");
	else {
		while ((strlen(buf)>0)&&(!isprint(buf[strlen(buf)-1])))
			buf[strlen(buf)-1] = 0;
		}
	}

/*
 * Output all that important stuff that the browser will want to see
 */
void output_headers(int print_standard_html_head) {

	static char *unset = "; expires=28-May-1971 18:10:00 GMT";

	printf("Server: %s\n", SERVER);
	printf("Connection: close\n");
	printf("Set-cookie: wc_session=%d\n", wc_session);

	if (strlen(wc_host)>0) printf("Set-cookie: wc_host=%s\n", wc_host);
	else printf("Set-cookie: wc_host=%s\n", unset);

	if (strlen(wc_port)>0) printf("Set-cookie: wc_port=%s\n", wc_port);
	else printf("Set-cookie: wc_port=%s\n", unset);

	if (strlen(wc_username)>0) printf("Set-cookie: wc_username=%s\n",
		wc_username);
	else printf("Set-cookie: wc_username=%s\n", unset);

	if (strlen(wc_password)>0) printf("Set-cookie: wc_password=%s\n",
		wc_password);
	else printf("Set-cookie: wc_password=%s\n", unset);

	if (strlen(wc_roomname)>0) printf("Set-cookie: wc_roomname=%s\n",
		wc_roomname);
	else printf("Set-cookie: wc_roomname=%s\n", unset);

	if (print_standard_html_head) {
        	wprintf("<HTML><HEAD><TITLE>");
		escputs("WebCit");	 /* FIX -- add BBS name here */
		wprintf("</TITLE></HEAD>");
		if (ExpressMessages != NULL) {
			wprintf("<SCRIPT language=\"javascript\">\n");
			wprintf("function ExpressMessage() {\n");
			wprintf(" alert(\"");
			escputs(ExpressMessages);
			wprintf("\")\n");
			wprintf(" }\n </SCRIPT>\n");
			}
		wprintf("<BODY ");
		if (ExpressMessages != NULL) {
			wprintf("onload=\"ExpressMessage()\" ");
			free(ExpressMessages);
			ExpressMessages = NULL;
			}
		wprintf("BACKGROUND=\"/image&name=background\" TEXT=\"#000000\" LINK=\"#004400\">\n");
		}

	}




void check_for_express_messages() {
	char buf[256];

	serv_puts("PEXP");
	serv_gets(buf);
	if (buf[0]=='1') {
		while (serv_gets(buf), strcmp(buf, "000")) {
			if (ExpressMessages == NULL) {
				ExpressMessages = malloc(strlen(buf) + 4);
				strcpy(ExpressMessages, "");
				}
			else {
				ExpressMessages = realloc(ExpressMessages,
				 (strlen(ExpressMessages) + strlen(buf) + 4) );
				}
			strcat(ExpressMessages, buf);
			strcat(ExpressMessages, "\\n");
			}
		}
	}




void output_static(char *what) {
	char buf[256];
	FILE *fp;
	struct stat statbuf;
	off_t bytes;

	sprintf(buf, "static/%s", what);
	fp = fopen(buf, "rb");
	if (fp == NULL) {
		printf("HTTP/1.0 404 %s\n", strerror(errno));
		output_headers(0);
		printf("Content-Type: text/plain\n");
		sprintf(buf, "%s: %s\n", what, strerror(errno));
		printf("Content-length: %d\n", strlen(buf));
		printf("\n");
		fwrite(buf, strlen(buf), 1, stdout);
		}
	else {
		printf("HTTP/1.0 200 OK\n");
		output_headers(0);

		if (!strncasecmp(&what[strlen(what)-4], ".gif", 4))
			printf("Content-type: image/gif\n");
		else if (!strncasecmp(&what[strlen(what)-4], ".jpg", 4))
			printf("Content-type: image/jpeg\n");
		else if (!strncasecmp(&what[strlen(what)-5], ".html", 5))
			printf("Content-type: text/html\n");
		else
			printf("Content-type: application/octet-stream\n");

		fstat(fileno(fp), &statbuf);
		bytes = statbuf.st_size;
		printf("Content-length: %ld\n", (long)bytes);
		printf("\n");
		while (bytes--) {
			putc(getc(fp), stdout);
			}
		fflush(stdout);
		fclose(fp);
		}
	}

void output_image() {
	char buf[256];
	char xferbuf[4096];
	off_t bytes;
	off_t thisblock;
	off_t accomplished = 0L;


	serv_printf("OIMG %s|%s", bstr("name"), bstr("parm"));
	serv_gets(buf);
	if (buf[0]=='2') {
		bytes = extract_long(&buf[4], 0);
		printf("HTTP/1.0 200 OK\n");
		output_headers(0);
		printf("Content-type: image/gif\n");
		printf("Content-length: %ld\n", bytes);
		printf("\n");

		while (bytes > (off_t)0) {
			thisblock = (off_t)sizeof(xferbuf);
			if (thisblock > bytes) thisblock = bytes;
			serv_printf("READ %ld|%ld", accomplished, thisblock);
			serv_gets(buf);
			if (buf[0]=='6') thisblock = extract_long(&buf[4],0);
			serv_read(xferbuf, (int)thisblock);
			fwrite(xferbuf, thisblock, 1, stdout);
			bytes = bytes - thisblock;
			accomplished = accomplished + thisblock;
			}
		fflush(stdout);
		serv_puts("CLOS");
		serv_gets(buf);
		}
	else {
		printf("HTTP/1.0 404 %s\n", strerror(errno));
		output_headers(0);
		printf("Content-Type: text/plain\n");
		sprintf(buf, "Error retrieving image\n");
		printf("Content-length: %d\n", strlen(buf));
		printf("\n");
		fwrite(buf, strlen(buf), 1, stdout);
		}

	}


/*
 * Convenience functions to display a page containing only a string
 */
void convenience_page(char *titlebarcolor, char *titlebarmsg, char *messagetext) {
        printf("HTTP/1.0 200 OK\n");
        output_headers(1);
        wprintf("<TABLE WIDTH=100% BORDER=0 BGCOLOR=%s><TR><TD>", titlebarcolor);
        wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"");
        wprintf("<B>%s</B>\n", titlebarmsg);
        wprintf("</FONT></TD></TR></TABLE><BR>\n");
	escputs(messagetext);
        wprintf("</BODY></HTML>\n");
        wDumpContent();
	}

void display_error(char *errormessage) {
	convenience_page("770000", "Error", errormessage);
	}

void display_success(char *successmessage) {
	convenience_page("007700", "OK", successmessage);
	}



void extract_action(char *actbuf, char *cmdbuf) {
	int i;

	strcpy(actbuf, cmdbuf);
	if (!strncasecmp(actbuf, "GET /", 5)) strcpy(actbuf, &actbuf[5]);
	if (!strncasecmp(actbuf, "PUT /", 5)) strcpy(actbuf, &actbuf[5]);
	if (!strncasecmp(actbuf, "POST /", 6)) strcpy(actbuf, &actbuf[6]);

	for (i=0; i<strlen(actbuf); ++i) {
		if (actbuf[i]==' ') { actbuf[i]=0; i=0; }
		if (actbuf[i]=='/') { actbuf[i]=0; i=0; }
		if (actbuf[i]=='?') { actbuf[i]=0; i=0; }
		if (actbuf[i]=='&') { actbuf[i]=0; i=0; }
		}
	}


void session_loop(void) {
	char cmd[256];
	char action[256];
	char buf[256];
	int a, b;
	int ContentLength = 0;
	char ContentType[512];
	char *content;

	/* We stuff these with the values coming from the client cookies,
	 * so we can use them to reconnect a timed out session if we have to.
	 */
	char c_host[256];
	char c_port[256];
	char c_username[256];
	char c_password[256];
	char c_roomname[256];

	strcpy(c_host, defaulthost);
	strcpy(c_port, defaultport);
	strcpy(c_username, "");
	strcpy(c_password, "");
	strcpy(c_roomname, "");

	upload_length = 0;
	upload = NULL;

	getz(cmd);
	extract_action(action, cmd);

	do {
		getz(buf);

		if (!strncasecmp(buf, "Cookie: wc_host=", 16))
			strcpy(c_host, &buf[16]);
		if (!strncasecmp(buf, "Cookie: wc_port=", 16))
			strcpy(c_port, &buf[16]);
		if (!strncasecmp(buf, "Cookie: wc_username=", 20))
			strcpy(c_username, &buf[20]);
		if (!strncasecmp(buf, "Cookie: wc_password=", 20))
			strcpy(c_password, &buf[20]);
		if (!strncasecmp(buf, "Cookie: wc_roomname=", 20))
			strcpy(c_roomname, &buf[20]);
		if (!strncasecmp(buf, "Content-length: ", 16)) {
			ContentLength = atoi(&buf[16]);
			}
		if (!strncasecmp(buf, "Content-type: ", 14)) {
			strcpy(ContentType, &buf[14]);
			}
		} while(strlen(buf)>0);

	++TransactionCount;

	if (ContentLength > 0) {
		content = malloc(ContentLength+1);
		fread(content, ContentLength, 1, stdin);

		content[ContentLength] = 0;

		if (!strncasecmp(ContentType,
		   "application/x-www-form-urlencoded", 33)) {
			addurls(content);
			}
		else if (!strncasecmp(ContentType, "multipart", 9)) {
			mime_parser(content, ContentLength, ContentType);
			}
		}
	else {
		content = NULL;
		}

	/* If there are variables in the URL, we must grab them now */	
	for (a=0; a<strlen(cmd); ++a) if ((cmd[a]=='?')||(cmd[a]=='&')) {
		for (b=a; b<strlen(cmd); ++b) if (isspace(cmd[b])) cmd[b]=0;
		addurls(&cmd[a+1]);
		cmd[a] = 0;
		}

	/*
	 * If we're not connected to a Citadel server, try to hook up the
	 * connection now.  Preference is given to the host and port specified
	 * by browser cookies, if cookies have been supplied.
	 */
	if (!connected) {
		if (strlen(bstr("host"))>0) strcpy(c_host, bstr("host"));
		if (strlen(bstr("port"))>0) strcpy(c_port, bstr("port"));
		serv_sock = connectsock(c_host, c_port, "tcp");
		connected = 1;
		serv_gets(buf);	/* get the server welcome message */
		strcpy(wc_host, c_host);
		strcpy(wc_port, c_port);
		get_serv_info();
		}

	check_for_express_messages();

	/*
	 * If we're not logged in, but we have username and password cookies
	 * supplied by the browser, try using them to log in.
	 */
	if ((!logged_in)&&(strlen(c_username)>0)&&(strlen(c_password)>0)) {
		serv_printf("USER %s", c_username);
		serv_gets(buf);
		if (buf[0]=='3') {
			serv_printf("PASS %s", c_password);
			serv_gets(buf);
			if (buf[0]=='2') {
				become_logged_in(c_username, c_password, buf);
				}
			}
		}

	/*
	 * If we don't have a current room, but a cookie specifying the
	 * current room is supplied, make an effort to go there.
	 */
	if ((strlen(wc_roomname)==0) && (strlen(c_roomname)>0) ) {
		serv_printf("GOTO %s", c_roomname);
		serv_gets(buf);
		if (buf[0]=='2') {
			strcpy(wc_roomname, c_roomname);
			}
		}

	if (!strcasecmp(action, "static")) {
		strcpy(buf, &cmd[12]);
		for (a=0; a<strlen(buf); ++a) if (isspace(buf[a])) buf[a]=0;
		output_static(buf);
		}

	else if (!strcasecmp(action, "image")) {
		output_image();
		}

	else if ((!logged_in)&&(!strcasecmp(action, "login"))) {
		do_login();
		}

	else if (!logged_in) {
		display_login(NULL);
		}

	/* Various commands... */
	
	else if (!strcasecmp(action, "do_welcome")) {
		do_welcome();
		}

	else if (!strcasecmp(action, "display_main_menu")) {
		display_main_menu();
		}

	else if (!strcasecmp(action, "advanced")) {
		display_advanced_menu();
		}

	else if (!strcasecmp(action, "whobbs")) {
		whobbs();
		}

	else if (!strcasecmp(action, "knrooms")) {
		list_all_rooms_by_floor();
		}

	else if (!strcasecmp(action, "gotonext")) {
		slrp_highest();
		gotonext();
		}

	else if (!strcasecmp(action, "skip")) {
		gotonext();
		}

	else if (!strcasecmp(action, "ungoto")) {
		ungoto();
		}

	else if (!strcasecmp(action, "dotgoto")) {
		slrp_highest();
		gotoroom(bstr("room"), 1);
		}

	else if (!strcasecmp(action, "termquit")) {
		do_logout();
		}

	else if (!strcasecmp(action, "readnew")) {
		readloop("readnew");
		}

	else if (!strcasecmp(action, "readold")) {
		readloop("readold");
		}

	else if (!strcasecmp(action, "readfwd")) {
		readloop("readfwd");
		}

	else if (!strcasecmp(action, "display_enter")) {
		display_enter();
		}

	else if (!strcasecmp(action, "post")) {
		post_message();
		}

	else if (!strcasecmp(action, "confirm_delete_msg")) {
		confirm_delete_msg();
		}

	else if (!strcasecmp(action, "delete_msg")) {
		delete_msg();
		}

	else if (!strcasecmp(action, "confirm_move_msg")) {
		confirm_move_msg();
		}

	else if (!strcasecmp(action, "move_msg")) {
		move_msg();
		}

	else if (!strcasecmp(action, "userlist")) {
		userlist();
		}

	else if (!strcasecmp(action, "showuser")) {
		showuser();
		}

	else if (!strcasecmp(action, "display_page")) {
		display_page();
		}

	else if (!strcasecmp(action, "page_user")) {
		page_user();
		}

	else if (!strcasecmp(action, "chat")) {
		do_chat();
		}

	else if (!strcasecmp(action, "display_private")) {
		display_private("", 0);
		}

	else if (!strcasecmp(action, "goto_private")) {
		goto_private();
		}

	else if (!strcasecmp(action, "zapped_list")) {
		zapped_list();
		}

	else if (!strcasecmp(action, "display_zap")) {
		display_zap();
		}

	else if (!strcasecmp(action, "zap")) {
		zap();
		}

	else if (!strcasecmp(action, "display_entroom")) {
		display_entroom();
		}

	else if (!strcasecmp(action, "entroom")) {
		entroom();
		}

	else if (!strcasecmp(action, "display_editroom")) {
		display_editroom();
		}

	else if (!strcasecmp(action, "editroom")) {
		editroom();
		}

	else if (!strcasecmp(action, "display_editinfo")) {
		display_edit("Room info", "EINF 0", "RINF", "/editinfo");
		}

	else if (!strcasecmp(action, "editinfo")) {
		save_edit("Room info", "EINF 1", 1);
		}

	else if (!strcasecmp(action, "display_editbio")) {
		sprintf(buf, "RBIO %s", wc_username);
		display_edit("Your bio", "NOOP", buf, "editbio");
		}

	else if (!strcasecmp(action, "editbio")) {
		save_edit("Your bio", "EBIO", 0);
		}

	else if (!strcasecmp(action, "confirm_delete_room")) {
		confirm_delete_room();
		}

	else if (!strcasecmp(action, "delete_room")) {
		delete_room();
		}

	else if (!strcasecmp(action, "validate")) {
		validate();
		}

	else if (!strcasecmp(action, "display_editpic")) {
		display_graphics_upload("your photo",
					"UIMG 0|_userpic_",
					"/editpic");
		}

	else if (!strcasecmp(action, "editpic")) {
		do_graphics_upload("UIMG 1|_userpic_");
		}

	else if (!strcasecmp(action, "display_editroompic")) {
		display_graphics_upload("the graphic for this room",
					"UIMG 0|_roompic_",
					"/editroompic");
		}

	else if (!strcasecmp(action, "editroompic")) {
		do_graphics_upload("UIMG 1|_roompic_");
		}

	else if (!strcasecmp(action, "select_floor_to_edit_pic")) {
		select_floor_to_edit_pic();
		}

	else if (!strcasecmp(action, "display_editfloorpic")) {
		sprintf(buf, "UIMG 0|_floorpic_|%s",
			bstr("which_floor"));
		display_graphics_upload("the graphic for this floor",
					buf,
					"/editfloorpic");
		}

	else if (!strcasecmp(action, "editfloorpic")) {
		sprintf(buf, "UIMG 1|_floorpic_|%s",
			bstr("which_floor"));
		do_graphics_upload(buf);
		}

	else if (!strcasecmp(action, "display_reg")) {
		display_reg(0);
		}

	else if (!strcasecmp(action, "register")) {
		register_user();
		}

	else if (!strcasecmp(action, "display_changepw")) {
		display_changepw();
		}

	else if (!strcasecmp(action, "changepw")) {
		changepw();
		}

	/* When all else fails... */
	else {
		printf("HTTP/1.0 200 OK\n");
		output_headers(1);
	
		wprintf("TransactionCount is %d<BR>\n", TransactionCount);
		wprintf("You're in session %d<HR>\n", wc_session);
		wprintf("Command: <BR><PRE>\n");
		escputs(cmd);
		wprintf("</PRE><HR>\n");
		wprintf("Variables: <BR><PRE>\n");
		dump_vars();
		wprintf("</PRE><HR>\n");
		wprintf("</BODY></HTML>\n");
		wDumpContent();
		}

	fflush(stdout);
	if (content != NULL) {
		free(content);
		content = NULL;
		}
	free_urls();
	if (upload_length > 0) {
		free(upload);
		upload_length = 0;
		}
	}

int main(int argc, char *argv[]) {

	if (argc < 2 || argc > 4) {
		fprintf(stderr,
			"webcit: usage: webcit <session_id> [host [port]]\n");
		return 1;
		}

	wc_session = atoi(argv[1]);

	if (argc > 2) {
		defaulthost = argv[2];
		if (argc > 3)
			defaultport = argv[3];
		}

	strcpy(wc_host, "");
	strcpy(wc_port, "");
	strcpy(wc_username, "");
	strcpy(wc_password, "");
	strcpy(wc_roomname, "");

	while (1) {
		session_loop();
		}
	}
