#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include "citadel.h"
#include "server.h"
#include "proto.h"

extern struct config config;

FILE *popen(const char *, const char *);

/*
 * is_known()  -  returns nonzero if room is in user's known room list
 */
int is_known(struct quickroom *roombuf, int roomnum, struct usersupp *userbuf)
{

	/* for internal programs, always succeed */
	if (((CC->internal_pgm))&&(roombuf->QRflags & QR_INUSE)) return(1);

	/* for regular rooms, check the permissions */
	if ((roombuf->QRflags & QR_INUSE)
		&& ( (roomnum!=2) || (userbuf->axlevel>=6))
		&& (roombuf->QRgen != (userbuf->forget[roomnum]) )

		&& (	((roombuf->QRflags&QR_PREFONLY)==0)
		||	((userbuf->axlevel)>=5)
		)

		&& (	((roombuf->QRflags&QR_PRIVATE)==0)
   		||	((userbuf->axlevel)>=6)
   		||	(roombuf->QRgen==(userbuf->generation[roomnum]))
   		)

		) return(1);
	else return(0);
	}


/*
 * has_newmsgs()  -  returns nonzero if room has new messages
 */
int has_newmsgs(struct quickroom *roombuf, int roomnum, struct usersupp *userbuf)
{
	if (roombuf->QRhighest > (userbuf->lastseen[roomnum]) )
		return(1);
	else return(0);
	}

/*
 * is_zapped()  -  returns nonzero if room is on forgotten rooms list
 */
int is_zapped(struct quickroom *roombuf, int roomnum, struct usersupp *userbuf)
{
	if ((roombuf->QRflags & QR_INUSE)
		&& (roombuf->QRgen == (userbuf->forget[roomnum]) )
		&& ( (roomnum!=2) || ((userbuf->axlevel)>=6))
		&& (	((roombuf->QRflags&QR_PRIVATE)==0)
   		||	((userbuf->axlevel)>=6)
   		||	(roombuf->QRgen==(userbuf->generation[roomnum]))
   		)
		) return(1);
	else return(0);
	}

/*
 * getroom()  -  retrieve room data from disk
 */
void getroom(struct quickroom *qrbuf, int room_num)
{
	struct cdbdata *cdbqr;
	int a;

	bzero(qrbuf, sizeof(struct quickroom));
	cdbqr = cdb_fetch(CDB_QUICKROOM, &room_num, sizeof(int));
	if (cdbqr != NULL) {
		memcpy(qrbuf, cdbqr->ptr, cdbqr->len);
		cdb_free(cdbqr);
		}
	else {
		if (room_num < 3) {
			qrbuf->QRflags = QR_INUSE;
			qrbuf->QRgen = 1;
			switch(room_num) {
				case 0:	strcpy(qrbuf->QRname, "Lobby");
					break;
				case 1:	strcpy(qrbuf->QRname, "Mail");
					break;
				case 2:	strcpy(qrbuf->QRname, "Aide");
					break;
				}
			}
		}


	/** FIX **   VILE SLEAZY HACK ALERT!!  
	 * This is a temporary fix until I can track down where room names
	 * are getting corrupted on some systems.
	 */
	for (a=0; a<20; ++a) if (qrbuf->QRname[a] < 32) qrbuf->QRname[a] = 0;
	qrbuf->QRname[19] = 0;
	}

/*
 * lgetroom()  -  same as getroom() but locks the record (if supported)
 */
void lgetroom(struct quickroom *qrbuf, int room_num)
{
	begin_critical_section(S_QUICKROOM);
	getroom(qrbuf,room_num);
	}


/*
 * putroom()  -  store room data on disk
 */
void putroom(struct quickroom *qrbuf, int room_num)
{

	cdb_store(CDB_QUICKROOM, &room_num, sizeof(int),
		qrbuf, sizeof(struct quickroom));
	}


/*
 * lputroom()  -  same as putroom() but unlocks the record (if supported)
 */
void lputroom(struct quickroom *qrbuf, int room_num)
{

	putroom(qrbuf,room_num);
	end_critical_section(S_QUICKROOM);

	}


/*
 * getfloor()  -  retrieve floor data from disk
 */
void getfloor(struct floor *flbuf, int floor_num)
{
	struct cdbdata *cdbfl;

	bzero(flbuf, sizeof(struct floor));
	cdbfl = cdb_fetch(CDB_FLOORTAB, &floor_num, sizeof(int));
	if (cdbfl != NULL) {
		memcpy(flbuf, cdbfl->ptr, cdbfl->len);
		cdb_free(cdbfl);
		}
	else {
		if (floor_num == 0) {
			strcpy(flbuf->f_name, "Main Floor");
			flbuf->f_flags = F_INUSE;
			flbuf->f_ref_count = 3;
			}
		}

	}

/*
 * lgetfloor()  -  same as getfloor() but locks the record (if supported)
 */
void lgetfloor(struct floor *flbuf, int floor_num)
{

	begin_critical_section(S_FLOORTAB);
	getfloor(flbuf,floor_num);
	}


/*
 * putfloor()  -  store floor data on disk
 */
void putfloor(struct floor *flbuf, int floor_num)
{
	cdb_store(CDB_FLOORTAB, &floor_num, sizeof(int),
		flbuf, sizeof(struct floor));
	}


/*
 * lputfloor()  -  same as putfloor() but unlocks the record (if supported)
 */
void lputfloor(struct floor *flbuf, int floor_num)
{

	putfloor(flbuf,floor_num);
	end_critical_section(S_FLOORTAB);

	}



