/* Citadel/UX room-oriented routines */

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include "citadel.h"
#include "rooms.h"
#include "tools.h"

#define IFEXPERT if (userflags&US_EXPERT)
#define IFNEXPERT if ((userflags&US_EXPERT)==0)
#define IFAIDE if (axlevel>=6)
#define IFNAIDE if (axlevel<6)


void sttybbs(int cmd);
void extract(char *dest, char *source, int parmnum);
int extract_int(char *source, int parmnum);
void hit_any_key(void);
int yesno(void);
int yesno_d(int d);
void strprompt(char *prompt, char *str, int len);
void newprompt(char *prompt, char *str, int len);
int struncmp(char *lstr, char *rstr, int len);
void dotgoto(char *towhere, int display_name);
long extract_long(char *source, int parmnum);
void serv_read(char *buf, int bytes);
void formout(char *name);
int inkey(void);
int fmout(int width, FILE *fp, char pagin, int height, int starting_lp, char subst);
void citedit(FILE *fp, long int base_pos);
void progress(long int curr, long int cmax);
int pattern(char *search, char *patn);
int file_checksum(char *filename);
int nukedir(char *dirname);
void color(int colornum);

extern unsigned room_flags;
extern char room_name[];
extern char temp[];
extern char tempdir[];
extern int editor_pid;
extern char editor_path[];
extern int screenwidth;
extern int screenheight;
extern char fullname[];
extern int userflags;
extern char sigcaught;
extern char floor_mode;
extern char curr_floor;


extern int ugnum;
extern long uglsn;
extern char ugname[];

extern char floorlist[128][256];


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

void listrms(char *variety)
{
	char buf[256];
	char rmname[32];
	int f,c;

	serv_puts(variety);
	serv_gets(buf);
	if (buf[0]!='1') return;
	c = 1;
	sigcaught = 0;
	sttybbs(SB_YES_INTR);
	while (serv_gets(buf), strcmp(buf,"000")) if (sigcaught==0) {
		extract(rmname,buf,0);
		if ((c + strlen(rmname) + 4) > screenwidth) {
			printf("\n");
			c = 1;
			}
		f = extract_int(buf,1);
		if (f & QR_PRIVATE) {
			color(1);
			}
		else {
			color(2);
			}
		printf("%s",rmname);
		if ((f & QR_DIRECTORY) && (f & QR_NETWORK)) printf("}  ");
		else if (f & QR_DIRECTORY) printf("]  ");
		else if (f & QR_NETWORK) printf(")  ");
		else printf(">  ");
		c = c + strlen(rmname) + 3;
		}
	color(7);
	sttybbs(SB_NO_INTR);
	}


void list_other_floors(void) {
	int a,c;

	c = 1;
	for (a=0; a<128; ++a) if ((strlen(floorlist[a])>0)&&(a!=curr_floor)) {
		if ((c + strlen(floorlist[a]) + 4) > screenwidth) {
			printf("\n");
			c = 1;
			}
		printf("%s:  ",floorlist[a]);
		c = c + strlen(floorlist[a]) + 3;
		}
	}


/*
 * List known rooms.  kn_floor_mode should be set to 0 for a 'flat' listing,
 * 1 to list rooms on the current floor, or 1 to list rooms on all floors.
 */
void knrooms(int kn_floor_mode)
{
	char buf[256];
	int a;

	load_floorlist();

	if (kn_floor_mode == 0) {
		color(3);
		printf("\n   Rooms with unread messages:\n");
		listrms("LKRN");
		color(3);
		printf("\n\n   No unseen messages in:\n");
		listrms("LKRO");
		printf("\n");
		}

	if (kn_floor_mode == 1) {
		color(3);
		printf("\n   Rooms with unread messages on %s:\n",
			floorlist[(int)curr_floor]);
		sprintf(buf,"LKRN %d",curr_floor);
		listrms(buf);
		color(3);
		printf("\n\n   Rooms with no new messages on %s:\n",
			floorlist[(int)curr_floor]);
		sprintf(buf,"LKRO %d",curr_floor);
		listrms(buf);
		color(3);
		printf("\n\n   Other floors:\n");
		list_other_floors();
		printf("\n");
		}

	if (kn_floor_mode == 2) {
		for (a=0; a<128; ++a) if (floorlist[a][0]!=0) {
			color(3);
			printf("\n   Rooms on %s:\n",floorlist[a]);
			sprintf(buf,"LKRA %d",a);
			listrms(buf);
			printf("\n");
			}
		}
	
	color(7);
	IFNEXPERT hit_any_key();
	}


