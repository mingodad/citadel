/* $Id$ */

#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include "webcit.h"
#include "child.h"

struct march {
	struct march *next;
	char march_name[32];
	};

char floorlist[128][256];
char ugname[128];
long uglsn = (-1L);
unsigned room_flags;
int is_aide = 0;
int is_room_aide = 0;

struct march *march = NULL;

/*
 * load the list of floors
 */
void load_floorlist(void) {
	int a;
	char buf[256];

	for (a=0; a<128; ++a) floorlist[a][0] = 0;

	serv_puts("LFLR");
	serv_gets(buf);
	if (buf[0]!='1') {
		strcpy(floorlist[0],"Main Floor");
		return;
		}
	while (serv_gets(buf), strcmp(buf,"000")) {
		extract(floorlist[extract_int(buf,0)],buf,1);
		}
	}



/*
 * remove a room from the march list
 */
void remove_march(char *aaa)
{
	struct march *mptr,*mptr2;

	if (march==NULL) return;

	if (!strcasecmp(march->march_name,aaa)) {
		mptr = march->next;
		free(march);
		march = mptr;
		return;
		}

	mptr2 = march;
	for (mptr=march; mptr!=NULL; mptr=mptr->next) {
		if (!strcasecmp(mptr->march_name,aaa)) {
			mptr2->next = mptr->next;
			free(mptr);
			mptr=mptr2;
			}
		else {
			mptr2=mptr;
			}
		}
	}


void listrms(char *variety)
{
	char buf[256];
	char rmname[32];
	int f;

	fprintf(stderr, "doing listrms(%s)\n", variety);
	serv_puts(variety);
	serv_gets(buf);
	if (buf[0]!='1') return;
	while (serv_gets(buf), strcmp(buf,"000")) {
		extract(rmname,buf,0);
		wprintf("<A HREF=\"/dotgoto&room=");
		urlescputs(rmname);
		wprintf("\" TARGET=\"top\">");
		escputs1(rmname,1);
		f = extract_int(buf,1);
		if ((f & QR_DIRECTORY) && (f & QR_NETWORK)) wprintf("}");
		else if (f & QR_DIRECTORY) wprintf("]");
		else if (f & QR_NETWORK) wprintf(")");
		else wprintf("&gt;");

		wprintf("</A><TT> </TT>\n");
		};
	wprintf("<BR>\n");
	}



/*
 * list all rooms by floor
 */
void list_all_rooms_by_floor(void) {
	int a;
	char buf[256];

	load_floorlist();

        printf("HTTP/1.0 200 OK\n");
        output_headers(1);

	wprintf("<TABLE width=100% border><TR><TH>Floor</TH>");
	wprintf("<TH>Rooms with new messages</TH>");
	wprintf("<TH>Rooms with no new messages</TH></TR>\n");

	for (a=0; a<128; ++a) if (floorlist[a][0]!=0) {

		/* Floor name column */
		wprintf("<TR><TD>");
	
/*	
		wprintf("<IMG SRC=\"/dynamic/_floorpic_/%d\" ALT=\"%s\">",
			&floorlist[a][0]);
 */
		escputs(&floorlist[a][0]);

		wprintf("</TD>");

		/* Rooms with new messages column */
		wprintf("<TD>");
		sprintf(buf,"LKRN %d",a);
		listrms(buf);
		wprintf("</TD><TD>\n");

		/* Rooms with old messages column */
		sprintf(buf,"LKRO %d",a);
		listrms(buf);
		wprintf("</TD></TR>\n");
		}
	wprintf("</TABLE>\n");
	wprintf("</BODY></HTML>\n");
	wDumpContent();
	}


/*
 * list all forgotten rooms
 */
void zapped_list(void) {
	wprintf("<CENTER><H1>Forgotten rooms</H1>\n");
	listrms("LZRM -1");
	wprintf("</CENTER><HR>\n");
	}
	

/*
 * read this room's info file (set v to 1 for verbose mode)
 */