/*
 * get_msglist()  -  retrieve room message pointers
 */
void get_msglist(int room_num)
{
	struct cdbdata *cdbfr;

	if (CC->msglist != NULL) {
		free(CC->msglist);
		}
	CC->msglist = NULL;
	CC->num_msgs = 0;

	if (room_num != 1) {
		cdbfr = cdb_fetch(CDB_MSGLISTS, &room_num, sizeof(int));
		}
	else {
		cdbfr = cdb_fetch(CDB_MAILBOXES, &CC->usersupp.usernum,
					sizeof(long));
		}

	if (cdbfr == NULL) {
		return;
		}

	CC->msglist = malloc(cdbfr->len);
	memcpy(CC->msglist, cdbfr->ptr, cdbfr->len);
	CC->num_msgs = cdbfr->len / sizeof(long);
	cdb_free(cdbfr);
	}


/*
 * put_msglist()  -  retrieve room message pointers
 */
void put_msglist(int room_num)
{

	if (room_num != 1) {
		cdb_store(CDB_MSGLISTS, &room_num, sizeof(int),
			CC->msglist, (CC->num_msgs * sizeof(long)) );
		}
	else {
		cdb_store(CDB_MAILBOXES, &CC->usersupp.usernum, sizeof(long),
			CC->msglist, (CC->num_msgs * sizeof(long)) );
		}
	}


/*
 * MessageFromList()  -  get a message number from the list currently in memory
 */
long MessageFromList(int whichpos) {

	/* Return zero if the position is invalid */
	if (whichpos >= CC->num_msgs) return 0L;

	return(CC->msglist[whichpos]);
	}

/* 
 * SetMessageInList()  -  set a message number in the list currently in memory
 */
void SetMessageInList(int whichpos, long newmsgnum) {

	/* Return zero if the position is invalid */
	if (whichpos >= CC->num_msgs) return;

	CC->msglist[whichpos] = newmsgnum;
	}



/*
 * sort message pointers
 * (returns new msg count)
 */
int sort_msglist(long listptrs[], int oldcount)
{
	int a,b;
	long hold1, hold2;
	int numitems;

	numitems = oldcount;
	if (numitems < 2) return(oldcount);

	/* do the sort */
	for (a=numitems-2; a>=0; --a) {
		for (b=0; b<=a; ++b) {
			if (listptrs[b] > (listptrs[b+1])) {
				hold1 = listptrs[b];
				hold2 = listptrs[b+1];
				listptrs[b] = hold2;
				listptrs[b+1] = hold1;
				}
			}
		}

	/* and yank any nulls */
	while ( (numitems > 0) && (listptrs[0] == 0L) ) {
		memcpy(&listptrs[0], &listptrs[1],
			(sizeof(long) * (CC->num_msgs - 1)) );
		--numitems;
		}

	return(numitems);
	}

 


/* 
 * cmd_lrms()   -  List all accessible rooms, known or forgotten
 */
void cmd_lrms(char *argbuf)
{
	int a;
	int target_floor = (-1);
	struct quickroom qrbuf;

	if (strlen(argbuf)>0) target_floor = extract_int(argbuf,0);

	if (!(CC->logged_in)) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
		}

	if (getuser(&CC->usersupp,CC->curr_user)) {
		cprintf("%d Can't locate user!\n",ERROR+INTERNAL_ERROR);
		return;
		}

	cprintf("%d Accessible rooms:\n",LISTING_FOLLOWS);
	
	for (a=0; a<MAXROOMS; ++a) {
		getroom(&qrbuf,a);
		if ( ( (is_known(&qrbuf,a,&CC->usersupp))
		   ||	(is_zapped(&qrbuf,a,&CC->usersupp)) )
		&& ((qrbuf.QRfloor == target_floor)||(target_floor<0)) )
			cprintf("%s|%u|%d\n",
				qrbuf.QRname,qrbuf.QRflags,qrbuf.QRfloor);
		}
	cprintf("000\n");
	}

/* 
 * cmd_lkra()   -  List all known rooms
 */
void cmd_lkra(char *argbuf)
{
	int a;
	struct quickroom qrbuf;
	int target_floor = (-1);

	if (strlen(argbuf)>0) target_floor = extract_int(argbuf,0);

	if ((!(CC->logged_in))&&(!(CC->internal_pgm))) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
		}

	if (!(CC->internal_pgm)) if (getuser(&CC->usersupp,CC->curr_user)) {
		cprintf("%d Can't locate user!\n",ERROR+INTERNAL_ERROR);
		return;
		}

	cprintf("%d Known rooms:\n",LISTING_FOLLOWS);
	
	for (a=0; a<MAXROOMS; ++a) {
		getroom(&qrbuf,a);
		if ((is_known(&qrbuf,a,&CC->usersupp))
		   && ((qrbuf.QRfloor == target_floor)||(target_floor<0)) )
			cprintf("%s|%u|%d\n",
				qrbuf.QRname,qrbuf.QRflags,qrbuf.QRfloor);
		}
	cprintf("000\n");
	}

/* 
 * cmd_lkrn()   -  List Known Rooms with New messages
 */