void listzrooms(void) {		/* list public forgotten rooms */
	color(3);
	printf("\n   Forgotten public rooms:\n");
	listrms("LZRM");
	printf("\n");
	color(7);
	IFNEXPERT hit_any_key();
	}


int set_room_attr(int ibuf, char *prompt, unsigned int sbit)
{
	int a;

	printf("%s [%s]? ",prompt,((ibuf&sbit) ? "Yes":"No"));
	a=yesno_d(ibuf&sbit);
	ibuf=(ibuf|sbit);
	if (!a) ibuf=(ibuf^sbit);
	return(ibuf);
	}



/*
 * Select a floor (used in several commands)
 * The supplied argument is the 'default' floor number.
 * This function returns the selected floor number.
 */
int select_floor(int rfloor)
{
	int a, newfloor;
	char floorstr[256];

	if (floor_mode == 1) {
		if (floorlist[(int)curr_floor][0]==0) load_floorlist();

		do {
			newfloor = (-1);
			safestrncpy(floorstr,floorlist[rfloor],sizeof floorstr);
			strprompt("Which floor",floorstr,256);
			for (a=0; a<128; ++a) {
				if (!strucmp(floorstr,&floorlist[a][0]))
					newfloor = a;
				if ((newfloor<0)&&(!struncmp(floorstr,
					&floorlist[a][0],strlen(floorstr))))
						newfloor = a;
				if ((newfloor<0)&&(pattern(&floorlist[a][0],
					floorstr)>=0)) newfloor = a;
				}
			if (newfloor<0) {
				printf("\n One of:\n");
				for (a=0; a<128; ++a)
					if (floorlist[a][0]!=0)
						printf("%s\n",
							&floorlist[a][0]);
				}
			} while(newfloor < 0);
		return(newfloor);
		}
	return(rfloor);
	}




/*
 * .<A>ide <E>dit room
 */