void readinfo(int v)
{
	char buf[256];

	serv_puts("RINF");
	serv_gets(buf);
	if (buf[0]=='1') fmout(NULL);
	else {
		if (v==1) wprintf("<EM>%s</EM><BR>\n",&buf[4]);
		}
	}


/*
 * generic routine to take the session to a new room
 *
 * display_name values:  0 = goto only
 *                       1 = goto and display
 *                       2 = display only
 */
void gotoroom(char *gname, int display_name)
{
	char buf[256];
	static long ls = (-1L);


	printf("HTTP/1.0 200 OK\n");
	output_headers(1);

	if (display_name != 2) {
		/* store ungoto information */
		strcpy(ugname, wc_roomname);
		uglsn = ls;
		fprintf(stderr, "setting ugname to %s and uglsn to %ld\n",
			ugname, uglsn);
		}

	/* move to the new room */
	sprintf(buf,"GOTO %s",gname);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0]!='2') {
		serv_puts("GOTO _BASEROOM_");
		serv_gets(buf);
		}
	if (buf[0]!='2') {
		wprintf("<EM>%s</EM><BR>\n",&buf[4]);
		wDumpContent();
		return;
		}

	extract(wc_roomname,&buf[4],0);
	room_flags = extract_int(&buf[4],4);
	/* highest_msg_read = extract_int(&buf[4],6);
	maxmsgnum = extract_int(&buf[4],5);
	is_mail = (char) extract_int(&buf[4],7); */
	ls = extract_long(&buf[4], 6);

	if (is_aide) is_room_aide = is_aide;
	else is_room_aide = (char) extract_int(&buf[4],8);

	remove_march(wc_roomname);
	if (!strcasecmp(gname,"_BASEROOM_")) remove_march(gname);

	/* Display the room banner */

	if (display_name) {
		wprintf("<CENTER><TABLE border=0><TR>");

		if ( (strlen(ugname)>0) && (strcasecmp(ugname,wc_roomname)) ) {
			wprintf("<TD><A HREF=\"/ungoto\">");
			wprintf("<IMG SRC=\"/static/back.gif\" border=0></A></TD>");
			}

		wprintf("<TD><H1>%s</H1>",wc_roomname);
		wprintf("<FONT SIZE=-1>%d new of %d messages</FONT></TD>\n",
			extract_int(&buf[4],1),
			extract_int(&buf[4],2));

		/* Display room graphic.  The server doesn't actually need the
		 * room name, but we supply it in order to keep the browser
		 * from using a cached graphic from another room.
		 */
		serv_puts("OIMG _roompic_");
		serv_gets(buf);
		if (buf[0]=='2') {
			wprintf("<TD>");
			wprintf("<IMG SRC=\"/image&name=_roompic_&room=");
			escputs(wc_roomname);
			wprintf("\"></TD>");
			serv_puts("CLOS");
			serv_gets(buf);
			}

		wprintf("<TD>");
		readinfo(0);
		wprintf("</TD>");

		wprintf("<TD><A HREF=\"/gotonext\">");
		wprintf("<IMG SRC=\"/static/forward.gif\" border=0></A></TD>");

		wprintf("</TR></TABLE></CENTER>\n");
		}

	strcpy(wc_roomname, wc_roomname);
	wDumpContent();
	}


/*
 * operation to goto a room
 */
void dotgoto(void) {
	fprintf(stderr, "DOTGOTO: <%s>\n", bstr("room"));
	gotoroom(bstr("room"),1);
	}


/* Goto next room having unread messages.
 * We want to skip over rooms that the user has already been to, and take the
 * user back to the lobby when done.  The room we end up in is placed in
 * newroom - which is set to 0 (the lobby) initially.
 * We start the search in the current room rather than the beginning to prevent
 * two or more concurrent users from dragging each other back to the same room.
 */