void cmd_lkrn(char *argbuf)
{
	int a;
	struct quickroom qrbuf;
	int target_floor = (-1);

	if (strlen(argbuf)>0) target_floor = extract_int(argbuf,0);

	if (!(CC->logged_in)) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
		}

	if (getuser(&CC->usersupp,CC->curr_user)) {
		cprintf("%d can't locate user\n",ERROR+INTERNAL_ERROR);
		return;
		}

	cprintf("%d list of rms w/ new msgs\n",LISTING_FOLLOWS);
	
	for (a=0; a<MAXROOMS; ++a) {
		getroom(&qrbuf,a);
		if ( ( (is_known(&qrbuf,a,&CC->usersupp))
		   &&	(has_newmsgs(&qrbuf,a,&CC->usersupp)) )
		   && ((qrbuf.QRfloor == target_floor)||(target_floor<0)) )
			cprintf("%s|%u|%d\n",
				qrbuf.QRname,qrbuf.QRflags,qrbuf.QRfloor);
		}
	cprintf("000\n");
	}

/* 
 * cmd_lkro()   -  List Known Rooms with Old (no new) messages
 */
void cmd_lkro(char *argbuf)
{
	int a;
	struct quickroom qrbuf;
	int target_floor = (-1);

	if (strlen(argbuf)>0) target_floor = extract_int(argbuf,0);

	if (!(CC->logged_in)) {
		cprintf("%d not logged in\n",ERROR+NOT_LOGGED_IN);
		return;
		}

	if (getuser(&CC->usersupp,CC->curr_user)) {
		cprintf("%d can't locate user\n",ERROR+INTERNAL_ERROR);
		return;
		}

	cprintf("%d list of rms w/o new msgs\n",LISTING_FOLLOWS);
	
	for (a=0; a<MAXROOMS; ++a) {
		getroom(&qrbuf,a);
		if ( ( (is_known(&qrbuf,a,&CC->usersupp))
		   &&	(!has_newmsgs(&qrbuf,a,&CC->usersupp)) ) 
		   && ((qrbuf.QRfloor == target_floor)||(target_floor<0)) ) {
			if (!strcmp(qrbuf.QRname,"000")) cprintf(">");
			cprintf("%s|%u|%d\n",
				qrbuf.QRname,qrbuf.QRflags,qrbuf.QRfloor);
			}
		}
	cprintf("000\n");
	}

/* 
 * cmd_lzrm()   -  List Zapped RooMs
 */
void cmd_lzrm(char *argbuf)
{
	int a;
	struct quickroom qrbuf;
	int target_floor = (-1);

	if (strlen(argbuf)>0) target_floor = extract_int(argbuf,0);

	if (!(CC->logged_in)) {
		cprintf("%d not logged in\n",ERROR+NOT_LOGGED_IN);
		return;
		}

	if (getuser(&CC->usersupp,CC->curr_user)) {
		cprintf("%d can't locate user\n",ERROR+INTERNAL_ERROR);
		return;
		}

	cprintf("%d list of forgotten rms\n",LISTING_FOLLOWS);
	
	for (a=0; a<MAXROOMS; ++a) {
		getroom(&qrbuf,a);
		if ( (is_zapped(&qrbuf,a,&CC->usersupp))
		   && ((qrbuf.QRfloor == target_floor)||(target_floor<0)) ) {
			if (!strcmp(qrbuf.QRname,"000")) cprintf(">");
			cprintf("%s|%u|%d\n",
				qrbuf.QRname,qrbuf.QRflags,qrbuf.QRfloor);
			}
		}
	cprintf("000\n");
	}



void usergoto(int where, int display_result)
{
	int a,b,c;
	int info = 0;
	int rmailflag;
	int raideflag;
	int newmailcount = 0;
	struct cdbdata *cdbmb;
	int num_mails;
	long *mailbox;

	CC->curr_rm=where;
	getroom(&CC->quickroom,CC->curr_rm);
	lgetuser(&CC->usersupp,CC->curr_user);
	CC->usersupp.forget[CC->curr_rm]=(-1);
	CC->usersupp.generation[CC->curr_rm]=CC->quickroom.QRgen;
	lputuser(&CC->usersupp,CC->curr_user);

	/* check for new mail */
	newmailcount = 0;

	cdbmb = cdb_fetch(CDB_MAILBOXES, &CC->usersupp.usernum, sizeof(long));
	if (cdbmb != NULL) {
		num_mails = cdbmb->len / sizeof(long);
		mailbox = (long *) cdbmb->ptr;
		if (num_mails > 0) for (a=0; a<num_mails; ++a) {
			if (mailbox[a] > (CC->usersupp.lastseen[1]))
				++newmailcount;
			}
		cdb_free(cdbmb);
		}

	/* set info to 1 if the user needs to read the room's info file */
	if (CC->quickroom.QRinfo > CC->usersupp.lastseen[CC->curr_rm]) info = 1;

	b=0; c=0;
	get_mm();
	get_msglist(CC->curr_rm);
	for (a=0; a<CC->num_msgs; ++a) {
		if (MessageFromList(a)>0L) {
			++b;
			if (MessageFromList(a)
			   > CC->usersupp.lastseen[CC->curr_rm]) ++c;
			}
		}


	if (CC->curr_rm == 1) rmailflag = 1;
	else rmailflag = 0;

	if ( (CC->quickroom.QRroomaide == CC->usersupp.usernum)
	   || (CC->usersupp.axlevel>=6) )  raideflag = 1;
	else raideflag = 0;

	if (display_result) cprintf("%d%c%s|%d|%d|%d|%d|%ld|%ld|%d|%d|%d|%d\n",
		OK,check_express(),
		CC->quickroom.QRname,c,b,info,CC->quickroom.QRflags,
		CC->quickroom.QRhighest,CC->usersupp.lastseen[CC->curr_rm],
		rmailflag,raideflag,newmailcount,CC->quickroom.QRfloor);
	if (CC->quickroom.QRflags & QR_PRIVATE) {
		set_wtmpsupp("<private room>");
		}
	else {
		set_wtmpsupp(CC->quickroom.QRname);
		}
	}


