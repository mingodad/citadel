/* More Citadel/UX routines...
 * unlike routines.c, some of these DO use global variables.
 */

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <pwd.h>
#include <setjmp.h>
#include <errno.h>
#include "citadel.h"
#include "routines2.h"
#include "routines.h"

void interr(int errnum);
void strprompt(char *prompt, char *str, int len);
void newprompt(char *prompt, char *str, int len);
void sttybbs(int cmd);
int inkey(void);
int ka_wait(pid_t *kstatus);
void serv_write(char *buf, int nbytes);
void extract(char *dest, char *source, int parmnum);
int haschar(char *st, int ch);
void progress(long int curr, long int cmax);
void citedit(FILE *fp, long int base_pos);
int yesno(void);

extern char temp[];
extern char tempdir[];
extern char *axdefs[7];
extern long highest_msg_read;
extern long maxmsgnum;
extern unsigned room_flags;
extern int screenwidth;


int eopen(char *name, int mode)
{
	int ret;
	ret = open(name,mode);
	if (ret<0) {
		fprintf(stderr,"Cannot open file '%s', mode=%d, errno=%d\n",
			name,mode,errno);
		interr(errno);
		}
	return(ret);
	}


int room_prompt(int qrflags)	/* return proper room prompt character */
             {
	int a;
	a='>';
	if (qrflags&QR_DIRECTORY) a=']';
	if ((a==']')&&(qrflags&QR_NETWORK)) a='}';
	if ((a=='>')&&(qrflags&QR_NETWORK)) a=')';
	return(a);
	}

void entregis(void)	/* register with name and address */
	{

	char buf[256];
	char tmpname[256];
	char tmpaddr[256];
	char tmpcity[256];
	char tmpstate[256];
	char tmpzip[256];
	char tmpphone[256];
	char tmpemail[256];
	int a;

	strcpy(tmpname,"");
	strcpy(tmpaddr,"");
	strcpy(tmpcity,"");
	strcpy(tmpstate,"");
	strcpy(tmpzip,"");
	strcpy(tmpphone,"");
	strcpy(tmpemail,"");

	serv_puts("GREG _SELF_");
	serv_gets(buf);
	if (buf[0]=='1') {
		a = 0;
		while (serv_gets(buf), strcmp(buf,"000")) {
			if (a==2) strcpy(tmpname,buf);
			if (a==3) strcpy(tmpaddr,buf);
			if (a==4) strcpy(tmpcity,buf);
			if (a==5) strcpy(tmpstate,buf);
			if (a==6) strcpy(tmpzip,buf);
			if (a==7) strcpy(tmpphone,buf);
			if (a==9) strcpy(tmpemail,buf);
			++a;
			}
		}

	strprompt("REAL name",tmpname,29);
	strprompt("Address",tmpaddr,24);
	strprompt("City/town",tmpcity,14);
	strprompt("State",tmpstate,2);
	strprompt("ZIP Code",tmpzip,10);
	strprompt("Telephone number",tmpphone,14);
	strprompt("Email address",tmpemail,31);

	/* now send the registration info back to the server */
	serv_puts("REGI");
	serv_gets(buf);
	if (buf[0]!='4') {
		printf("%s\n",&buf[4]);
		return;
		}
	serv_puts(tmpname);
	serv_puts(tmpaddr);
	serv_puts(tmpcity);
	serv_puts(tmpstate);
	serv_puts(tmpzip);
	serv_puts(tmpphone);
	serv_puts(tmpemail);
	serv_puts("000");
	printf("\n");
	}

void updatels(void) {	/* make all messages old in current room */
	char buf[256];
	serv_puts("SLRP HIGHEST");
	serv_gets(buf);
	if (buf[0]!='2') printf("%s\n",&buf[4]);
	}

void updatelsa(void) {   /* only make messages old in this room that have been read */
	char buf[256];
	sprintf(buf,"SLRP %ld",highest_msg_read);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0]!='2') printf("%s\n",&buf[4]);
	}


/*
 * This routine completes a client upload
 */