void gotonext(void) {
	char buf[256];
	struct march *mptr,*mptr2;
	char next_room[32];

	/* First check to see if the march-mode list is already allocated.
	 * If it is, pop the first room off the list and go there.
	 */

	if (march==NULL) {
		serv_puts("LKRN");
		serv_gets(buf);
		if (buf[0]=='1')
		    while (serv_gets(buf), strcmp(buf,"000")) {
			mptr = (struct march *) malloc(sizeof(struct march));
			mptr->next = NULL;
			extract(mptr->march_name,buf,0);
			if (march==NULL) {
				march = mptr;
				}
			else {
				mptr2 = march;
				while (mptr2->next != NULL)
					mptr2 = mptr2->next;
				mptr2->next = mptr;
				}
			}

/* add _BASEROOM_ to the end of the march list, so the user will end up
 * in the system base room (usually the Lobby>) at the end of the loop
 */
		mptr = (struct march *) malloc(sizeof(struct march));
		mptr->next = NULL;
		strcpy(mptr->march_name,"_BASEROOM_");
		if (march==NULL) {
			march = mptr;
			}
		else {
			mptr2 = march;
			while (mptr2->next != NULL)
				mptr2 = mptr2->next;
			mptr2->next = mptr;
			}
/*
 * ...and remove the room we're currently in, so a <G>oto doesn't make us
 * walk around in circles
 */
		remove_march(wc_roomname);
		}


	if (march!=NULL) {
		strcpy(next_room,march->march_name);
		}
	else {
		strcpy(next_room,"_BASEROOM_");
		}
	gotoroom(next_room,1);
   }



/*
 * mark all messages in current room as having been read
 */
void slrp_highest(void) {
	char buf[256];

	/* set pointer */
	serv_puts("SLRP HIGHEST");
	serv_gets(buf);
	if (buf[0]!='2') {
		wprintf("<EM>%s</EM><BR>\n",&buf[4]);
		return;
		}
	}


/*
 * un-goto the previous room
 */
void ungoto(void) { 
	char buf[256];
	
	if (!strcmp(ugname, "")) {
		gotoroom(wc_roomname, 1);
		return;
		}
	serv_printf("GOTO %s", ugname);
	serv_gets(buf);
	if (buf[0]!='2') {
		gotoroom(wc_roomname, 1);
		return;
		}
	if (uglsn >= 0L) {
		serv_printf("SLRP %ld",uglsn);
		serv_gets(buf);
		}
	strcpy(buf,ugname);
	strcpy(ugname, "");
	gotoroom(buf,1);
	}

/*
 * display the form for editing a room
 */