/* 
 * cmd_goto()  -  goto a new room
 */
void cmd_goto(char *gargs)
{
	struct quickroom QRscratch;
	int a,c;
	int ok;
	char bbb[20],towhere[32],password[20];

	if ((!(CC->logged_in)) && (!(CC->internal_pgm))) {
		cprintf("%d not logged in\n",ERROR+NOT_LOGGED_IN);
		return;
		}

	extract(towhere,gargs,0);
	extract(password,gargs,1);

	c=0;
	getuser(&CC->usersupp,CC->curr_user);
	for (a=0; a<MAXROOMS; ++a) {
		getroom(&QRscratch,a);
		if ((a==0)&&(!strucmp(towhere,"_BASEROOM_"))) {
			strncpy(towhere,QRscratch.QRname,31);
			}
		if ((a==1)&&(!strucmp(towhere,"_MAIL_"))) {
			strncpy(towhere,QRscratch.QRname,31);
			}
		if ((!strucmp(QRscratch.QRname,config.c_twitroom))
		   &&(!strucmp(towhere,"_BITBUCKET_"))) {
			strncpy(towhere,QRscratch.QRname,31);
			}
		strcpy(bbb,QRscratch.QRname);
		ok = 0;

		/* let internal programs go directly to any room */
		if (((CC->internal_pgm))&&(!strucmp(bbb,towhere))) {
			usergoto(a,1);
			return;
			}

		/* normal clients have to pass through security */
		if ( 
			(strucmp(bbb,towhere)==0)
			&&	((QRscratch.QRflags&QR_INUSE)!=0)

			&& (	((QRscratch.QRflags&QR_PREFONLY)==0)
			||	(CC->usersupp.axlevel>=5)
			)

			&& (	(a!=2) || (CC->usersupp.axlevel>=6) )

			&& (	((QRscratch.QRflags&QR_PRIVATE)==0)
   			|| (QRscratch.QRflags&QR_GUESSNAME)
			|| (CC->usersupp.axlevel>=6)
   			|| (QRscratch.QRflags&QR_PASSWORDED)
   			||	(QRscratch.QRgen==CC->usersupp.generation[a])
   			)
	
			) ok = 1;


		if (ok==1) {

			if (  (QRscratch.QRflags&QR_PASSWORDED) &&
				(CC->usersupp.axlevel<6) &&
				(QRscratch.QRgen!=CC->usersupp.generation[a]) &&
				(strucmp(QRscratch.QRpasswd,password))
				) {
					cprintf("%d wrong or missing passwd\n",
						ERROR+PASSWORD_REQUIRED);
					return;
					}

			usergoto(a,1);
			return;
			}

		}
	cprintf("%d room '%s' not found\n",ERROR+ROOM_NOT_FOUND,towhere);
	}


void cmd_whok(void) {
	struct usersupp temp;
	struct cdbdata *cdbus;

	if ((!(CC->logged_in))&&(!(CC->internal_pgm))) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
		}
	getuser(&CC->usersupp,CC->curr_user);

	if ((!is_room_aide()) && (!(CC->internal_pgm)) ) {
		cprintf("%d Higher access required.\n",
			ERROR+HIGHER_ACCESS_REQUIRED);
		return;
		}

	cprintf("%d Who knows room:\n",LISTING_FOLLOWS);
	cdb_rewind(CDB_USERSUPP);
	while(cdbus = cdb_next_item(CDB_USERSUPP), cdbus != NULL) {
		bzero(&temp, sizeof(struct usersupp));
		memcpy(&temp, cdbus->ptr, cdbus->len);
		cdb_free(cdbus);
		if ((CC->quickroom.QRflags & QR_INUSE)
			&& ( (CC->curr_rm!=2) || (temp.axlevel>=6) )
			&& (CC->quickroom.QRgen != (temp.forget[CC->curr_rm]) )

			&& (	((CC->quickroom.QRflags&QR_PREFONLY)==0)
			||	(temp.axlevel>=5)
			)

			&& (	((CC->quickroom.QRflags&QR_PRIVATE)==0)
   			||	(temp.axlevel>=6)
   			||	(CC->quickroom.QRgen==(temp.generation[CC->curr_rm]))
   			)

			&& (strncmp(temp.fullname,"000",3))

		) cprintf("%s\n",temp.fullname);
		}
	cprintf("000\n");
	}


/*
 * RDIR command for room directory
 */