void editthisroom(void) {
	char rname[ROOMNAMELEN];
	char rpass[10];
	char rdir[15];
	unsigned rflags;
	int rbump;
	char raide[32];
	char buf[256];
	int rfloor;

	serv_puts("GETR");
	serv_gets(buf);
	if (buf[0]!='2') {
		printf("%s\n",&buf[4]);
		return;
		}

	extract(rname,&buf[4],0);
	extract(rpass,&buf[4],1);
	extract(rdir, &buf[4],2);
	rflags = extract_int(&buf[4],3);
	rfloor = extract_int(&buf[4],4);
	rbump = 0;

	serv_puts("GETA");
	serv_gets(buf);
	if (buf[0]=='2') safestrncpy(raide,&buf[4],sizeof raide);
	else strcpy(raide,"");
	if (strlen(raide)==0) strcpy(raide,"none");

	strprompt("Room name",rname,ROOMNAMELEN-1);

	rfloor = select_floor(rfloor);
	rflags = set_room_attr(rflags,"Private room",QR_PRIVATE);
	if (rflags & QR_PRIVATE)
		rflags = set_room_attr(rflags,
			"Accessible by guessing room name",QR_GUESSNAME);

	/* if it's public, clear the privacy classes */
	if ((rflags & QR_PRIVATE)==0) {
		if (rflags & QR_GUESSNAME)  rflags = rflags - QR_GUESSNAME;
		if (rflags & QR_PASSWORDED) rflags = rflags - QR_PASSWORDED;
		}

	/* if it's private, choose the privacy classes */
	if ( (rflags & QR_PRIVATE)
	   && ( (rflags & QR_GUESSNAME) == 0) ) {
		rflags = set_room_attr(rflags,
			"Accessible by entering a password",QR_PASSWORDED);
		}
	if ( (rflags & QR_PRIVATE)
	   && ((rflags&QR_PASSWORDED)==QR_PASSWORDED) ) {
		strprompt("Room password",rpass,9);
		}

	if ((rflags&QR_PRIVATE)==QR_PRIVATE) {
		printf("Cause current users to forget room [No] ? ");
		if (yesno_d(0)==1) rbump = 1;
		}

	rflags = set_room_attr(rflags,"Preferred users only",QR_PREFONLY);
	rflags = set_room_attr(rflags,"Read-only room",QR_READONLY);
	rflags = set_room_attr(rflags,"Directory room",QR_DIRECTORY);
	rflags = set_room_attr(rflags,"Permanent room",QR_PERMANENT);
	if (rflags & QR_DIRECTORY) {
		strprompt("Directory name",rdir,14);
		rflags = set_room_attr(rflags,"Uploading allowed",QR_UPLOAD);
		rflags = set_room_attr(rflags,"Downloading allowed",
								QR_DOWNLOAD);
		rflags = set_room_attr(rflags,"Visible directory",QR_VISDIR);
		}
	rflags = set_room_attr(rflags,"Network shared room",QR_NETWORK);
	rflags = set_room_attr(rflags,
		"Automatically make all messages anonymous",QR_ANONONLY);
	if ( (rflags & QR_ANONONLY) == 0) {
		rflags = set_room_attr(rflags,
			"Ask users whether to make messages anonymous",
			QR_ANONOPT);
		}

	do {
		strprompt("Room aide (or 'none')",raide,29);
		if (!strucmp(raide,"none")) {
			strcpy(raide,"");
			strcpy(buf,"200");
			}
		else {
			snprintf(buf,sizeof buf,"QUSR %s",raide);
			serv_puts(buf);
			serv_gets(buf);
			if (buf[0]!='2') printf("%s\n",&buf[4]);
			}
		} while(buf[0]!='2');

	if (!strucmp(raide,"none")) strcpy(raide,"");

	printf("Save changes (y/n)? ");
	if (yesno()==1) {
		snprintf(buf,sizeof buf,"SETR %s|%s|%s|%d|%d|%d",
			rname,rpass,rdir,rflags,rbump,rfloor);
		serv_puts(buf);
		serv_gets(buf);
		printf("%s\n",&buf[4]);
		snprintf(buf,sizeof buf,"SETA %s",raide);
		serv_puts(buf);
		serv_gets(buf);
		if (buf[0]=='2') dotgoto(rname,2);
		}
	}


/*
 * un-goto the previous room
 */
void ungoto(void) { 
	char buf[256];
	
	if (!strcmp(ugname,"")) return;
	snprintf(buf,sizeof buf,"GOTO %s",ugname);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0]!='2') {
		printf("%s\n",&buf[4]);
		return;
		}
	sprintf(buf,"SLRP %ld",uglsn);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0]!='2') printf("%s\n",&buf[4]);
	safestrncpy(buf,ugname,sizeof buf);
	strcpy(ugname,"");
	dotgoto(buf,0);
	}


/*
 * download()  -  download a file or files.  The argument passed to this
 *                function determines which protocol to use.
 */