int display_editroom(void) {
	char buf[256];
	char er_name[20];
	char er_password[10];
	char er_dirname[15];
	char er_roomaide[26];
	unsigned er_flags;

	serv_puts("GETR");
	serv_gets(buf);

	if (buf[0]!='2') {
		wprintf("<EM>%s</EM><BR>\n",&buf[4]);
		return(0);
		}

	extract(er_name,&buf[4],0);
	extract(er_password,&buf[4],1);
	extract(er_dirname,&buf[4],2);
	er_flags=extract_int(&buf[4],3);


        wprintf("<TABLE WIDTH=100% BORDER=0 BGCOLOR=000077><TR><TD>");
        wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"");
        wprintf("<B>Edit this room</B>\n");
        wprintf("</FONT></TD></TR></TABLE>\n");

	wprintf("<FORM METHOD=\"POST\" ACTION=\"/editroom\">\n");

	wprintf("<UL><LI>Name of room: ");	
	wprintf("<INPUT TYPE=\"text\" NAME=\"er_name\" VALUE=\"%s\" MAXLENGTH=\"19\">\n",er_name);

	wprintf("<LI>Type of room:<UL>\n");

	wprintf("<LI><INPUT TYPE=\"radio\" NAME=\"type\" VALUE=\"public\" ");
	if ((er_flags & QR_PRIVATE) == 0) wprintf("CHECKED ");
	wprintf("> Public room\n");

	wprintf("<LI><INPUT TYPE=\"radio\" NAME=\"type\" VALUE=\"guessname\" ");
	if ((er_flags & QR_PRIVATE)&&
	   (er_flags & QR_GUESSNAME)) wprintf("CHECKED ");
	wprintf("> Private - guess name\n");

	wprintf("<LI><INPUT TYPE=\"radio\" NAME=\"type\" VALUE=\"passworded\" ");
	if ((er_flags & QR_PRIVATE)&&
	   (er_flags & QR_PASSWORDED)) wprintf("CHECKED ");
	wprintf("> Private - require password:\n");
	wprintf("<INPUT TYPE=\"text\" NAME=\"er_password\" VALUE=\"%s\" MAXLENGTH=\"9\">\n",er_password);

	wprintf("<LI><INPUT TYPE=\"radio\" NAME=\"type\" VALUE=\"invonly\" ");
	if ( (er_flags & QR_PRIVATE)
	   && ((er_flags & QR_GUESSNAME) == 0)
	   && ((er_flags & QR_PASSWORDED) == 0) )
		wprintf("CHECKED ");
	wprintf("> Private - invitation only\n");

	wprintf("<LI><INPUT TYPE=\"checkbox\" NAME=\"bump\" VALUE=\"yes\" ");
	wprintf("> If private, cause current users to forget room\n");

	wprintf("</UL>\n");

	wprintf("<LI><INPUT TYPE=\"checkbox\" NAME=\"prefonly\" VALUE=\"yes\" ");
	if (er_flags & QR_PREFONLY) wprintf("CHECKED ");
	wprintf("> Preferred users only\n");

	wprintf("<LI><INPUT TYPE=\"checkbox\" NAME=\"readonly\" VALUE=\"yes\" ");
	if (er_flags & QR_READONLY) wprintf("CHECKED ");
	wprintf("> Read-only room\n");

/* directory stuff */
	wprintf("<LI><INPUT TYPE=\"checkbox\" NAME=\"directory\" VALUE=\"yes\" ");
	if (er_flags & QR_DIRECTORY) wprintf("CHECKED ");
	wprintf("> File directory room\n");

	wprintf("<UL><LI>Directory name: ");
	wprintf("<INPUT TYPE=\"text\" NAME=\"er_dirname\" VALUE=\"%s\" MAXLENGTH=\"14\">\n",er_dirname);

	wprintf("<LI><INPUT TYPE=\"checkbox\" NAME=\"ulallowed\" VALUE=\"yes\" ");
	if (er_flags & QR_UPLOAD) wprintf("CHECKED ");
	wprintf("> Uploading allowed\n");

	wprintf("<LI><INPUT TYPE=\"checkbox\" NAME=\"dlallowed\" VALUE=\"yes\" ");
	if (er_flags & QR_DOWNLOAD) wprintf("CHECKED ");
	wprintf("> Downloading allowed\n");

	wprintf("<LI><INPUT TYPE=\"checkbox\" NAME=\"visdir\" VALUE=\"yes\" ");
	if (er_flags & QR_VISDIR) wprintf("CHECKED ");
	wprintf("> Visible directory</UL>\n");

/* end of directory stuff */
	
	wprintf("<LI><INPUT TYPE=\"checkbox\" NAME=\"network\" VALUE=\"yes\" ");
	if (er_flags & QR_NETWORK) wprintf("CHECKED ");
	wprintf("> Network shared room\n");

/* start of anon options */

	wprintf("<LI>Anonymous messages<UL>\n");
	
	wprintf("<LI><INPUT TYPE=\"radio\" NAME=\"anon\" VALUE=\"no\" ");
	if ( ((er_flags & QR_ANONONLY)==0)
	   && ((er_flags & QR_ANONOPT)==0)) wprintf("CHECKED ");
	wprintf("> No anonymous messages\n");

	wprintf("<LI><INPUT TYPE=\"radio\" NAME=\"anon\" VALUE=\"anononly\" ");
	if (er_flags & QR_ANONONLY) wprintf("CHECKED ");
	wprintf("> All messages are anonymous\n");

	wprintf("<LI><INPUT TYPE=\"radio\" NAME=\"anon\" VALUE=\"anon2\" ");
	if (er_flags & QR_ANONOPT) wprintf("CHECKED ");
	wprintf("> Prompt user when entering messages</UL>\n");