void cmd_rdir(void) {
	char buf[256];
	char flnm[256];
	char comment[256];
	FILE *ls,*fd;
	struct stat statbuf;

	if (!(CC->logged_in)) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
		}

	getroom(&CC->quickroom,CC->curr_rm);
	getuser(&CC->usersupp,CC->curr_user);

	if ((CC->quickroom.QRflags & QR_DIRECTORY) == 0) {
		cprintf("%d not here.\n",ERROR+NOT_HERE);
		return;
		}

	if (((CC->quickroom.QRflags & QR_VISDIR) == 0)
	   && (CC->usersupp.axlevel<6)
	   && (CC->usersupp.usernum != CC->quickroom.QRroomaide)) {
		cprintf("%d not here.\n",ERROR+HIGHER_ACCESS_REQUIRED);
		return;
		}

	cprintf("%d %s|%s/files/%s\n",
		LISTING_FOLLOWS,config.c_fqdn,BBSDIR,CC->quickroom.QRdirname);

	sprintf(buf,"cd %s/files/%s; ls >%s 2>/dev/null",
		BBSDIR,CC->quickroom.QRdirname,CC->temp);
	system(buf);

	sprintf(buf,"%s/files/%s/filedir",BBSDIR,CC->quickroom.QRdirname);
	fd = fopen(buf,"r");
	if (fd==NULL) fd=fopen("/dev/null","r");

	ls = fopen(CC->temp,"r");
	while (fgets(flnm,256,ls)!=NULL) {
		flnm[strlen(flnm)-1]=0;
		if (strucmp(flnm,"filedir")) {
			sprintf(buf,"%s/files/%s/%s",
				BBSDIR,CC->quickroom.QRdirname,flnm);
			stat(buf,&statbuf);
			strcpy(comment,"");
			fseek(fd,0L,0);
			while ((fgets(buf,256,fd)!=NULL)
			    &&(strlen(comment)==0)) {
				buf[strlen(buf)-1] = 0;
				if ((!struncmp(buf,flnm,strlen(flnm)))
				   && (buf[strlen(flnm)]==' ')) 
					strncpy(comment,
						&buf[strlen(flnm)+1],255);
				}
			cprintf("%s|%ld|%s\n",flnm,statbuf.st_size,comment);
			}
		}
	fclose(ls);
	fclose(fd);
	unlink(CC->temp);

	cprintf("000\n");
	}

/*
 * get room parameters (aide or room aide command)
 */
void cmd_getr(void) {
	if ((!(CC->logged_in))&&(!(CC->internal_pgm))) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
		}

	if ( (!is_room_aide()) && (!(CC->internal_pgm)) ) {
		cprintf("%d Higher access required.\n",
			ERROR+HIGHER_ACCESS_REQUIRED);
		return;
		}

	if (CC->curr_rm < 3) {
		cprintf("%d Can't edit this room.\n",ERROR+NOT_HERE);
		return;
		}

	getroom(&CC->quickroom,CC->curr_rm);
	cprintf("%d%c%s|%s|%s|%d|%d\n",
		OK,check_express(),
		CC->quickroom.QRname,
		((CC->quickroom.QRflags & QR_PASSWORDED) ? CC->quickroom.QRpasswd : ""),
		((CC->quickroom.QRflags & QR_DIRECTORY) ? CC->quickroom.QRdirname : ""),
		CC->quickroom.QRflags,
		(int)CC->quickroom.QRfloor);
	}


/*
 * set room parameters (aide or room aide command)
 */
void cmd_setr(char *args) {
	char buf[256];
	struct floor flbuf;
	int old_floor;

	if (!(CC->logged_in)) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
		}

	if (!is_room_aide()) {
		cprintf("%d Higher access required.\n",
			ERROR+HIGHER_ACCESS_REQUIRED);
		return;
		}

	if (CC->curr_rm < 3) {
		cprintf("%d Can't edit this room.\n",ERROR+NOT_HERE);
		return;
		}

	if (num_parms(args)>=6) {
		getfloor(&flbuf,extract_int(args,5));
		if ((flbuf.f_flags & F_INUSE) == 0) {
			cprintf("%d Invalid floor number.\n",
				ERROR+INVALID_FLOOR_OPERATION);
			return;
			}
		}

	lgetroom(&CC->quickroom,CC->curr_rm);
	extract(buf,args,0); buf[20]=0;
	strncpy(CC->quickroom.QRname,buf,19);
	extract(buf,args,1); buf[10]=0;
	strncpy(CC->quickroom.QRpasswd,buf,9);
	extract(buf,args,2); buf[15]=0;
	strncpy(CC->quickroom.QRdirname,buf,19);
	CC->quickroom.QRflags = ( extract_int(args,3) | QR_INUSE);

	/* Clean up a client boo-boo: if the client set the room to
	 * guess-name or passworded, ensure that the private flag is
	 * also set.
	 */
	if ((CC->quickroom.QRflags & QR_GUESSNAME)
	   ||(CC->quickroom.QRflags & QR_PASSWORDED))
		CC->quickroom.QRflags |= QR_PRIVATE;

	/* Kick everyone out if the client requested it */
	if (extract_int(args,4)) {
		++CC->quickroom.QRgen;
		if (CC->quickroom.QRgen==100) CC->quickroom.QRgen=1;
		}

	old_floor = CC->quickroom.QRfloor;
	if (num_parms(args)>=6) {
		CC->quickroom.QRfloor = extract_int(args,5);
		}

	lputroom(&CC->quickroom,CC->curr_rm);

	/* adjust the floor reference counts */
	lgetfloor(&flbuf,old_floor);
	--flbuf.f_ref_count;
	lputfloor(&flbuf,old_floor);
	lgetfloor(&flbuf,CC->quickroom.QRfloor);
	++flbuf.f_ref_count;
	lputfloor(&flbuf,CC->quickroom.QRfloor);

	/* create a room directory if necessary */
	if (CC->quickroom.QRflags & QR_DIRECTORY) {
		sprintf(buf,
			"mkdir ./files/%s </dev/null >/dev/null 2>/dev/null",
		CC->quickroom.QRdirname);
		system(buf);
		}

	sprintf(buf,"%s> edited by %s",CC->quickroom.QRname,CC->curr_user);
	aide_message(buf);
	cprintf("%d Ok\n",OK);
	}