void download(int proto)
{

/*
  - 0 = paginate, 1 = xmodem, 2 = raw, 3 = ymodem, 4 = zmodem, 5 = save
*/


	char buf[256];
	char dbuf[4096];
	char filename[256];
	long total_bytes = 0L;
	long transmitted_bytes = 0L;
	long aa,bb;
	int a,b;
	int packet;
	FILE *tpipe = NULL;
	FILE *savefp = NULL;
	int proto_pid;
	int broken = 0;

	if ((room_flags & QR_DOWNLOAD) == 0) {
		printf("*** You cannot download from this room.\n");
		return;
		}
	
	newprompt("Enter filename: ",filename,255);

	snprintf(buf,sizeof buf,"OPEN %s",filename);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0]!='2') {
		printf("%s\n",&buf[4]);
		return;
		}
	total_bytes = extract_long(&buf[4],0);


	/* Here's the code for simply transferring the file to the client,
	 * for folks who have their own clientware.  It's a lot simpler than
	 * the [XYZ]modem code below...
	 */
	if (proto == 5) {
		printf("Enter the name of the directory to save '%s'\n",
			filename);
		printf("to, or press return for the current directory.\n");
		newprompt("Directory: ",dbuf,256);
		if (strlen(dbuf)==0) strcpy(dbuf,".");
		strcat(dbuf,"/");
		strcat(dbuf,filename);
		
		savefp = fopen(dbuf,"w");
		if (savefp == NULL) {
			printf("Cannot open '%s': %s\n",dbuf,strerror(errno));
			/* close the download file at the server */
			serv_puts("CLOS");
			serv_gets(buf);
			if (buf[0]!='2') {
				printf("%s\n",&buf[4]);
				}
			return;
			}
		progress(0,total_bytes);
		while ( (transmitted_bytes < total_bytes) && (broken == 0) ) {
			bb = total_bytes - transmitted_bytes;
			aa = ((bb < 4096) ? bb : 4096);
			sprintf(buf,"READ %ld|%ld",transmitted_bytes,aa);
			serv_puts(buf);
			serv_gets(buf);
			if (buf[0]!='6') {
				printf("%s\n",&buf[4]);
				return;
				}
			packet = extract_int(&buf[4],0);
			serv_read(dbuf,packet);
			if (fwrite(dbuf,packet,1,savefp) < 1) broken = 1;
			transmitted_bytes = transmitted_bytes + (long)packet;
			progress(transmitted_bytes,total_bytes);
			}
		fclose(savefp);
		/* close the download file at the server */
		serv_puts("CLOS");
		serv_gets(buf);
		if (buf[0]!='2') {
			printf("%s\n",&buf[4]);
			}
		return;
		}


	mkdir(tempdir,0700);
	snprintf(buf,sizeof buf,"%s/%s",tempdir,filename);
	mkfifo(buf, 0777);

	/* We do the remainder of this function as a separate process in
	 * order to allow recovery if the transfer is aborted.  If the
	 * file transfer program aborts, the first child process receives a
	 * "broken pipe" signal and aborts.  We *should* be able to catch
	 * this condition with signal(), but it doesn't seem to work on all
	 * systems.
	 */
	a = fork();
	if (a!=0) {
		/* wait for the download to finish */
		while (wait(&b)!=a) ;;
		sttybbs(0);
		/* close the download file at the server */
		serv_puts("CLOS");
		serv_gets(buf);
		if (buf[0]!='2') {
			printf("%s\n",&buf[4]);
			}
		/* clean up the temporary directory */
		nukedir(tempdir);
		return;
		}

	snprintf(buf,sizeof buf,"%s/%s",tempdir,filename); /* full pathname */

	/* The next fork() creates a second child process that is used for
	 * the actual file transfer program (usually sz).
	 */
	proto_pid = fork();
	if (proto_pid == 0) {
		if (proto==0)  {
			sttybbs(0);
			signal(SIGINT,SIG_DFL);
			signal(SIGQUIT,SIG_DFL);
			snprintf(dbuf,sizeof dbuf,"SHELL=/dev/null; export SHELL; TERM=dumb; export TERM; exec more -d <%s",buf);
			system(dbuf);
			sttybbs(SB_NO_INTR);
			exit(0);
			}
		sttybbs(3);
		signal(SIGINT,SIG_DFL);
		signal(SIGQUIT,SIG_DFL);
		if (proto==1) execlp("sx","sx",buf,NULL);
		if (proto==2) execlp("cat","cat",buf,NULL);
		if (proto==3) execlp("sb","sb",buf,NULL);
		if (proto==4) execlp("sz","sz",buf,NULL);
		execlp("cat","cat",buf,NULL);
		exit(1);
		}

	tpipe = fopen(buf,"w");

	while ( (transmitted_bytes < total_bytes) && (broken == 0) ) {
		bb = total_bytes - transmitted_bytes;
		aa = ((bb < 4096) ? bb : 4096);
		sprintf(buf,"READ %ld|%ld",transmitted_bytes,aa);
		serv_puts(buf);
		serv_gets(buf);
		if (buf[0]!='6') {
			printf("%s\n",&buf[4]);
			return;
			}
		packet = extract_int(&buf[4],0);
		serv_read(dbuf,packet);
		if (fwrite(dbuf,packet,1,tpipe) < 1) broken = 1;
		transmitted_bytes = transmitted_bytes + (long)packet;
		}
	if (tpipe!=NULL) fclose(tpipe);

	/* Hang out and wait for the file transfer program to finish */
	while (wait(&a) != proto_pid) ;;


	putc(7,stdout);
	exit(0);	/* transfer control back to the main program */
	}