/* end of anon options */

	wprintf("<LI>Room aide: \n");
	serv_puts("GETA");
	serv_gets(buf);
	if (buf[0]!='2') {
		wprintf("<EM>%s</EM>\n",&buf[4]);
		}
	else {
		extract(er_roomaide,&buf[4],0);
		wprintf("<INPUT TYPE=\"text\" NAME=\"er_roomaide\" VALUE=\"%s\" MAXLENGTH=\"25\">\n",er_roomaide);
		}
		
	wprintf("</UL><CENTER>\n");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"OK\">");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Cancel\">");
	wprintf("</CENTER>\n");

	wprintf("</FORM>\n");
	return(1);
	}


/*
 * save new parameters for a room
 */
int editroom(void) {
	char buf[256];
	char er_name[20];
	char er_password[10];
	char er_dirname[15];
	char er_roomaide[26];
	unsigned er_flags;
	int bump;


	if (strcmp(bstr("sc"),"OK")) {
		wprintf("<EM>Changes have <STRONG>not</STRONG> been saved.</EM><BR>");
		return(0);
		}
	
	serv_puts("GETR");
	serv_gets(buf);

	if (buf[0]!='2') {
		wprintf("<EM>%s</EM><BR>\n",&buf[4]);
		return(0);
		}

	extract(er_name,&buf[4],0);
	extract(er_password,&buf[4],1);
	extract(er_dirname,&buf[4],2);
	er_flags=extract_int(&buf[4],3);

	strcpy(er_roomaide,bstr("er_roomaide"));
	if (strlen(er_roomaide)==0) {
		serv_puts("GETA");
		serv_gets(buf);
		if (buf[0]!='2') {
			strcpy(er_roomaide,"");
			}
		else {
			extract(er_roomaide,&buf[4],0);
			}
		}

	strcpy(buf,bstr("er_name"));		buf[20] = 0;
	if (strlen(buf)>0) strcpy(er_name,buf);

	strcpy(buf,bstr("er_password"));	buf[10] = 0;
	if (strlen(buf)>0) strcpy(er_password,buf);

	strcpy(buf,bstr("er_dirname"));		buf[15] = 0;
	if (strlen(buf)>0) strcpy(er_dirname,buf);

	strcpy(buf,bstr("type"));
	er_flags &= !(QR_PRIVATE|QR_PASSWORDED|QR_GUESSNAME);

	if (!strcmp(buf,"invonly")) {
		er_flags |= (QR_PRIVATE);
		}
	if (!strcmp(buf,"guessname")) {
		er_flags |= (QR_PRIVATE | QR_GUESSNAME);
		}
	if (!strcmp(buf,"passworded")) {
		er_flags |= (QR_PRIVATE | QR_PASSWORDED);
		}

	if (!strcmp(bstr("prefonly"),"yes")) {
		er_flags |= QR_PREFONLY;
		}
	else {
		er_flags &= ~QR_PREFONLY;
		}

	if (!strcmp(bstr("readonly"),"yes")) {
		er_flags |= QR_READONLY;
		}
	else {
		er_flags &= ~QR_READONLY;
		}

	if (!strcmp(bstr("network"),"yes")) {
		er_flags |= QR_NETWORK;
		}
	else {
		er_flags &= ~QR_NETWORK;
		}

	if (!strcmp(bstr("directory"),"yes")) {
		er_flags |= QR_DIRECTORY;
		}
	else {
		er_flags &= ~QR_DIRECTORY;
		}

	if (!strcmp(bstr("ulallowed"),"yes")) {
		er_flags |= QR_UPLOAD;
		}
	else {
		er_flags &= ~QR_UPLOAD;
		}

	if (!strcmp(bstr("dlallowed"),"yes")) {
		er_flags |= QR_DOWNLOAD;
		}
	else {
		er_flags &= ~QR_DOWNLOAD;
		}

	if (!strcmp(bstr("visdir"),"yes")) {
		er_flags |= QR_VISDIR;
		}
	else {
		er_flags &= ~QR_VISDIR;
		}

	strcpy(buf,bstr("anon"));

	er_flags &= ~(QR_ANONONLY | QR_ANONOPT);
	if (!strcmp(buf,"anononly")) er_flags |= QR_ANONONLY;
	if (!strcmp(buf,"anon2")) er_flags |= QR_ANONOPT;

	bump = 0;
	if (!strcmp(bstr("bump"),"yes")) bump = 1;

	sprintf(buf,"SETR %s|%s|%s|%u|%d",
		er_name,er_password,er_dirname,er_flags,bump);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0]!='2') {
		wprintf("<EM>%s</EM><HR>\n",&buf[4]);
		return(display_editroom());
		}
	gotoroom(er_name,0);

	if (strlen(er_roomaide)>0) {
		sprintf(buf,"SETA %s",er_roomaide);
		serv_puts(buf);
		serv_gets(buf);
		if (buf[0]!='2') {
			wprintf("<EM>%s</EM><HR>\n",&buf[4]);
			return(display_editroom());
			}
		}

	wprintf("<EM>Changes have been saved.</EM><BR>");
	return(0);
	}