/* 
 * get the name of the room aide for this room
 */
void cmd_geta(void) {
	struct usersupp usbuf;

	if ((!(CC->logged_in))&&(!(CC->internal_pgm))) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
		}

	if (CC->curr_rm < 0) {
		cprintf("%d No current room.\n",ERROR);
		return;
		}

	if (getuserbynumber(&usbuf,CC->quickroom.QRroomaide)==0) {
		cprintf("%d %s\n",OK,usbuf.fullname);
		}
	else {
		cprintf("%d \n",OK);
		}
	}


/* 
 * set the room aide for this room
 */
void cmd_seta(char *new_ra)
{
	struct usersupp usbuf;
	long newu;
	char buf[256];
	int post_notice;
	
	if (!(CC->logged_in)) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
		}

	if (!is_room_aide()) {
		cprintf("%d Higher access required.\n",
			ERROR+HIGHER_ACCESS_REQUIRED);
		return;
		}

	if (CC->curr_rm < 3) {
		cprintf("%d Can't edit this room.\n",ERROR+NOT_HERE);
		return;
		}

	if (getuser(&usbuf,new_ra)!=0) {
		newu = (-1L);
		}
	else {
		newu = usbuf.usernum;
		}

	lgetroom(&CC->quickroom,CC->curr_rm);
	post_notice = 0;
	if (CC->quickroom.QRroomaide != newu) {
		post_notice = 1;
		}
	CC->quickroom.QRroomaide = newu;
	lputroom(&CC->quickroom,CC->curr_rm);

	/*
	 * We have to post the change notice _after_ writing changes to 
	 * the room table, otherwise it would deadlock!
	 */
	if (post_notice == 1) {
		sprintf(buf,"%s is now room aide for %s>",
			usbuf.fullname,CC->quickroom.QRname);
		aide_message(buf);
		}
	cprintf("%d Ok\n",OK);
	}


/* 
 * retrieve info file for this room
 */
void cmd_rinf(void) {
	char filename[64];
	char buf[256];
	FILE *info_fp;
	
	sprintf(filename,"./info/%d",CC->curr_rm);
	info_fp = fopen(filename,"r");

	if (info_fp==NULL) {
		cprintf("%d No info file.\n",ERROR);
		return;
		}

	cprintf("%d Info:\n",LISTING_FOLLOWS);	
	while (fgets(buf, 256, info_fp) != NULL) {
		if (strlen(buf) > 0) buf[strlen(buf)-1] = 0;
		cprintf("%s\n", buf);
		}
	cprintf("000\n");
	fclose(info_fp);
	}

/*
 * aide command: kill the current room
 */
void cmd_kill(char *argbuf)
{
	char aaa[100];
	int a;
	int kill_ok;
	struct floor flbuf;
	long MsgToDelete;
	
	kill_ok = extract_int(argbuf,0);

	if (!(CC->logged_in)) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
		}

	if (!is_room_aide()) {
		cprintf("%d Higher access required.\n",
			ERROR+HIGHER_ACCESS_REQUIRED);
		return;
		}

	if (CC->curr_rm < 3) {
		cprintf("%d Can't kill this room.\n",ERROR+NOT_HERE);
		return;
		}

	if (kill_ok) {

		/* first flag the room record as not in use */
		lgetroom(&CC->quickroom,CC->curr_rm);
		CC->quickroom.QRflags=0;

		/* then delete the messages in the room */
		get_msglist(CC->curr_rm);
		if (CC->num_msgs > 0) for (a=0; a < CC->num_msgs; ++a) {
			MsgToDelete = MessageFromList(a);
			cdb_delete(CDB_MSGMAIN, &MsgToDelete, sizeof(long));
			}
		put_msglist(CC->curr_rm);
		free(CC->msglist);
		CC->num_msgs = 0;
		cdb_delete(CDB_MSGLISTS, &CC->curr_rm, sizeof(int));

		lputroom(&CC->quickroom,CC->curr_rm);


		/* then decrement the reference count for the floor */
		lgetfloor(&flbuf,(int)CC->quickroom.QRfloor);
		flbuf.f_ref_count = flbuf.f_ref_count - 1;
		lputfloor(&flbuf,(int)CC->quickroom.QRfloor);

		/* tell the world what we did */
		sprintf(aaa,"%s> killed by %s",CC->quickroom.QRname,CC->curr_user);
		aide_message(aaa);
		CC->curr_rm=(-1);
		cprintf("%d '%s' deleted.\n",OK,CC->quickroom.QRname);
		}
	else {
		cprintf("%d ok to delete.\n",OK);
		}
	}


/*
 * Find a free slot to create a new room in, or return -1 for error.
 * search_dir is the direction to search in.  1 causes this function to
 * return the first available slot, -1 gets the last available slot.
 */
int get_free_room_slot(int search_dir)
{
	int a,st;
	struct quickroom qrbuf;

	st = ((search_dir>0) ? 3 : (MAXROOMS-1));

	for (a=st; ((a<MAXROOMS)&&(a>=3)); a=a+search_dir) {
		getroom(&qrbuf,a);
		if ((qrbuf.QRflags & QR_INUSE)==0) return(a);
		}
	return(-1);
	}


/*
 * internal code to create a new room (returns room flags)
 */
