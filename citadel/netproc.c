/*
 * $Id$
 *
 * Citadel/UX Intelligent Network Processor for IGnet/Open networks
 * See copyright.txt for copyright information
 *
 */

/* How long it takes for an old node to drop off the network map */
#define EXPIRY_TIME	(2592000L)

/* How long we keep recently arrived messages in the use table */
#define USE_TIME	(604800L)

/* Where do we keep our lock file? */
#define LOCKFILE	"/tmp/netproc.LCK"

/* Path to the 'uudecode' utility (needed for network file transfers) */
#define UUDECODE	"/usr/bin/uudecode"

/* Files used by the networker */
#define MAILSYSINFO	"./network/mail.sysinfo"

/* Uncomment the DEBUG def to see noisy traces */
#define DEBUG 1


#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <signal.h>
#include <errno.h>
#include <syslog.h>
#include "citadel.h"
#include "tools.h"
#include "ipc.h"

/* A list of users you wish to filter out of incoming traffic can be kept
 * in ./network/filterlist -- messages from these users will be automatically
 * moved to FILTERROOM.  Normally this will be the same as TWITROOM (the
 * room problem user messages are moved to) but you can override this by
 * specifying a different room name here.
 */
#ifndef FILTERROOM
#define FILTERROOM TWITROOM
#endif

struct msglist {
	struct msglist *next;
	long m_num;
	char m_rmname[ROOMNAMELEN];
};

struct rmlist {
	struct rmlist *next;
	char rm_name[ROOMNAMELEN];
	long rm_lastsent;
};

struct filterlist {
	struct filterlist *next;
	char f_person[64];
	char f_room[64];
	char f_system[64];
};

struct syslist {
	struct syslist *next;
	char s_name[16];
	char s_type[4];
	char s_nexthop[128];
	time_t s_lastcontact;
	char s_humannode[64];
	char s_phonenum[32];
	char s_gdom[64];
};



/*
 * This structure is used to hold all of the fields of a message
 * during conversion, processing, or whatever.
 */
struct minfo {
	char A[512];
	char B[512];
	char C[512];
	char D[512];
	char E[512];
	char G[512];
	char H[512];
	char I[512];
	char N[512];
	char O[512];
	char P[512];
	char R[512];
	char S[512];
	long T;
	char U[512];
	char Z[512];
	char nexthop[512];
	};


struct usetable {
	struct usetable *next;
	char msgid[256];
	time_t timestamp;
};




void serv_read(char *buf, int bytes);
void serv_write(char *buf, int nbytes);
void get_config(void);

struct filterlist *filter = NULL;
struct syslist *slist = NULL;
struct msglist *purgelist = NULL;
struct usetable *usetable = NULL;

struct config config;
extern char bbs_home_directory[];
extern int home_specified;


#ifndef HAVE_STRERROR
/*
 * replacement strerror() for systems that don't have it
 */
char *strerror(int e)
{
	static char buf[32];

	sprintf(buf, "errno = %d", e);
	return (buf);
}
#endif


void strip_trailing_whitespace(char *buf)
{
	while (isspace(buf[strlen(buf) - 1]))
		buf[strlen(buf) - 1] = 0;
}


/*
 * we also load the mail.sysinfo table into memory, make changes
 * as we learn more about the network from incoming messages, and write
 * the table back to disk when we're done.
 */
int load_syslist(void)
{
	FILE *fp;
	struct syslist *stemp;
	char insys = 0;
	char buf[128];

	fp = fopen(MAILSYSINFO, "r");
	if (fp == NULL)
		return (1);

	while (1) {
		if (fgets(buf, 128, fp) == NULL) {
			fclose(fp);
			return (0);
		}
		buf[strlen(buf) - 1] = 0;
		while (isspace(buf[0]))
			strcpy(buf, &buf[1]);
		if (buf[0] == '#')
			buf[0] = 0;
		if ((insys == 0) && (strlen(buf) != 0)) {
			insys = 1;
			stemp = (struct syslist *) malloc(sizeof(struct syslist));
			stemp->next = slist;
			slist = stemp;
			strcpy(slist->s_name, buf);
			strcpy(slist->s_type, "bin");
			strcpy(slist->s_nexthop, "Mail");
			slist->s_lastcontact = 0L;
			strcpy(slist->s_humannode, "");
			strcpy(slist->s_phonenum, "");
			strcpy(slist->s_gdom, "");
		} else if ((insys == 1) && (strlen(buf) == 0)) {
			insys = 0;
		} else if ((insys == 1) && (!strncasecmp(buf, "bin", 3))) {
			strcpy(slist->s_type, "bin");
			strcpy(slist->s_nexthop, &buf[4]);
		} else if ((insys == 1) && (!strncasecmp(buf, "use", 3))) {
			strcpy(slist->s_type, "use");
			strcpy(slist->s_nexthop, &buf[4]);
		} else if ((insys == 1) && (!strncasecmp(buf, "uum", 3))) {
			strcpy(slist->s_type, "uum");
			strcpy(slist->s_nexthop, &buf[4]);
		} else if ((insys == 1) && (!strncasecmp(buf, "lastcontact", 11))) {
			long foo;
			sscanf(&buf[12], "%ld", &foo);
			slist->s_lastcontact = foo;
		} else if ((insys == 1) && (!strncasecmp(buf, "humannode", 9))) {
			strcpy(slist->s_humannode, &buf[10]);
		} else if ((insys == 1) && (!strncasecmp(buf, "phonenum", 8))) {
			strcpy(slist->s_phonenum, &buf[9]);
		} else if ((insys == 1) && (!strncasecmp(buf, "gdom", 4))) {
			strcpy(slist->s_gdom, &buf[5]);
		}
	}
}

/* now we have to set up two "special" nodes on the list: one
 * for the local node, and one for an Internet gateway
 */