/*
 * read directory of this room
 */
void roomdir(void) {
	char flnm[256];
	char flsz[32];
	char comment[256];
	char buf[256];

	serv_puts("RDIR");
	serv_gets(buf);
	if (buf[0]!='1') {
		printf("%s\n",&buf[4]);
		return;
		}

	extract(comment,&buf[4],0);
	extract(flnm,&buf[4],1);
	printf("\nDirectory of %s on %s\n",flnm,comment);
	printf("-----------------------\n");
	while (serv_gets(buf), strcmp(buf,"000")) {
		extract(flnm,buf,0);
		extract(flsz,buf,1);
		extract(comment,buf,2);
		if (strlen(flnm)<=14)
	    		printf("%-14s %8s %s\n",flnm,flsz,comment);
		else
			printf("%s\n%14s %8s %s\n",flnm,"",flsz,comment);
		}
	}


/*
 * add a user to a private room
 */
void invite(void) {
	char aaa[31],bbb[256];

	if ((room_flags & QR_PRIVATE)==0) {
		printf("This is not a private room.\n");
		return;
		}

	newprompt("Name of user? ",aaa,30);
	if (aaa[0]==0) return;

	snprintf(bbb,sizeof bbb,"INVT %s",aaa);
	serv_puts(bbb);
	serv_gets(bbb);
	printf("%s\n",&bbb[4]);
	}


/*
 * kick a user out of a room
 */
void kickout(void) {
	char aaa[31],bbb[256];

	if ((room_flags & QR_PRIVATE)==0) {
		printf("Note: this is not a private room.  Kicking a user ");
		printf("out of this room will only\nhave the same effect ");
		printf("as if they <Z>apped the room.\n\n");
		}

	newprompt("Name of user? ",aaa,30);
	if (aaa[0]==0) return;

	snprintf(bbb,sizeof bbb,"KICK %s",aaa);
	serv_puts(bbb);
	serv_gets(bbb);
	printf("%s\n",&bbb[4]);
	}


/*
 * aide command: kill the current room
 */
void killroom(void) {
	char aaa[100];

	serv_puts("KILL 0");
	serv_gets(aaa);
	if (aaa[0]!='2') {
		printf("%s\n",&aaa[4]);
		return;
		}

	printf("Are you sure you want to kill this room? ");
	if (yesno()==0) return;

	serv_puts("KILL 1");
	serv_gets(aaa);
	printf("%s\n",&aaa[4]);
	if (aaa[0]!='2') return;
	dotgoto("_BASEROOM_",0);
	}

void forget(void) {	/* forget the current room */
	char cmd[256];

	printf("Are you sure you want to forget this room? ");
	if (yesno()==0) return;

	serv_puts("FORG");
	serv_gets(cmd);
	if (cmd[0]!='2') {
		printf("%s\n",&cmd[4]);
		return;
		}

	/* now return to the lobby */
	dotgoto("_BASEROOM_",0);
	}