/*
 * display the form for entering a new room
 */
int display_entroom(void) {
	char buf[256];

	serv_puts("CRE8 0");
	serv_gets(buf);
	
	if (buf[0]!='2') {
		wprintf("<EM>%s</EM><HR>\n",&buf[4]);
		return(0);
		}

        wprintf("<TABLE WIDTH=100% BORDER=0 BGCOLOR=000077><TR><TD>");
        wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"");
        wprintf("<B>Enter (create) a new room</B>\n");
        wprintf("</FONT></TD></TR></TABLE>\n");

	wprintf("<FORM METHOD=\"POST\" ACTION=\"/entroom\">\n");

	wprintf("<UL><LI>Name of room: ");	
	wprintf("<INPUT TYPE=\"text\" NAME=\"er_name\" MAXLENGTH=\"19\">\n");

	wprintf("<LI>Type of room:<UL>\n");

	wprintf("<LI><INPUT TYPE=\"radio\" NAME=\"type\" VALUE=\"public\" ");
	wprintf("CHECKED > Public room\n");

	wprintf("<LI><INPUT TYPE=\"radio\" NAME=\"type\" VALUE=\"guessname\" ");
	wprintf("> Private - guess name\n");

	wprintf("<LI><INPUT TYPE=\"radio\" NAME=\"type\" VALUE=\"passworded\" ");
	wprintf("> Private - require password:\n");
	wprintf("<INPUT TYPE=\"text\" NAME=\"er_password\" MAXLENGTH=\"9\">\n");

	wprintf("<LI><INPUT TYPE=\"radio\" NAME=\"type\" VALUE=\"invonly\" ");
	wprintf("> Private - invitation only\n");

	wprintf("<CENTER>\n");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"OK\">");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Cancel\">");
	wprintf("</CENTER>\n");
	wprintf("</FORM>\n");
	return(1);
	}



/*
 * enter a new room
 */
int entroom(void) {
	char buf[256];
	char er_name[20];
	char er_type[20];
	char er_password[10];
	int er_num_type;

	if (strcmp(bstr("sc"),"OK")) {
		wprintf("<EM>Changes have <STRONG>not</STRONG> been saved.</EM><BR>");
		return(0);
		}
	
	strcpy(er_name,bstr("er_name"));
	strcpy(er_type,bstr("type"));
	strcpy(er_password,bstr("er_password"));

	er_num_type = 0;
	if (!strcmp(er_type,"guessname")) er_num_type = 1;
	if (!strcmp(er_type,"passworded")) er_num_type = 2;
	if (!strcmp(er_type,"invonly")) er_num_type = 3;

	sprintf(buf,"CRE8 1|%s|%d|%s",er_name,er_num_type,er_password);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0]!='2') {
		wprintf("<EM>%s</EM><HR>\n",&buf[4]);
		return(display_editroom());
		}
	gotoroom(er_name,0);
	return(0);
	}