void setup_special_nodes(void)
{
	struct syslist *stemp, *slocal;

	slocal = NULL;
	for (stemp = slist; stemp != NULL; stemp = stemp->next) {
		if (!strcasecmp(stemp->s_name, config.c_nodename))
			slocal = stemp;
	}
	if (slocal == NULL) {
		slocal = (struct syslist *) malloc(sizeof(struct syslist));
		slocal->next = slist;
		slist = slocal;
	}
	strcpy(slocal->s_name, config.c_nodename);
	strcpy(slocal->s_type, "bin");
	strcpy(slocal->s_nexthop, "Mail");
	time(&slocal->s_lastcontact);
	strcpy(slocal->s_humannode, config.c_humannode);
	strcpy(slocal->s_phonenum, config.c_phonenum);

	slocal = NULL;
	for (stemp = slist; stemp != NULL; stemp = stemp->next) {
		if (!strcasecmp(stemp->s_name, "internet"))
			slocal = stemp;
	}
	if (slocal == NULL) {
		slocal = (struct syslist *) malloc(sizeof(struct syslist));
		slocal->next = slist;
		slist = slocal;
	}
	strcpy(slocal->s_name, "internet");
	strcpy(slocal->s_type, "uum");
	strcpy(slocal->s_nexthop, "%s");
	time(&slocal->s_lastcontact);
	strcpy(slocal->s_humannode, "Internet Gateway");
	strcpy(slocal->s_phonenum, "");
	strcpy(slocal->s_gdom, "");

}

/*
 * here's the routine to write the table back to disk.
 */
void rewrite_syslist(void)
{
	struct syslist *stemp;
	FILE *newfp;
	time_t now;

	time(&now);
	newfp = fopen(MAILSYSINFO, "w");
	for (stemp = slist; stemp != NULL; stemp = stemp->next) {
		if (!strcasecmp(stemp->s_name, config.c_nodename)) {
			time(&stemp->s_lastcontact);
			strcpy(stemp->s_type, "bin");
			strcpy(stemp->s_humannode, config.c_humannode);
			strcpy(stemp->s_phonenum, config.c_phonenum);
		}
		/* remove systems we haven't heard from in a while */
		if ((stemp->s_lastcontact == 0L)
		    || (now - stemp->s_lastcontact < EXPIRY_TIME)) {
			fprintf(newfp, "%s\n%s %s\n",
			 stemp->s_name, stemp->s_type, stemp->s_nexthop);
			if (strlen(stemp->s_phonenum) > 0)
				fprintf(newfp, "phonenum %s\n", stemp->s_phonenum);
			if (strlen(stemp->s_gdom) > 0)
				fprintf(newfp, "gdom %s\n", stemp->s_gdom);
			if (strlen(stemp->s_humannode) > 0)
				fprintf(newfp, "humannode %s\n", stemp->s_humannode);
			if (stemp->s_lastcontact > 0L)
				fprintf(newfp, "lastcontact %ld %s",
					(long) stemp->s_lastcontact,
					asctime(localtime(&stemp->s_lastcontact)));
			fprintf(newfp, "\n");
		}
	}
	fclose(newfp);
	/* now free the list */
	while (slist != NULL) {
		stemp = slist;
		slist = slist->next;
		free(stemp);
	}
}


/* call this function with the node name of a system and it returns a pointer
 * to its syslist structure.
 */
struct syslist *get_sys_ptr(char *sysname)
{
	static char sysnambuf[16];
	static struct syslist *sysptrbuf = NULL;
	struct syslist *stemp;

	if ((!strcmp(sysname, sysnambuf))
	    && (sysptrbuf != NULL))
		return (sysptrbuf);

	strcpy(sysnambuf, sysname);
	for (stemp = slist; stemp != NULL; stemp = stemp->next) {
		if (!strcmp(sysname, stemp->s_name)) {
			sysptrbuf = stemp;
			return (stemp);
		}
	}
	sysptrbuf = NULL;
	return (NULL);
}


/*
 * make sure only one copy of netproc runs at a time, using lock files
 */
int set_lockfile(void)
{
	FILE *lfp;
	int onppid;

	if ((lfp = fopen(LOCKFILE, "r")) != NULL) {
		fscanf(lfp, "%d", &onppid);
		fclose(lfp);
		if (!kill(onppid, 0) || errno == EPERM)
			return 1;
	}
	lfp = fopen(LOCKFILE, "w");
	if (lfp == NULL) {
		syslog(LOG_NOTICE, "Cannot create %s: %s", LOCKFILE,
			strerror(errno));
		return(1);
	}
	fprintf(lfp, "%ld\n", (long) getpid());
	fclose(lfp);
	return (0);
}

void remove_lockfile(void)
{
	unlink(LOCKFILE);
}

/*
 * Why both cleanup() and nq_cleanup() ?  Notice the alarm() call in
 * cleanup() .  If for some reason netproc hangs waiting for the server
 * to clean up, the alarm clock goes off and the program exits anyway.
 * The cleanup() routine makes a check to ensure it's not reentering, in
 * case the ipc module looped it somehow.
 */
void nq_cleanup(int e)
{
	remove_lockfile();
	closelog();
	exit(e);
}

void cleanup(int e)
{
	static int nested = 0;

	alarm(30);
	signal(SIGALRM, nq_cleanup);
	if (nested++ < 1)
		serv_puts("QUIT");
	nq_cleanup(e);
}

/*
 * This is implemented as a function rather than as a macro because the
 * client-side IPC modules expect logoff() to be defined.  They call logoff()
 * when a problem connecting or staying connected to the server occurs.
 */
void logoff(int e)
{
	cleanup(e);
}

/*
 * If there is a kill file in place, this function will process it.
 */
void load_filterlist(void)
{
	FILE *fp;
	struct filterlist *fbuf;
	char sbuf[256];
	int a, p;
	fp = fopen("./network/filterlist", "r");
	if (fp == NULL)
		return;
	while (fgets(sbuf, sizeof sbuf, fp) != NULL) {
		if (sbuf[0] != '#') {
			sbuf[strlen(sbuf) - 1] = 0;
			fbuf = (struct filterlist *)
			    malloc((long) sizeof(struct filterlist));
			fbuf->next = filter;
			filter = fbuf;
			strcpy(fbuf->f_person, "*");
			strcpy(fbuf->f_room, "*");
			strcpy(fbuf->f_system, "*");
			p = (-1);
			for (a = strlen(sbuf); a >= 0; --a)
				if (sbuf[a] == ',')
					p = a;
			if (p >= 0) {
				sbuf[p] = 0;
				strcpy(fbuf->f_person, sbuf);
				strcpy(sbuf, &sbuf[p + 1]);
			}
			for (a = strlen(sbuf); a >= 0; --a)
				if (sbuf[a] == ',')
					p = a;
			if (p >= 0) {
				sbuf[p] = 0;
				strcpy(fbuf->f_room, sbuf);
				strcpy(sbuf, &sbuf[p + 1]);
			}
			strcpy(fbuf->f_system, sbuf);
		}
	}
	fclose(fp);
}