unsigned create_room(int free_slot, char *new_room_name, int new_room_type, char *new_room_pass, int new_room_floor)
{
	struct quickroom qrbuf;
	struct floor flbuf;

	lgetroom(&qrbuf,free_slot);
	strncpy(qrbuf.QRname,new_room_name,19);
	strncpy(qrbuf.QRpasswd,new_room_pass,9);
	qrbuf.QRflags = QR_INUSE;
	if (new_room_type > 0) qrbuf.QRflags=(qrbuf.QRflags|QR_PRIVATE);
	if (new_room_type == 1) qrbuf.QRflags=(qrbuf.QRflags|QR_GUESSNAME);
	if (new_room_type == 2) qrbuf.QRflags=(qrbuf.QRflags|QR_PASSWORDED);
	qrbuf.QRroomaide = (-1L);
	if ((new_room_type > 0)&&(CREATAIDE==1))
		qrbuf.QRroomaide=CC->usersupp.usernum;
	qrbuf.QRhighest = 0L;
	++qrbuf.QRgen; if (qrbuf.QRgen>=126) qrbuf.QRgen=10;
	qrbuf.QRfloor = new_room_floor;

	/* save what we just did... */
	lputroom(&qrbuf,free_slot);

	/* bump the reference count on whatever floor the room is on */
	lgetfloor(&flbuf,(int)qrbuf.QRfloor);
	flbuf.f_ref_count = flbuf.f_ref_count + 1;
	lputfloor(&flbuf,(int)qrbuf.QRfloor);

	/* be sure not to kick the creator out of the room! */
	lgetuser(&CC->usersupp,CC->curr_user);
	CC->usersupp.generation[free_slot] = qrbuf.QRgen;
	CC->usersupp.forget[free_slot] = (-1);
	lputuser(&CC->usersupp,CC->curr_user);

	/* resume our happy day */
	return(qrbuf.QRflags);
	}


/*
 * create a new room
 */
void cmd_cre8(char *args)
{
	int cre8_ok;
	int free_slot;
	int a;
	char new_room_name[256];
	int new_room_type;
	char new_room_pass[256];
	int new_room_floor;
	char aaa[256];
	unsigned newflags;
	struct quickroom qrbuf;
	struct floor flbuf;

	cre8_ok = extract_int(args,0);
	extract(new_room_name,args,1);
	new_room_name[19] = 0;
	new_room_type = extract_int(args,2);
	extract(new_room_pass,args,3);
	new_room_pass[9] = 0;
	new_room_floor = 0;

	if ((strlen(new_room_name)==0) && (cre8_ok==1)) {
		cprintf("%d Invalid room name.\n",ERROR);
		return;
		}

	if (num_parms(args)>=5) {
		getfloor(&flbuf,extract_int(args,4));
		if ((flbuf.f_flags & F_INUSE) == 0) {
			cprintf("%d Invalid floor number.\n",
				ERROR+INVALID_FLOOR_OPERATION);
			return;
			}
		else {
			new_room_floor = extract_int(args,4);
			}
		}

	if (!(CC->logged_in)) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
		}

	if (CC->usersupp.axlevel<3) {
		cprintf("%d You need higher access to create rooms.\n",
			ERROR+HIGHER_ACCESS_REQUIRED);
		return;
		}

	free_slot = get_free_room_slot(1);
	if (free_slot<0) {
		cprintf("%d There is no space available for a new room.\n",
			ERROR);
		return;
		}

	if (cre8_ok==0) {
		cprintf("%d ok to create...\n",OK);
		return;
		}

	for (a=0; a<MAXROOMS; ++a) {
		getroom(&qrbuf,a);
		if ( (!strucmp(qrbuf.QRname,new_room_name))
		   && (qrbuf.QRflags & QR_INUSE) ) {
			cprintf("%d '%s' already exists.\n",
				ERROR,qrbuf.QRname);
			return;
			}
		}

	if ((new_room_type < 0) || (new_room_type > 3)) {
		cprintf("%d Invalid room type.\n",ERROR);
		return;
		}

	newflags = create_room(free_slot,new_room_name,
			new_room_type,new_room_pass,new_room_floor);

	/* post a message in Aide> describing the new room */
	strncpy(aaa,new_room_name,255);
	strcat(aaa,"> created by ");
	strcat(aaa,CC->usersupp.fullname);
	if (newflags&QR_PRIVATE) strcat(aaa," [private]");
	if (newflags&QR_GUESSNAME) strcat(aaa,"[guessname] ");
	if (newflags&QR_PASSWORDED) {
		strcat(aaa,"\n Password: ");
		strcat(aaa,new_room_pass);
		}
	aide_message(aaa); 

	sprintf(aaa,"./info/%d",free_slot);	/* delete old info file */
	unlink(aaa);	
	sprintf(aaa,"./images/room.%d.gif",free_slot);	/* and picture */
	unlink(aaa);	

	cprintf("%d '%s' has been created.\n",OK,qrbuf.QRname);
	}



void cmd_einf(char *ok)
{	/* enter info file for current room */
	FILE *fp;
	char infofilename[32];
	char buf[256];

	if (!(CC->logged_in)) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
		}

	if (!is_room_aide()) {
		cprintf("%d Higher access required.\n",
			ERROR+HIGHER_ACCESS_REQUIRED);
		return;
		}

	if (atoi(ok)==0) {
		cprintf("%d Ok.\n",OK);
		return;
		}

	cprintf("%d Send info...\n",SEND_LISTING);

	sprintf(infofilename,"./info/%d",CC->curr_rm);

	fp=fopen(infofilename,"w");
	do {
		client_gets(buf);
		if (strcmp(buf,"000")) fprintf(fp,"%s\n",buf);
		} while(strcmp(buf,"000"));
	fclose(fp);

	/* now update the room index so people will see our new info */
	lgetroom(&CC->quickroom,CC->curr_rm);	/* lock so no one steps on us */
	CC->quickroom.QRinfo = CC->quickroom.QRhighest + 1L;
	lputroom(&CC->quickroom,CC->curr_rm);
	}