void do_upload(int fd) {
	char buf[256];
	char tbuf[4096];
	long transmitted_bytes, total_bytes;
	int bytes_to_send;
	int bytes_expected;

	/* learn the size of the file */
	total_bytes = lseek(fd,0L,2);
	lseek(fd,0L,0);

	transmitted_bytes = 0L;
	progress(transmitted_bytes,total_bytes);
	do {
		bytes_to_send = read(fd,tbuf,4096);
		if (bytes_to_send>0) {
			sprintf(buf,"WRIT %d",bytes_to_send);
			serv_puts(buf);
			serv_gets(buf);
			if (buf[0]=='7') {
				bytes_expected = atoi(&buf[4]);
				serv_write(tbuf,bytes_expected);
				}
			else {
				printf("%s\n",&buf[4]);
				}
			}
		transmitted_bytes = transmitted_bytes + (long)bytes_to_send;
		progress(transmitted_bytes, total_bytes);
		} while (bytes_to_send > 0);

	/* close the upload file, locally and at the server */
	close(fd);
	serv_puts("UCLS 1");
	serv_gets(buf);
	printf("%s\n",&buf[4]);
	}


/*
 * client-based uploads (for users with their own clientware)
 */
void cli_upload(void) {
	char flnm[256];
	char desc[151];
	char buf[256];
	char tbuf[256];
	int a;
	int fd;

	if ((room_flags & QR_UPLOAD) == 0) {
		printf("*** You cannot upload to this room.\n");
		return;
		}

	newprompt("File to be uploaded: ",flnm,55);
	fd = open(flnm,O_RDONLY);
	if (fd<0) {
		printf("Cannot open '%s': %s\n",flnm,strerror(errno));
		return;
		}
	printf("Enter a description of this file:\n");
	newprompt(": ",desc,75);

	/* keep generating filenames in hope of finding a unique one */
	a = 0;
	do {
		if (a==10) return; /* fail if tried 10 times */
		strcpy(buf,flnm);
		while ((strlen(buf)>0)&&(haschar(buf,'/')))
			strcpy(buf,&buf[1]);
		if (a>0) sprintf(&buf[strlen(buf)],"%d",a);
		sprintf(tbuf,"UOPN %s|%s",buf,desc);
		serv_puts(tbuf);
		serv_gets(buf);
		if (buf[0]!='2') printf("%s\n",&buf[4]);
		++a;
		} while (buf[0]!='2');

	/* at this point we have an open upload file at the server */
	do_upload(fd);
	}


/*
 * Function used for various image upload commands
 */
void cli_image_upload(char *keyname) {
	char flnm[256];
	char buf[256];
	int fd;

	sprintf(buf, "UIMG 0|%s", keyname);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0] != '2') {
		printf("%s\n", &buf[4]);
		return;
		}

	newprompt("Image file to be uploaded: ",flnm,55);
	fd = open(flnm,O_RDONLY);
	if (fd<0) {
		printf("Cannot open '%s': %s\n",flnm,strerror(errno));
		return;
		}

	sprintf(buf, "UIMG 1|%s", keyname);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0] != '2') {
		printf("%s\n", &buf[4]);
		return;
		}

	do_upload(fd);
	}


/*
 * protocol-based uploads (Xmodem, Ymodem, Zmodem)
 */