/* returns 1 if user/message/room combination is in the kill file */
int is_banned(char *k_person, char *k_room, char *k_system)
{
	struct filterlist *fptr;

	for (fptr = filter; fptr != NULL; fptr = fptr->next)
		if (
			   ((!strcasecmp(fptr->f_person, k_person)) || (!strcmp(fptr->f_person, "*")))
			   &&
			   ((!strcasecmp(fptr->f_room, k_room)) || (!strcmp(fptr->f_room, "*")))
			   &&
			   ((!strcasecmp(fptr->f_system, k_system)) || (!strcmp(fptr->f_system, "*")))
		    )
			return (1);

	return (0);
}

/*
 * Determine routing from sysinfo file
 */
int get_sysinfo_type(char *name) {
	struct syslist *stemp;

GETSN:	for (stemp = slist; stemp != NULL; stemp = stemp->next) {
		if (!strcasecmp(stemp->s_name, name)) {
			if (!strcasecmp(stemp->s_type, "use")) {
				strcpy(name, stemp->s_nexthop);
				goto GETSN;
			}
			if (!strcasecmp(stemp->s_type, "bin")) {
				return (MES_BINARY);
			}
			if (!strcasecmp(stemp->s_type, "uum")) {
				return (MES_INTERNET);
			}
		}
	}
	syslog(LOG_ERR, "cannot find system '%s' in mail.sysinfo", name);
	return (-1);
}


void fpgetfield(FILE *fp, char *string, int limit)
{
	int a, b;

	strcpy(string, "");
	a = 0;
	do {
		b = getc(fp);
		if ((b < 1) || (a >= limit)) {
			string[a] = 0;
			return;
		}
		string[a] = b;
		++a;
	} while (b != 0);
}



/*
 * Load all of the fields of a message, except the actual text, into a
 * table in memory (so we know how to process the message).
 */
void fpmsgfind(FILE *fp, struct minfo *buffer)
{
	int b, e, mtype, aflag;
	char bbb[1024];
	char userid[1024];

	strcpy(userid, "");
	e = getc(fp);
	if (e != 255) {
		syslog(LOG_ERR, "Magic number check failed for this message");
		goto END;
	}

	memset(buffer, 0, sizeof(struct minfo));
	mtype = getc(fp);
	aflag = getc(fp);

BONFGM:	b = getc(fp);
	if (b < 0)
		goto END;
	if (b == 'M')
		goto END;
	fpgetfield(fp, bbb, sizeof bbb);
	while ((bbb[0] == ' ') && (strlen(bbb) > 1))
		strcpy(bbb, &bbb[1]);
	if (b == 'A') {
		strcpy(buffer->A, bbb);
		if (strlen(userid) == 0) {
			strcpy(userid, bbb);
			for (e = 0; e < strlen(userid); ++e)
				if (userid[e] == ' ')
					userid[e] = '_';
		}
	}
	if (b == 'O')
		strcpy(buffer->O, bbb);
	if (b == 'C')
		strcpy(buffer->C, bbb);
	if (b == 'N')
		strcpy(buffer->N, bbb);
	if (b == 'S')
		strcpy(buffer->S, bbb);
	if (b == 'P') {
		/* extract the user id from the path */
		for (e = 0; e < strlen(bbb); ++e)
			if (bbb[e] == '!')
				strcpy(userid, &bbb[e + 1]);

		/* now find the next hop */
		for (e = 0; e < strlen(bbb); ++e)
			if (bbb[e] == '!')
				bbb[e] = 0;
		strcpy(buffer->nexthop, bbb);
	}
	if (b == 'R') {
		for (e = 0; e < strlen(bbb); ++e)
			if (bbb[e] == '_')
				bbb[e] = ' ';
		strcpy(buffer->R, bbb);
	}
	if (b == 'D')
		strcpy(buffer->D, bbb);
	if (b == 'T')
		buffer->T = atol(bbb);
	if (b == 'I')
		strcpy(buffer->I, bbb);
	if (b == 'H')
		strcpy(buffer->H, bbb);
	if (b == 'B')
		strcpy(buffer->B, bbb);
	if (b == 'G')
		strcpy(buffer->G, bbb);
	if (b == 'E')
		strcpy(buffer->E, bbb);
	if (b == 'Z')
		strcpy(buffer->Z, bbb);
	goto BONFGM;

END:

}


/*
 * msgfind() is the same as fpmsgfind() except it accepts a filename
 * instead of a file handle.
 */
void msgfind(char *msgfile, struct minfo *buffer) {
	FILE *fp;

	fp = fopen(msgfile, "rb");
	if (fp == NULL) {
		syslog(LOG_ERR, "can't open %s: %s", msgfile, strerror(errno));
		return;
	}

	fpmsgfind(fp, buffer);
	fclose(fp);
}





void ship_to(char *filenm, char *sysnm)
{				/* send spool file filenm to system sysnm */
	char sysflnm[100];
	char commbuf1[100];
	char commbuf2[100];
	FILE *sysflfd;

#ifdef DEBUG
	syslog(LOG_NOTICE, "shipping %s to %s", filenm, sysnm);
#endif
	sprintf(sysflnm, "./network/systems/%s", sysnm);
	sysflfd = fopen(sysflnm, "r");
	if (sysflfd == NULL)
		syslog(LOG_ERR, "cannot open %s", sysflnm);
	fgets(commbuf1, 99, sysflfd);
	commbuf1[strlen(commbuf1) - 1] = 0;
	fclose(sysflfd);
	sprintf(commbuf2, commbuf1, filenm);
	system(commbuf2);
}

/*
 * proc_file_transfer()  -  handle a simple file transfer packet
 *
 */