/*
 * create a new room
 */
void entroom(void) {
	char cmd[256];
	char new_room_name[ROOMNAMELEN];
	int new_room_type;
	char new_room_pass[10];
	int new_room_floor;
	int a,b;

	serv_puts("CRE8 0");
	serv_gets(cmd);
	
	if (cmd[0]!='2') {
		printf("%s\n",&cmd[4]);
		return;
		}
	
	newprompt("Name for new room? ",new_room_name,ROOMNAMELEN-1);
	if (strlen(new_room_name)==0) return;
	for (a=0; a<strlen(new_room_name); ++a)
		if (new_room_name[a] == '|') new_room_name[a]='_';

	new_room_floor = select_floor((int)curr_floor);

	IFNEXPERT formout("roomaccess");
	do {
		printf("<?>Help\n<1>Public room\n<2>Guess-name room\n");
		printf("<3>Passworded room\n<4>Invitation-only room\n");
		printf("Enter room type: ");
		do {
			b=inkey();
			} while (((b<'1')||(b>'4')) && (b!='?'));
		if (b=='?') {
			printf("?\n");
			formout("roomaccess");
			}
		} while ((b<'1')||(b>'4'));
	b=b-48;
	printf("%d\n",b);
	new_room_type = b - 1;
	if (new_room_type==2) {
		newprompt("Enter a room password: ",new_room_pass,9);
		for (a=0; a<strlen(new_room_pass); ++a)
			if (new_room_pass[a] == '|') new_room_pass[a]='_';
		}
	else strcpy(new_room_pass,"");

	printf("\042%s\042, a",new_room_name);
	if (b==1) printf(" public room.");
	if (b==2) printf(" guess-name room.");
	if (b==3) printf(" passworded room, password: %s",new_room_pass);
	if (b==4) printf("n invitation-only room.");
	printf("\nInstall it? (y/n) : ");
	a=yesno();
	if (a==0) return;

	snprintf(cmd, sizeof cmd, "CRE8 1|%s|%d|%s|%d", new_room_name,
		new_room_type, new_room_pass, new_room_floor);
	serv_puts(cmd);
	serv_gets(cmd);
	if (cmd[0]!='2') {
		printf("%s\n",&cmd[4]);
		return;
		}

	/* command succeeded... now GO to the new room! */
	dotgoto(new_room_name,0);
	}



void readinfo(void) {	/* read info file for current room */
	char cmd[256];
	
	sprintf(cmd,"RINF");
	serv_puts(cmd);
	serv_gets(cmd);

	if (cmd[0]!='1') return;

	fmout(screenwidth,NULL,
		((userflags & US_PAGINATOR) ? 1 : 0),
		screenheight,0,1);
	}


/*
 * <W>ho knows room...
 */
void whoknows(void) {
	char buf[256];
	serv_puts("WHOK");
	serv_gets(buf);
	if (buf[0]!='1') {
		printf("%s\n",&buf[5]);
		return;
		}
	sigcaught = 0;
	sttybbs(SB_YES_INTR);
	while (serv_gets(buf), strncmp(buf,"000",3)) {
		if (sigcaught==0) printf("%s\n",buf);
		}
	sttybbs(SB_NO_INTR);
	}