void upload(int c)	/* c = upload mode */
       {
	char flnm[256];
	char desc[151];
	char buf[256];
	char tbuf[4096];
	int xfer_pid;
	int a,b;
	FILE *fp,*lsfp;
	int fd;

	if ((room_flags & QR_UPLOAD) == 0) {
		printf("*** You cannot upload to this room.\n");
		return;
		}

	/* we don't need a filename when receiving batch y/z modem */
	if ((c==2)||(c==3)) strcpy(flnm,"x");
	else newprompt("Enter filename: ",flnm,15);

	for (a=0; a<strlen(flnm); ++a)
		if ( (flnm[a]=='/') || (flnm[a]=='\\') || (flnm[a]=='>')
		     || (flnm[a]=='?') || (flnm[a]=='*')
		     || (flnm[a]==';') || (flnm[a]=='&') ) flnm[a]='_';

	newprompt("Enter a short description of the file:\n: ",desc,150);

	/* create a temporary directory... */
	if (mkdir(tempdir,0700) != 0) {
		printf("*** Could not create temporary directory %s: %s\n",
			tempdir,strerror(errno));
		return;
		}

	/* now do the transfer ... in a separate process */
	xfer_pid = fork();
	if (xfer_pid == 0) {
	    chdir(tempdir);
	    switch(c) {
	 	case 0:
			sttybbs(0);
			printf("Receiving %s - press Ctrl-D to end.\n",flnm);
			fp = fopen(flnm,"w");
			do {
				b=inkey(); 
				if (b==13) {
					b=10; printf("\r");
					}
				if (b!=4) {
					printf("%c",b);
					putc(b,fp);
					}
				} while(b!=4);
			fclose(fp);
			exit(0);
	 	case 1:
			sttybbs(3);
			execlp("rx","rx",flnm,NULL);
			exit(1);
	 	case 2:
			sttybbs(3);
			execlp("rb","rb",NULL);
			exit(1);
	 	case 3:
			sttybbs(3);
			execlp("rz","rz",NULL);
			exit(1);
			}
		}
	else do {
		b=ka_wait(&a);
		} while ((b!=xfer_pid)&&(b!=(-1)));
	sttybbs(0);

	if (a != 0) {
		printf("\r*** Transfer unsuccessful.\n");
		nukedir(tempdir);
		return;
		}

	printf("\r*** Transfer successful.  Sending file(s) to server...\n");
	sprintf(buf,"cd %s; ls",tempdir);
	lsfp = popen(buf,"r");
	if (lsfp!=NULL) {
		while (fgets(flnm,256,lsfp)!=NULL) {
			flnm[strlen(flnm)-1] = 0;
			sprintf(buf,"%s/%s",tempdir,flnm);
			fd = open(buf,O_RDONLY);
			if (fd>=0) {
				a = 0;
				do {
					sprintf(buf,"UOPN %s|%s",flnm,desc);
					if (a>0) sprintf(&buf[strlen(buf)],
						".%d",a);
					++a;
					serv_puts(buf);
					serv_gets(buf);
					} while((buf[0]!='2')&&(a<100));
				if (buf[0]=='2') do {
					a=read(fd,tbuf,4096);
					if (a>0) {
						sprintf(buf,"WRIT %d",a);
						serv_puts(buf);
						serv_gets(buf);
						if (buf[0]=='7')
							serv_write(tbuf,a);
						}
					} while (a>0);
				close(fd);
				serv_puts("UCLS 1");
				serv_gets(buf);
				printf("%s\n",&buf[4]);
				}
			}
		pclose(lsfp);
		}

	nukedir(tempdir);
	}

/* 
 * validate a user
 */
void val_user(char *user)
{
	int a,b;
	char cmd[256];
	char buf[256];
	int ax = 0;

	sprintf(cmd,"GREG %s",user);
	serv_puts(cmd);
	serv_gets(cmd);
	if (cmd[0]=='1') {
		a = 0;
		do {
			serv_gets(buf);
			++a;
			if (a==1) printf("User #%s - %s  ",
				buf,&cmd[4]);
			if (a==2) printf("PW: %s\n",buf);
			if (a==3) printf("%s\n",buf);
			if (a==4) printf("%s\n",buf);
			if (a==5) printf("%s, ",buf);
			if (a==6) printf("%s ",buf);
			if (a==7) printf("%s\n",buf);
			if (a==8) printf("%s\n",buf);
			if (a==9) ax=atoi(buf);
			if (a==10) printf("%s\n",buf);
			} while(strcmp(buf,"000"));
		printf("Current access level: %d (%s)\n",ax,axdefs[ax]);
		}
	else {
		printf("%-30s\n%s\n",user,&cmd[4]);
		}

	/* now set the access level */
	do {
		printf("Access level (? for list): ");
		a=inkey();
		if (a=='?') {
			printf("list\n");
			for (b=0; b<7; ++b)
				printf("%d %s\n",b,axdefs[b]);
			}
		a=a-48;
		} while((a<0)||(a>6));
	printf("%d\n\n",a);
	sprintf(cmd,"VALI %s|%d",user,a);
	serv_puts(cmd);
	serv_gets(cmd);
	if (cmd[0]!='2') printf("%s\n",&cmd[4]);
	printf("\n");
	}


void validate(void) {	/* validate new users */
	char cmd[256];
	char buf[256];
	int finished = 0;

	do {
		serv_puts("GNUR");
		serv_gets(cmd);
		if (cmd[0]!='3') finished = 1;
		if (cmd[0]=='2') printf("%s\n",&cmd[4]);
		if (cmd[0]=='3') {
			extract(buf,cmd,0);
			val_user(&buf[4]);
			}
		} while(finished==0);
	}