void proc_file_transfer(char *tname)
{				/* name of temp file containing the whole message */
	char buf[256];
	char dest_room[ROOMNAMELEN];
	char subdir_name[256];
	FILE *tfp, *uud;
	int a;

	syslog(LOG_NOTICE, "processing network file transfer...");

	tfp = fopen(tname, "rb");
	if (tfp == NULL)
		syslog(LOG_ERR, "cannot open %s", tname);
	getc(tfp);
	getc(tfp);
	getc(tfp);
	do {
		a = getc(tfp);
		if (a != 'M') {
			fpgetfield(tfp, buf, sizeof buf);
			if (a == 'O') {
				strcpy(dest_room, buf);
			}
		}
	} while ((a != 'M') && (a >= 0));
	if (a != 'M') {
		fclose(tfp);
		syslog(LOG_ERR, "no message text for file transfer");
		return;
	}
	strcpy(subdir_name, "---xxx---");
	sprintf(buf, "GOTO %s", dest_room);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0] == '2') {
		extract(subdir_name, &buf[4], 2);
		if (strlen(subdir_name) == 0)
			strcpy(subdir_name, "--xxx--");
	}
	/* Change to the room's directory; if that fails, change to the
	 * bitbucket directory.  Then run uudecode.
	 */
	sprintf(buf, "(cd %s/files/%s || cd %s/files/%s ) ; exec %s",
		bbs_home_directory, subdir_name,
		bbs_home_directory, config.c_bucket_dir,
		UUDECODE);

	uud = (FILE *) popen(buf, "w");
	if (uud == NULL) {
		syslog(LOG_ERR, "cannot open uudecode pipe");
		fclose(tfp);
		return;
	}
	fgets(buf, 128, tfp);
	buf[strlen(buf) - 1] = 0;
	for (a = 0; a < strlen(buf); ++a)
		if (buf[a] == '/')
			buf[a] = '_';
	fprintf(uud, "%s\n", buf);
	printf("netproc: %s\n", buf);
	while (a = getc(tfp), a > 0)
		putc(a, uud);
	fclose(tfp);
	pclose(uud);
	return;
}


/* send a bounce message */
void bounce(struct minfo *bminfo)
{

	FILE *bounce;
	char bfilename[64];
	static int bseq = 1;
	time_t now;

	sprintf(bfilename, "./network/spoolin/bounce.%ld.%d", (long) getpid(),
		bseq++);
	bounce = fopen(bfilename, "wb");
	time(&now);

	fprintf(bounce, "%c%c%c", 0xFF, MES_NORMAL, 0);
	fprintf(bounce, "Ppostmaster%c", 0);
	fprintf(bounce, "T%ld%c", (long) now, 0);
	fprintf(bounce, "APostmaster%c", 0);
	fprintf(bounce, "OMail%c", 0);
	fprintf(bounce, "N%s%c", config.c_nodename, 0);
	fprintf(bounce, "H%s%c", config.c_humannode, 0);

	if (strlen(bminfo->E) > 0) {
		fprintf(bounce, "R%s%c", bminfo->E, 0);
	} else {
		fprintf(bounce, "R%s%c", bminfo->A, 0);
	}

	fprintf(bounce, "D%s%c", bminfo->N, 0);
	fprintf(bounce, "M%s could not deliver your mail to:\n",
		config.c_humannode);
	fprintf(bounce, " \n %s\n \n", bminfo->R);
	fprintf(bounce, " because there is no such user on this system.\n");
	fprintf(bounce, " (Unsent message does *not* follow.  ");
	fprintf(bounce, "Help to conserve bandwidth.)\n%c", 0);
	fclose(bounce);
}




/*
 * Generate a Message-ID string for the use table
 */
void strmsgid(char *buf, struct minfo *msginfo) {
	int i;

	strcpy(buf, msginfo->I);
	if (strchr(buf, '@') == NULL) {
		strcat(buf, "@");
		strcat(buf, msginfo->N);
	}

	for (i=0; i<strlen(buf); ++i) {
		if (isspace(buf[i])) {
			strcpy(&buf[i], &buf[i+1]);
		}
		buf[i] = tolower(buf[i]);
	}
}



/*
 * Check the use table to see if a message has been here before.
 * Returns 1 if the message is a duplicate; otherwise, it returns
 * 0 and the message ID is added to the use table.
 */
int already_received(struct minfo *msginfo) {
	char buf[256];
	struct usetable *u;
	time_t now;

	/* We can't check for dups on a zero msgid, so just pass them through */
	if (strlen(msginfo->I)==0) {
		return 0;
	}

	strmsgid(buf, msginfo);
	now = time(NULL);

	/* Set return value to 1 if message exists */
	for (u=usetable; u!=NULL; u=u->next) {
		if (!strcasecmp(buf, u->msgid)) {
			u->timestamp = time(NULL);	/* keep it fresh */
			return(1);
		}
	}

	/* Not found, so we're ok, but add it to the use table now */
	u = (struct usetable *) malloc(sizeof (struct usetable));
	u->next = usetable;
	u->timestamp = time(NULL);
	strncpy(u->msgid, buf, 255);
	usetable = u;

	return(0);
}


/*
 * Load the use table from disk
 */
void read_use_table(void) {
	struct usetable *u;
	struct usetable ubuf;
	FILE *fp;

	unlink("data/usetable.gdbm");	/* we don't use this anymore */

	fp = fopen("usetable", "rb");
	if (fp == NULL) return;

	while (fread(&ubuf, sizeof (struct usetable), 1, fp) > 0) {
		u = (struct usetable *) malloc(sizeof (struct usetable));
		memcpy(u, &ubuf, sizeof (struct usetable));
		u->next = usetable;
		usetable = u;
	}

	fclose(fp);
}



/*
 * Purge any old entries out of the use table as we write them back to disk.
 * 
 */
void write_use_table(void) {
	struct usetable *u;
	time_t now;
	FILE *fp;

	now = time(NULL);
	fp = fopen("usetable", "wb");
	if (fp == NULL) return;
	for (u=usetable; u!=NULL; u=u->next) {
		if ((now - u->timestamp) <= USE_TIME) {
			fwrite(u, sizeof(struct usetable), 1, fp);
		}
	}
	fclose(fp);
}