/* 
 * cmd_lflr()   -  List all known floors
 */
void cmd_lflr(void) {
	int a;
	struct floor flbuf;

	if (!(CC->logged_in)) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
		}

	/* if (getuser(&CC->usersupp,CC->curr_user)) {
		cprintf("%d Can't locate user!\n",ERROR+INTERNAL_ERROR);
		return;
		}
	*/

	cprintf("%d Known floors:\n",LISTING_FOLLOWS);
	
	for (a=0; a<MAXFLOORS; ++a) {
		getfloor(&flbuf,a);
		if (flbuf.f_flags & F_INUSE) {
			cprintf("%d|%s|%d\n",
				a,
				flbuf.f_name,
				flbuf.f_ref_count);
			}
		}
	cprintf("000\n");
	}



/*
 * create a new floor
 */
void cmd_cflr(char *argbuf)
{
	char new_floor_name[256];
	struct floor flbuf;
	int cflr_ok;
	int free_slot = (-1);
	int a;

	extract(new_floor_name,argbuf,0);
	cflr_ok = extract_int(argbuf,1);

	
	if (!(CC->logged_in)) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
		}

	if (CC->usersupp.axlevel<6) {
		cprintf("%d You need higher access to create rooms.\n",
			ERROR+HIGHER_ACCESS_REQUIRED);
		return;
		}

	for (a=0; a<MAXFLOORS; ++a) {
		getfloor(&flbuf,a);

		/* note any free slots while we're scanning... */
		if ( ((flbuf.f_flags & F_INUSE)==0) 
		     && (free_slot < 0) )  free_slot = a;

		/* check to see if it already exists */
		if ( (!strucmp(flbuf.f_name,new_floor_name))
		     && (flbuf.f_flags & F_INUSE) ) {
			cprintf("%d Floor '%s' already exists.\n",
				ERROR+ALREADY_EXISTS,
				flbuf.f_name);
			return;
			}

		}

	if (free_slot<0) {
		cprintf("%d There is no space available for a new floor.\n",
			ERROR+INVALID_FLOOR_OPERATION);
		return;
		}

	if (cflr_ok==0) {
		cprintf("%d ok to create...\n",OK);
		return;
		}

	lgetfloor(&flbuf,free_slot);
	flbuf.f_flags = F_INUSE;
	flbuf.f_ref_count = 0;
	strncpy(flbuf.f_name,new_floor_name,255);
	lputfloor(&flbuf,free_slot);
	cprintf("%d %d\n",OK,free_slot);
	}



/*
 * delete a floor
 */
void cmd_kflr(char *argbuf)
{
	struct floor flbuf;
	int floor_to_delete;
	int kflr_ok;
	int delete_ok;

	floor_to_delete = extract_int(argbuf,0);
	kflr_ok = extract_int(argbuf,1);

	
	if (!(CC->logged_in)) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
		}

	if (CC->usersupp.axlevel<6) {
		cprintf("%d You need higher access to delete floors.\n",
			ERROR+HIGHER_ACCESS_REQUIRED);
		return;
		}

	lgetfloor(&flbuf,floor_to_delete);

	delete_ok = 1;	
	if ((flbuf.f_flags & F_INUSE) == 0) {
		cprintf("%d Floor %d not in use.\n",
			ERROR+INVALID_FLOOR_OPERATION,floor_to_delete);
		delete_ok = 0;
		}

	else {
		if (flbuf.f_ref_count != 0) {
			cprintf("%d Cannot delete; floor contains %d rooms.\n",
				ERROR+INVALID_FLOOR_OPERATION,
				flbuf.f_ref_count);
			delete_ok = 0;
			}

		else {
			if (kflr_ok == 1) {
				cprintf("%d Ok\n",OK);
				}
			else {
				cprintf("%d Ok to delete...\n",OK);
				}

			}

		}

	if ( (delete_ok == 1) && (kflr_ok == 1) ) flbuf.f_flags = 0;
	lputfloor(&flbuf,floor_to_delete);
	}

/*
 * edit a floor
 */
void cmd_eflr(char *argbuf)
{
	struct floor flbuf;
	int floor_num;
	int np;

	np = num_parms(argbuf);
	if (np < 1) {
		cprintf("%d Usage error.\n",ERROR);
		return;
		}
	
	if (!(CC->logged_in)) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
		}

	if (CC->usersupp.axlevel<6) {
		cprintf("%d You need higher access to edit floors.\n",
			ERROR+HIGHER_ACCESS_REQUIRED);
		return;
		}

	floor_num = extract_int(argbuf,0);
	lgetfloor(&flbuf,floor_num);
	if ( (flbuf.f_flags & F_INUSE) == 0) {
		lputfloor(&flbuf,floor_num);
		cprintf("%d Floor %d is not in use.\n",
			ERROR+INVALID_FLOOR_OPERATION,floor_num);
		return;
		}
	if (np >= 2) extract(flbuf.f_name,argbuf,1);
	lputfloor(&flbuf,floor_num);
	
	cprintf("%d Ok\n",OK);
	}