/*
 * display the screen to enter a private room
 */
void display_private(char *rname, int req_pass)
{


        wprintf("<TABLE WIDTH=100% BORDER=0 BGCOLOR=770000><TR><TD>");
        wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"");
        wprintf("<B>Enter a private room</B>\n");
        wprintf("</FONT></TD></TR></TABLE>\n");

	wprintf("<CENTER>\n");
	wprintf("If you know the name of a hidden (guess-name) or\n");
	wprintf("passworded room, you can enter that room by typing\n");
	wprintf("its name below.  Once you gain access to a private\n");
	wprintf("room, it will appear in your regular room listings\n");
	wprintf("so you don't have to keep returning here.\n");
	wprintf("<BR><BR>");
	
	wprintf("<FORM METHOD=\"POST\" ACTION=\"/goto_private\">\n");

	wprintf("<TABLE border><TR><TD>");
	wprintf("Enter room name:</TD><TD>");
	wprintf("<INPUT TYPE=\"text\" NAME=\"gr_name\" VALUE=\"%s\" MAXLENGTH=\"19\">\n",rname);

	if (req_pass) {
		wprintf("</TD></TR><TR><TD>");
		wprintf("Enter room password:</TD><TD>");
		wprintf("<INPUT TYPE=\"password\" NAME=\"gr_pass\" MAXLENGTH=\"9\">\n");
		}

	wprintf("</TD></TR></TABLE>\n");
	
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"OK\">");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Cancel\">");
	wprintf("</FORM>");
	}

/* 
 * goto a private room
 */
int goto_private(void) {
	char hold_rm[32];
	char buf[256];
	
	if (strcmp(bstr("sc"),"OK")) {
		return(2);
		}

	strcpy(hold_rm,wc_roomname);
	strcpy(buf,"GOTO ");
	strcat(buf,bstr("gr_name"));
	strcat(buf,"|");
	strcat(buf,bstr("gr_pass"));
	serv_puts(buf);
	serv_gets(buf);

	if (buf[0]=='2') {
		gotoroom(bstr("gr_name"),1);
		return(0);
		}

	if (!strncmp(buf,"540",3)) {
		display_private(bstr("gr_name"),1);
		return(1);
		}

	wprintf("<EM>%s</EM>\n",&buf[4]);
	return(2);
	}


/*
 * display the screen to zap a room
 */
void display_zap(void) {
	char zaproom[32];
	
	strcpy(zaproom, bstr("room"));
	
        wprintf("<TABLE WIDTH=100% BORDER=0 BGCOLOR=770000><TR><TD>");
        wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"");
        wprintf("<B>Zap (forget) the current room</B>\n");
        wprintf("</FONT></TD></TR></TABLE>\n");

	wprintf("If you select this option, <em>%s</em> will ", zaproom);
	wprintf("disappear from your room list.  Is this what you wish ");
	wprintf("to do?<BR>\n");

	wprintf("<FORM METHOD=\"POST\" ACTION=\"/zap\">\n");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"OK\">");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Cancel\">");
	wprintf("</FORM>");
	}


/* 
 * zap a room
 */
int zap(void) {
	char zaproom[32];
	char buf[256];

	if (strcmp(bstr("sc"),"OK")) {
		return(2);
		}

	strcpy(zaproom, bstr("room"));
	sprintf(buf, "GOTO %s", zaproom);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0] != '2') {
		wprintf("<EM>%s</EM>\n",&buf[4]);
		return(2);
		}

	serv_puts("FORG");
	serv_gets(buf);
	if (buf[0] != '2') {
		wprintf("<EM>%s</EM>\n",&buf[4]);
		return(2);
		}

	gotoroom(bstr("_BASEROOM_"),1);
	return(0);
	}