/*
 * process incoming files in ./network/spoolin
 */
void inprocess(void)
{
	FILE *fp, *message, *testfp, *ls, *duplist;
	static struct minfo minfo;
	char tname[128], aaa[1024], iname[256], sfilename[256], pfilename[256];
	int a, b;
	int FieldID;
	struct syslist *stemp;
	char *ptr = NULL;
	char buf[256];
	long msglen;
	int bloklen;
	int valid_msg;

	/* temp file names */
	sprintf(tname, "%s.netproc.%d", tmpnam(NULL), __LINE__);
	sprintf(iname, "%s.netproc.%d", tmpnam(NULL), __LINE__);

	load_filterlist();

	/* Make sure we're in the right directory */
	chdir(bbs_home_directory);

	/* temporary file to contain a log of rejected dups */
	duplist = tmpfile();

	/* Let the shell do the dirty work. Get all data from spoolin */
	do {
		sprintf(aaa, "cd %s/network/spoolin; ls", bbs_home_directory);
		ls = popen(aaa, "r");
		if (ls == NULL) {
			syslog(LOG_ERR, "could not open dir cmd: %s", strerror(errno));
		}
		if (ls != NULL) {
			do {
SKIP:				ptr = fgets(sfilename, sizeof sfilename, ls);
				if (ptr != NULL) {
					sfilename[strlen(sfilename) - 1] = 0;
#ifdef DEBUG
					syslog(LOG_DEBUG,
						"Trying <%s>", sfilename);
#endif
					if (!strcmp(sfilename, ".")) goto SKIP;
					if (!strcmp(sfilename, "..")) goto SKIP;
					if (!strcmp(sfilename, "CVS")) goto SKIP;
					goto PROCESS_IT;
				}
			} while (ptr != NULL);
PROCESS_IT:		pclose(ls);
		}
		if (ptr != NULL) {
			sprintf(pfilename, "%s/network/spoolin/%s", bbs_home_directory, sfilename);
			syslog(LOG_NOTICE, "processing <%s>", pfilename);

			fp = fopen(pfilename, "rb");
			if (fp == NULL) {
				syslog(LOG_ERR, "cannot open %s: %s", pfilename, strerror(errno));
				fp = fopen("/dev/null", "rb");
			}
NXMSG:	/* Seek to the beginning of the next message */
			do {
				a = getc(fp);
			} while ((a != 255) && (a >= 0));
			if (a < 0)
				goto ENDSTR;

			/* This crates the temporary file. */
			valid_msg = 1;
			message = fopen(tname, "wb");
			if (message == NULL) {
				syslog(LOG_ERR, "error creating %s: %s",
					tname, strerror(errno));
				goto ENDSTR;
			}
			putc(255, message);	/* 0xFF (start-of-message) */
			a = getc(fp);
			putc(a, message);	/* type */
			a = getc(fp);
			putc(a, message);	/* mode */
			do {
				FieldID = getc(fp); /* Header field ID */
				if (isalpha(FieldID)) {
					putc(FieldID, message);
					do {
						a = getc(fp);
						if (a < 127) putc(a, message);
					} while (a > 0);
					if (a != 0) putc(0, message);
				}
				else {	/* Invalid field ID; flush it */
					do {
						a = getc(fp);
					} while (a > 0);
					valid_msg = 0;
				}
			} while ((FieldID != 'M') && (a >= 0));
			/* M is always last */
			if (FieldID != 'M') valid_msg = 0;

			msglen = ftell(message);
			fclose(message);

			if (!valid_msg) {
				unlink(tname);
				goto NXMSG;
			}

			/* process the individual mesage */
			msgfind(tname, &minfo);
			syslog(LOG_NOTICE, "#%ld fm <%s> in <%s> @ <%s>",
			       minfo.I, minfo.A, minfo.O, minfo.N);
			if (strlen(minfo.R) > 0) {
				syslog(LOG_NOTICE, "     to <%s>", minfo.R);
				if (strlen(minfo.D) > 0) {
					syslog(LOG_NOTICE, "     @ <%s>",
						minfo.D);
				}
			}
			if (!strcasecmp(minfo.D, FQDN))
				strcpy(minfo.D, NODENAME);

/* this routine updates our info on the system that sent the message */
			stemp = get_sys_ptr(minfo.N);
			if ((stemp == NULL) && (get_sys_ptr(minfo.nexthop) != NULL)) {
				/* add non-neighbor system to map */
				syslog(LOG_NOTICE, "Adding non-neighbor system <%s> to map",
				       slist->s_name);
				stemp = (struct syslist *) malloc((long) sizeof(struct syslist));
				stemp->next = slist;
				slist = stemp;
				strcpy(slist->s_name, minfo.N);
				strcpy(slist->s_type, "use");
				strcpy(slist->s_nexthop, minfo.nexthop);
				time(&slist->s_lastcontact);
			} else if ((stemp == NULL) && (!strcasecmp(minfo.N, minfo.nexthop))) {
				/* add neighbor system to map */
				syslog(LOG_NOTICE, "Adding neighbor system <%s> to map",
				       slist->s_name);
				sprintf(aaa, "%s/network/systems/%s", bbs_home_directory, minfo.N);
				testfp = fopen(aaa, "r");
				if (testfp != NULL) {
					fclose(testfp);
					stemp = (struct syslist *)
					    malloc((long) sizeof(struct syslist));
					stemp->next = slist;
					slist = stemp;
					strcpy(slist->s_name, minfo.N);
					strcpy(slist->s_type, "bin");
					strcpy(slist->s_nexthop, "Mail");
					time(&slist->s_lastcontact);
				}
			}
			/* now update last contact and long node name if we can */
			if (stemp != NULL) {
				time(&stemp->s_lastcontact);
				if (strlen(minfo.H) > 0)
					strcpy(stemp->s_humannode, minfo.H);
				if (strlen(minfo.B) > 0)
					strcpy(stemp->s_phonenum, minfo.B);
				if (strlen(minfo.G) > 0)
					strcpy(stemp->s_gdom, minfo.G);
			}

			/* Check the use table; reject message if it's been here before */
			if (already_received(&minfo)) {
				syslog(LOG_NOTICE, "rejected duplicate message");
				fprintf(duplist, "#<%s> fm <%s> in <%s> @ <%s>\n",
			       		minfo.I, minfo.A, minfo.O, minfo.N);
			}


			/* route the message if necessary */
			else if ((strcasecmp(minfo.D, NODENAME)) && (minfo.D[0] != 0)) {
				a = get_sysinfo_type(minfo.D);
				syslog(LOG_NOTICE, "routing message to system <%s>", minfo.D);
				fflush(stdout);
				if (a == MES_INTERNET) {
					if (fork() == 0) {
						syslog(LOG_NOTICE, "netmailer %s", tname);
						fflush(stdout);
						execlp("./netmailer", "netmailer",
						       tname, NULL);
						syslog(LOG_ERR, "error running netmailer: %s",
						       strerror(errno));
						exit(errno);
					} else
						while (wait(&b) != (-1));
				} else if (a == MES_BINARY) {
					ship_to(tname, minfo.D);
				} else {
					/* message falls into the bit bucket? */
				}
			}

			/* check to see if it's a file transfer */
			else if (!strncasecmp(minfo.S, "FILE", 4)) {
				proc_file_transfer(tname);
			}

			/* otherwise process it as a normal message */
			else {
				if (!strcasecmp(minfo.R, "postmaster")) {
					strcpy(minfo.R, "");
					strcpy(minfo.C, "Aide");
				}
				if (strlen(minfo.R) > 0) {
					sprintf(buf, "GOTO _MAIL_");
				}
				if (is_banned(minfo.A, minfo.C, minfo.N)) {
					sprintf(buf, "GOTO %s", FILTERROOM);
				} else {
					if (strlen(minfo.C) > 0) {
						sprintf(buf, "GOTO %s", minfo.C);
					} else {
						sprintf(buf, "GOTO %s", minfo.O);
					}
				}
				serv_puts(buf);
				serv_gets(buf);
				if (buf[0] != '2') {
					syslog(LOG_ERR, "%s", buf);
					sprintf(buf, "GOTO _BITBUCKET_");
					serv_puts(buf);
					serv_gets(buf);
				}
				/* Open the temporary file containing the message */
				message = fopen(tname, "rb");
				if (message == NULL) {
					syslog(LOG_ERR, "cannot open %s: %s",
					       tname, strerror(errno));
					unlink(tname);
					goto NXMSG;
				}
				/* Transmit the message to the server */
				sprintf(buf, "ENT3 1|%s|%ld", minfo.R, msglen);
				serv_puts(buf);
				serv_gets(buf);
				if (!strncmp(buf, "570", 3)) {
					/* no such user, do a bounce */
					bounce(&minfo);
				}
				if (buf[0] == '7') {
					/* Always use the server's idea of the message length,
					 * even though they should both be identical */
					msglen = atol(&buf[4]);
					while (msglen > 0L) {
						bloklen = ((msglen >= 255L) ? 255 : ((int) msglen));
						if (fread(buf, bloklen, 1, message) < 1) {
							syslog(LOG_ERR,
							       "error trying to read %d bytes: %s",
							       bloklen, strerror(errno));
						}
						serv_write(buf, bloklen);
						msglen = msglen - (long) bloklen;
					}
					serv_puts("NOOP");
					serv_gets(buf);
				} else {
					syslog(LOG_ERR, "%s", buf);
				}

				fclose(message);
				
			}

			unlink(tname);
			goto NXMSG;

ENDSTR:			fclose(fp);
			unlink(pfilename);
		}
	} while (ptr != NULL);
	unlink(iname);


	/*
	 * If dups were rejected, post a message saying so
	 */
	if (ftell(duplist)!=0L) {
		fp = fopen("./network/spoolin/ctdl_rejects", "ab");
		if (fp != NULL) {
			fprintf(fp, "%cA%c", 255, 1);
			fprintf(fp, "T%ld%c", time(NULL), 0);
			fprintf(fp, "ACitadel%c", 0);
			fprintf(fp, "OAide%cM", 0);
			fprintf(fp, "The following duplicate messages"
				" were rejected:\n \n");
			rewind(duplist);
			while (fgets(buf, sizeof(buf), duplist) != NULL) {
				buf[strlen(buf)-1] = 0;
				fprintf(fp, " %s\n", buf);
			}
			fprintf(fp, "%c", 0);
			pclose(fp);
		}
	}

	fclose(duplist);

}