void do_edit(char *desc, char *read_cmd, char *check_cmd, char *write_cmd)
{
	FILE *fp;
	char cmd[256];
	int b,cksum,editor_exit;


	if (strlen(editor_path)==0) {
		printf("Do you wish to re-enter %s? ",desc);
		if (yesno()==0) return;
		}

	fp = fopen(temp,"w");
	fclose(fp);

	serv_puts(check_cmd);
	serv_gets(cmd);
	if (cmd[0]!='2') {
		printf("%s\n",&cmd[4]);
		return;
		}

	if (strlen(editor_path)>0) {
		serv_puts(read_cmd);
		serv_gets(cmd);
		if (cmd[0]=='1') {
			fp = fopen(temp,"w");
			while (serv_gets(cmd), strcmp(cmd,"000")) {
				fprintf(fp,"%s\n",cmd);
				}
			fclose(fp);
			}
		}

	cksum = file_checksum(temp);

	if (strlen(editor_path)>0) {
		editor_pid=fork();
		if (editor_pid==0) {
                        chmod(temp,0600);
			sttybbs(SB_RESTORE);
			execlp(editor_path,editor_path,temp,NULL);
			exit(1);
			}
		if (editor_pid>0) do {
			editor_exit = 0;
			b=wait(&editor_exit);
			} while((b!=editor_pid)&&(b>=0));
		editor_pid = (-1);
		printf("Executed %s\n", editor_path);
		sttybbs(0);
		}
	else {
		printf("Entering %s.  ",desc);
		printf("Press return twice when finished.\n");
		fp=fopen(temp,"r+");
		citedit(fp,0);
		fclose(fp);
		}

	if (file_checksum(temp) == cksum) {
		printf("*** Aborted.\n");
		}

	else {
		serv_puts(write_cmd);
		serv_gets(cmd);
		if (cmd[0]!='4') {
			printf("%s\n",&cmd[4]);
			return;
			}

		fp=fopen(temp,"r");
		while (fgets(cmd,255,fp)!=NULL) {
			cmd[strlen(cmd)-1] = 0;
			serv_puts(cmd);
			}
		fclose(fp);
		serv_puts("000");
		}

	unlink(temp);
	}


void enterinfo(void) {		/* edit info file for current room */
	do_edit("the Info file for this room","RINF","EINF 0","EINF 1");
	}

void enter_bio(void) {
	char cmd[256];
	snprintf(cmd,sizeof cmd,"RBIO %s",fullname);
	do_edit("your Bio",cmd,"NOOP","EBIO");
	}

/*
 * create a new floor
 */
void create_floor(void) {
	char buf[256];
	char newfloorname[256];

	serv_puts("CFLR xx|0");
	serv_gets(buf);
	if (buf[0]!='2') {
		printf("%s\n",&buf[4]);
		return;
		}

	newprompt("Name for new floor: ",newfloorname,255);
	snprintf(buf,sizeof buf,"CFLR %s|1",newfloorname);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0]=='2') {
		printf("Floor has been created.\n");
		}
	else {
		printf("%s\n",&buf[4]);
		}
	}

/*
 * edit the current floor
 */
void edit_floor(void) {
	char buf[256];

	if (floorlist[(int)curr_floor][0]==0) load_floorlist();
	strprompt("New floor name",&floorlist[(int)curr_floor][0],255);
	snprintf(buf,sizeof buf,"EFLR %d|%s",curr_floor,
		 &floorlist[(int)curr_floor][0]);
	serv_puts(buf);
	serv_gets(buf);
	printf("%s\n",&buf[4]);
	load_floorlist();
	}




/*
 * kill the current floor 
 */
void kill_floor(void) {
	int floornum_to_delete,a;
	char buf[256];

	if (floorlist[(int)curr_floor][0]==0) load_floorlist();
	do {
		floornum_to_delete = (-1);
		printf("(Press return to abort)\n");
		newprompt("Delete which floor? ",buf,255);
		if (strlen(buf)==0) return;
		for (a=0; a<128; ++a)
			if (!strucmp(&floorlist[a][0],buf))
				floornum_to_delete = a;
		if (floornum_to_delete < 0) {
			printf("No such floor.  Select one of:\n");
			for (a=0; a<128; ++a)
				if (floorlist[a][0]!=0)
					printf("%s\n",&floorlist[a][0]);
			}
		} while (floornum_to_delete < 0);
	sprintf(buf,"KFLR %d|1",floornum_to_delete);
	serv_puts(buf);
	serv_gets(buf);
	printf("%s\n",&buf[4]);
	}