void subshell(void) {
	int a,b;
	a=fork();
	if (a==0) {
		sttybbs(SB_RESTORE);
		signal(SIGINT,SIG_DFL);
		signal(SIGQUIT,SIG_DFL);
		execlp(getenv("SHELL"),getenv("SHELL"),NULL);
		printf("Could not open a shell: %s\n", strerror(errno));
		exit(errno);
		}
	do {
		b=ka_wait(NULL);
		} while ((a!=b)&&(a!=(-1)));
	sttybbs(0);
	}

/*
 * <.A>ide <F>ile <D>elete command
 */
void deletefile(void) {
	char filename[32];
	char cmd[256];

	newprompt("Filename: ",filename,31);
	if (strlen(filename)==0) return;
	sprintf(cmd,"DELF %s",filename);
	serv_puts(cmd);
	serv_gets(cmd);
	printf("%s\n",&cmd[4]);
	}

/*
 * <.A>ide <F>ile <S>end command
 */
void netsendfile(void) {
	char filename[32],destsys[20],cmd[256];

	newprompt("Filename: ",filename,31);
	if (strlen(filename)==0) return;
	newprompt("System to send to: ",destsys,19);
	sprintf(cmd,"NETF %s|%s",filename,destsys);
	serv_puts(cmd);
	serv_gets(cmd);
	printf("%s\n",&cmd[4]);
	return;
	}

/*
 * <.A>ide <F>ile <M>ove command
 */
void movefile(void) {
	char filename[64];
	char newroom[ROOMNAMELEN];
	char cmd[256];

	newprompt("Filename: ",filename,63);
	if (strlen(filename)==0) return;
	newprompt("Enter target room: ",newroom,ROOMNAMELEN-1);

	sprintf(cmd,"MOVF %s|%s",filename,newroom);
	serv_puts(cmd);
	serv_gets(cmd);
	printf("%s\n",&cmd[4]);
	}


/* 
 * list of users who have filled out a bio
 */
void list_bio(void) {
	char buf[256];
	int pos = 1;

	serv_puts("LBIO");
	serv_gets(buf);
	if (buf[0]!='1') {
		printf("%s\n",&buf[4]);
		return;
		}
	while (serv_gets(buf), strcmp(buf,"000")) {
		if ((pos+strlen(buf)+5)>screenwidth) {
			printf("\n");
			pos = 1;
			}
		printf("%s, ",buf);
		pos = pos + strlen(buf) + 2;
		}
	printf("%c%c  \n\n",8,8);
	}


/*
 * read bio
 */
void read_bio(void) {
	char who[256];
	char buf[256];

	do {
		newprompt("Read bio for who ('?' for list) : ",who,25);
		printf("\n");
		if (!strcmp(who,"?")) list_bio();
		} while(!strcmp(who,"?"));
	sprintf(buf,"RBIO %s",who);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0]!='1') {
		printf("%s\n",&buf[4]);
		return;
		}
	while (serv_gets(buf), strcmp(buf,"000")) {
		printf("%s\n",buf);
		}
	}


/* 
 * General system configuration command
 */
void do_system_configuration() {
	char buf[256];
	int expire_mode = 0;
	int expire_value = 0;

	/* Fetch the expire policy (this will silently fail on old servers,
	 * resulting in "default" policy)
	 */
	serv_puts("GPEX site");
	serv_gets(buf);
	if (buf[0]=='2') {
		expire_mode = extract_int(&buf[4], 0);
		expire_value = extract_int(&buf[4], 1);
		}

	/* Angels and demons dancing in my head... */
	do {
		sprintf(buf, "%d", expire_mode);
		strprompt("System default essage expire policy (? for list)",
			buf, 1);
		if (buf[0] == '?') {
			printf("\n");
			printf("1. Never automatically expire messages\n");
			printf("2. Expire by message count\n");
			printf("3. Expire by message age\n");
			}
		} while((buf[0]<49)||(buf[0]>51));
	expire_mode = buf[0] - 48;

	/* ...lunatics and monsters underneath my bed */
	if (expire_mode == 2) {
		sprintf(buf, "%d", expire_value);
		strprompt("Keep how many messages online?", buf, 10);
		expire_value = atol(buf);
		}

	if (expire_mode == 3) {
		sprintf(buf, "%d", expire_value);
		strprompt("Keep messages for how many days?", buf, 10);
		expire_value = atol(buf);
		}

	/* Save it */
	snprintf(buf, sizeof buf, "SPEX site|%d|%d",
		expire_mode, expire_value);
	serv_puts(buf);
	serv_gets(buf);
	}