/* Checks to see whether its ok to send */
/* Returns 1 for ok, send message       */
/* Returns 0 if message already there   */
int checkpath(char *path, char *sys)
{
	int a;
	char sys2[512];
	strcpy(sys2, sys);
	strcat(sys2, "!");

#ifdef DEBUG
	syslog(LOG_NOTICE, "checkpath <%s> <%s> ... ", path, sys);
#endif
	for (a = 0; a < strlen(path); ++a) {
		if (!strncmp(&path[a], sys2, strlen(sys2)))
			return (0);
	}
	return (1);
}

/*
 * Implement split horizon algorithm (prevent infinite spooling loops
 * by refusing to send any node a message which already contains its
 * nodename in the path).
 */
int ismsgok(FILE *mmfp, char *sysname)
{
	int a;
	int ok = 0;		/* fail safe - no path, don't send it */
	char fbuf[256];

	fseek(mmfp, 0L, 0);
	if (getc(mmfp) != 255)
		return (0);
	getc(mmfp);
	getc(mmfp);

	while (a = getc(mmfp), ((a != 'M') && (a != 0))) {
		fpgetfield(mmfp, fbuf, sizeof fbuf);
		if (a == 'P') {
			ok = checkpath(fbuf, sysname);
		}
	}
#ifdef DEBUG
	syslog(LOG_NOTICE, "%s", ((ok) ? "SEND" : "(no)"));
#endif
	return (ok);
}




/*
 * Add a message to the list of messages to be deleted off the local server
 * at the end of this run.
 */
void delete_locally(long msgid, char *roomname) {
	struct msglist *mptr;

	mptr = (struct msglist *) malloc(sizeof(struct msglist));
	mptr->next = purgelist;
	mptr->m_num = msgid;
	strcpy(mptr->m_rmname, roomname);
	purgelist = mptr;
}



/*
 * Delete all messages on the purge list from the local server.
 */
void process_purgelist(void) {
	char curr_rm[ROOMNAMELEN];
	char buf[256];
	struct msglist *mptr;


	strcpy(curr_rm, "__nothing__");
	while (purgelist != NULL) {
		if (strcasecmp(curr_rm, purgelist->m_rmname)) {
			sprintf(buf, "GOTO %s", purgelist->m_rmname);
			serv_puts(buf);
			serv_gets(buf);
			if (buf[0] == '2') {
				extract(curr_rm, &buf[4], 0);
			}
			else {
				syslog(LOG_ERR, "%s", buf);
			}
		}
		if (!strcasecmp(curr_rm, purgelist->m_rmname)) {
			syslog(LOG_NOTICE, "Purging <%ld> in <%s>",
				purgelist->m_num, purgelist->m_rmname);
			sprintf(buf, "DELE %ld", purgelist->m_num);
			serv_puts(buf);
			serv_gets(buf);
			if (buf[0] != '2') {
				syslog(LOG_ERR, "%s", buf);
			}

		}
		mptr = purgelist->next;
		free(purgelist);
		purgelist = mptr;
	}
}




/* spool list of messages to a file */
/* returns # of msgs spooled */
int spool_out(struct msglist *cmlist, FILE * destfp, char *sysname)
{
	struct msglist *cmptr;
	FILE *mmfp;
	char fbuf[1024];
	int a;
	int msgs_spooled = 0;
	long msg_len;
	int blok_len;
	static struct minfo minfo;

	char buf[256];
	char curr_rm[256];

	strcpy(curr_rm, "");

	/* for each message in the list... */
	for (cmptr = cmlist; cmptr != NULL; cmptr = cmptr->next) {

		/* make sure we're in the correct room... */
		if (strcasecmp(curr_rm, cmptr->m_rmname)) {
			sprintf(buf, "GOTO %s", cmptr->m_rmname);
			serv_puts(buf);
			serv_gets(buf);
			if (buf[0] == '2') {
				strcpy(curr_rm, cmptr->m_rmname);
			} else {
				syslog(LOG_ERR, "%s", buf);
			}
		}
		/* download the message from the server... */
		mmfp = tmpfile();
		if (mmfp == NULL) {
			syslog(LOG_NOTICE, "tmpfile() failed: %s\n",
				strerror(errno) );
		}
		sprintf(buf, "MSG3 %ld", cmptr->m_num);
		serv_puts(buf);
		serv_gets(buf);
		if (buf[0] == '6') {	/* read the msg */
			msg_len = atol(&buf[4]);
			while (msg_len > 0L) {
				blok_len = ((msg_len >= 256L) ? 256 : (int) msg_len);
				serv_read(buf, blok_len);
				fwrite(buf, blok_len, 1, mmfp);
				msg_len = msg_len - (long) blok_len;
			}
		} else {	/* or print the err */
			syslog(LOG_ERR, "%s", buf);
		}

		rewind(mmfp);

		if (ismsgok(mmfp, sysname)) {
			++msgs_spooled;
			fflush(stdout);
			fseek(mmfp, 0L, 0);
			fread(fbuf, 3, 1, mmfp);
			fwrite(fbuf, 3, 1, destfp);
			while (a = getc(mmfp), ((a != 0) && (a != 'M'))) {
				if (a != 'C') {
					putc(a, destfp);
				}
				fpgetfield(mmfp, fbuf, sizeof fbuf);
				if (a == 'P') {
					fprintf(destfp, "%s!", NODENAME);
				}
				if (a != 'C') {
					fwrite(fbuf, strlen(fbuf) + 1, 1, destfp);
				}
				if (a == 'S') if (!strcasecmp(fbuf, "CANCEL")) {
					delete_locally(cmptr->m_num, cmptr->m_rmname);
				}
			}
			if (a == 'M') {
				fprintf(destfp, "C%s%c",
					cmptr->m_rmname, 0);
				putc('M', destfp);
				do {
					a = getc(mmfp);
					putc(a, destfp);
				} while (a > 0);
			}

		/* Get this message into the use table, so we can reject it
		 * if a misconfigured remote system sends it back to us.
		 */
		fseek(mmfp, 0L, 0);
		fpmsgfind(mmfp, &minfo);
		already_received(&minfo);

		}
		fclose(mmfp);
	}

	return (msgs_spooled);
}

void outprocess(char *sysname)
{				/* send new room messages to sysname */
	char sysflnm[64];
	char srmname[32];
	char shiptocmd[128];
	char lbuf[64];
	char tempflnm[64];
	char buf[256];
	struct msglist *cmlist = NULL;
	struct msglist *cmlast = NULL;
	struct rmlist *crmlist = NULL;
	struct rmlist *rmptr, *rmptr2;
	struct msglist *cmptr;
	FILE *sysflfp, *tempflfp;
	int outgoing_msgs = 0;
	long thismsg;

	sprintf(tempflnm, "%s.netproc.%d", tmpnam(NULL), __LINE__);
	tempflfp = fopen(tempflnm, "w");
	if (tempflfp == NULL)
		return;


/*
 * Read system file for node in question and put together room list
 */
	sprintf(sysflnm, "%s/network/systems/%s", bbs_home_directory, sysname);
	sysflfp = fopen(sysflnm, "r");
	if (sysflfp == NULL)
		return;
	fgets(shiptocmd, 128, sysflfp);
	shiptocmd[strlen(shiptocmd) - 1] = 0;
	while (!feof(sysflfp)) {
		if (fgets(srmname, 32, sysflfp) == NULL)
			break;
		srmname[strlen(srmname) - 1] = 0;
		fgets(lbuf, 32, sysflfp);
		rmptr = (struct rmlist *) malloc(sizeof(struct rmlist));
		rmptr->next = NULL;
		strcpy(rmptr->rm_name, srmname);
		strip_trailing_whitespace(rmptr->rm_name);
		rmptr->rm_lastsent = atol(lbuf);
		if (crmlist == NULL)
			crmlist = rmptr;
		else if (!strcasecmp(rmptr->rm_name, "control")) {
			/* control has to be first in room list */
			rmptr->next = crmlist;
			crmlist = rmptr;
		} else {
			rmptr2 = crmlist;
			while (rmptr2->next != NULL)
				rmptr2 = rmptr2->next;
			rmptr2->next = rmptr;
		}
	}
	fclose(sysflfp);

/*
 * Assemble list of messages to be spooled
 */
	for (rmptr = crmlist; rmptr != NULL; rmptr = rmptr->next) {

		sprintf(buf, "GOTO %s", rmptr->rm_name);
		serv_puts(buf);
		serv_gets(buf);
		if (buf[0] != '2') {
			syslog(LOG_ERR, "%s", buf);
		} else {
			sprintf(buf, "MSGS GT|%ld", rmptr->rm_lastsent);
			serv_puts(buf);
			serv_gets(buf);
			if (buf[0] == '1')
				while (serv_gets(buf), strcmp(buf, "000")) {
					thismsg = atol(buf);
					if (thismsg > (rmptr->rm_lastsent)) {
						rmptr->rm_lastsent = thismsg;

						cmptr = (struct msglist *)
						    malloc(sizeof(struct msglist));
						cmptr->next = NULL;
						cmptr->m_num = thismsg;
						strcpy(cmptr->m_rmname, rmptr->rm_name);

						if (cmlist == NULL) {
							cmlist = cmptr;
						}
						else {
							cmlast->next = cmptr;
						}
						cmlast = cmptr;
						++outgoing_msgs;
					}
			} else {	/* print error from "msgs all" */
				syslog(LOG_ERR, "%s", buf);
			}
		}
	}

	syslog(LOG_NOTICE, "%d messages to be spooled to %s",
	       outgoing_msgs, sysname);

/*
 * Spool out the messages, but only if there are any.
 */
	if (outgoing_msgs != 0) {
		outgoing_msgs = spool_out(cmlist, tempflfp, sysname);
	}

	syslog(LOG_NOTICE, "%d messages actually spooled", outgoing_msgs);

/*
 * Deallocate list of spooled messages.
 */
	while (cmlist != NULL) {
		cmptr = cmlist->next;
		free(cmlist);
		cmlist = cmptr;
	}

/*
 * Rewrite system file and deallocate room list.
 */
	syslog(LOG_NOTICE, "Spooling...");
	sysflfp = fopen(sysflnm, "w");
	fprintf(sysflfp, "%s\n", shiptocmd);
	for (rmptr = crmlist; rmptr != NULL; rmptr = rmptr->next)
		fprintf(sysflfp, "%s\n%ld\n", rmptr->rm_name, rmptr->rm_lastsent);
	fclose(sysflfp);
	while (crmlist != NULL) {
		rmptr = crmlist->next;
		free(crmlist);
		crmlist = rmptr;
	}

/* 
 * Close temporary file, ship it out, and return
 */
	fclose(tempflfp);
	if (outgoing_msgs != 0)
		ship_to(tempflnm, sysname);
	unlink(tempflnm);
}


/*
 * Connect netproc to the Citadel server running on this computer.
 */
void np_attach_to_server(void)
{
	char buf[256];
	char *args[] =
	{ "netproc", NULL };

	syslog(LOG_NOTICE, "Attaching to server...");
	attach_to_server(1, args, NULL, NULL);
	serv_gets(buf);
	syslog(LOG_NOTICE, "%s", &buf[4]);
	sprintf(buf, "IPGM %d", config.c_ipgm_secret);
	serv_puts(buf);
	serv_gets(buf);
	syslog(LOG_NOTICE, "%s", &buf[4]);
	if (buf[0] != '2') {
		cleanup(2);
	}
}



/*
 * main
 */
int main(int argc, char **argv)
{
	char allst[32];
	FILE *allfp;
	int a;
	int import_only = 0;	/* if set to 1, don't export anything */

	openlog("netproc", LOG_PID, LOG_USER);
	strcpy(bbs_home_directory, BBSDIR);

	/*
	 * Change directories if specified
	 */
	for (a = 1; a < argc; ++a) {
		if (!strncmp(argv[a], "-h", 2)) {
			strcpy(bbs_home_directory, argv[a]);
			strcpy(bbs_home_directory, &bbs_home_directory[2]);
			home_specified = 1;
		} else if (!strcmp(argv[a], "-i")) {
			import_only = 1;
		} else {
			fprintf(stderr, "netproc: usage: ");
			fprintf(stderr, "netproc [-hHomeDir] [-i]\n");
			exit(1);
		}
	}

#ifdef DEBUG
	syslog(LOG_DEBUG, "Calling get_config()");
#endif
	get_config();

#ifdef DEBUG
	syslog(LOG_DEBUG, "Creating lock file");
#endif
	if (set_lockfile() != 0) {
		syslog(LOG_NOTICE, "lock file exists: already running");
		cleanup(1);
	}
	signal(SIGINT, cleanup);
	signal(SIGQUIT, cleanup);
	signal(SIGHUP, cleanup);
	signal(SIGTERM, cleanup);

	syslog(LOG_NOTICE, "started.  pid=%d", getpid());
	fflush(stdout);
	np_attach_to_server();
	fflush(stdout);

	if (load_syslist() != 0)
		syslog(LOG_ERR, "cannot load sysinfo");
	setup_special_nodes();

	/* Open the use table */
	read_use_table();

	/* first collect incoming stuff */
	inprocess();

	/* Now process outbound messages, but NOT if this is just a
	 * quick import-only run (i.e. the -i command-line option
	 * was specified)
	 */
	if (import_only != 1) {
		allfp = (FILE *) popen("cd ./network/systems; ls", "r");
		if (allfp != NULL) {
			while (fgets(allst, 32, allfp) != NULL) {
				allst[strlen(allst) - 1] = 0;
				if (strcmp(allst, "CVS"))
					outprocess(allst);
			}
			pclose(allfp);
		}
		/* import again in case anything new was generated */
		inprocess();
	}

	/* Update mail.sysinfo with new information we learned */
	rewrite_syslist();

	/* Delete any messages which need to be purged locally */
	syslog(LOG_NOTICE, "calling process_purgelist()");
	process_purgelist();

	/* Close the use table */
	write_use_table();

	syslog(LOG_NOTICE, "processing ended.");
	cleanup(0);
	return 0;
}
