#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <limits.h>
#include "citadel.h"
#include "server.h"
#include "database.h"
#include "config.h"
#include "room_ops.h"
#include "sysdep_decls.h"
#include "support.h"
#include "user_ops.h"
#include "msgbase.h"
#include "serv_chat.h"
#include "citserver.h"

/*
 * Generic routine for determining user access to rooms
 */
int CtdlRoomAccess(struct quickroom *roombuf, struct usersupp *userbuf) {
	int retval = 0;
	struct visit vbuf;

	/* Make sure we're dealing with a real, existing room */
	if (roombuf->QRflags & QR_INUSE) {
		retval = retval | UA_INUSE;
		}
	else {
		return(0);
		}

	/* for internal programs, always do everything */
	if (((CC->internal_pgm))&&(roombuf->QRflags & QR_INUSE)) {
		return(UA_INUSE | UA_KNOWN | UA_GOTOALLOWED);
		}

	/* Locate any applicable user/room relationships */
	CtdlGetRelationship(&vbuf, userbuf, roombuf);

	/* If this is a public room, it's accessible... */
	if ((roombuf->QRflags & QR_PRIVATE) == 0) {
		retval = retval | UA_KNOWN | UA_GOTOALLOWED;
		}

	/* If this is a preferred users only room, check access level */
	if (roombuf->QRflags & QR_PREFONLY) {
		if (userbuf->axlevel < 5) {
			retval = retval & ~UA_KNOWN & ~UA_GOTOALLOWED;
			}
		}

	/* For private rooms, check the generation number matchups */
	if (roombuf->QRflags & QR_PRIVATE) {

		/* An explicit match means the user belongs in this room */
		if (vbuf.v_flags & V_ACCESS) {
			retval = retval | UA_KNOWN | UA_GOTOALLOWED;
			}
		/* Otherwise, check if this is a guess-name or passworded
		 * room.  If it is, a goto may at least be attempted
		 */
		else if ((roombuf->QRflags & QR_PRIVATE)
		     ||(roombuf->QRflags & QR_PASSWORDED)) {
			retval = retval & ~UA_KNOWN;
			retval = retval | UA_GOTOALLOWED;
			}
		}

	/* Check to see if the user has forgotten this room */
	if (vbuf.v_flags & V_FORGET) {
		retval = retval & ~UA_KNOWN;
		retval = retval | UA_ZAPPED;
		}

	/* If user is explicitly locked out of this room, deny everything */
	if (vbuf.v_flags & V_LOCKOUT) {
		retval = retval & ~UA_KNOWN & ~UA_GOTOALLOWED;
		}

	/* Aides get access to everything */
	if (userbuf->axlevel >= 6) {
		retval = retval | UA_INUSE | UA_KNOWN | UA_GOTOALLOWED;
		retval = retval & ~UA_ZAPPED;
		}

	/* By the way, we also check for the presence of new messages */
	if ( (roombuf->QRhighest) > (vbuf.v_lastseen) ) {
		retval = retval | UA_HASNEWMSGS;
		}

	return(retval);
	}

/*
 * getroom()  -  retrieve room data from disk
 */
int getroom(struct quickroom *qrbuf, char *room_name)
{
	struct cdbdata *cdbqr;
	char lowercase_name[ROOMNAMELEN];
	int a;

	for (a=0; a<=strlen(room_name); ++a) {
		lowercase_name[a] = tolower(room_name[a]);
		}

	bzero(qrbuf, sizeof(struct quickroom));
	cdbqr = cdb_fetch(CDB_QUICKROOM,
			lowercase_name, strlen(lowercase_name));
	if (cdbqr != NULL) {
		memcpy(qrbuf, cdbqr->ptr,
	                ( (cdbqr->len > sizeof(struct quickroom)) ?
                	sizeof(struct quickroom) : cdbqr->len) );
		cdb_free(cdbqr);
		return(0);
		}
	else {
		return(1);
		}
	}

/*
 * lgetroom()  -  same as getroom() but locks the record (if supported)
 */
int lgetroom(struct quickroom *qrbuf, char *room_name)
{
	begin_critical_section(S_QUICKROOM);
	return(getroom(qrbuf, room_name));
	}


/*
 * putroom()  -  store room data on disk
 */
void putroom(struct quickroom *qrbuf, char *room_name)
{
	char lowercase_name[ROOMNAMELEN];
	int a;

	for (a=0; a<=strlen(room_name); ++a) {
		lowercase_name[a] = tolower(room_name[a]);
		}

	time(&qrbuf->QRmtime);
	cdb_store(CDB_QUICKROOM, lowercase_name, strlen(lowercase_name),
		qrbuf, sizeof(struct quickroom));
	}


/*
 * lputroom()  -  same as putroom() but unlocks the record (if supported)
 */
void lputroom(struct quickroom *qrbuf, char *room_name)
{

	putroom(qrbuf, room_name);
	end_critical_section(S_QUICKROOM);

	}

/****************************************************************************/

/*
 * getfloor()  -  retrieve floor data from disk
 */
void getfloor(struct floor *flbuf, int floor_num)
{
	struct cdbdata *cdbfl;

	bzero(flbuf, sizeof(struct floor));
	cdbfl = cdb_fetch(CDB_FLOORTAB, &floor_num, sizeof(int));
	if (cdbfl != NULL) {
		memcpy(flbuf, cdbfl->ptr,
	                ( (cdbfl->len > sizeof(struct floor)) ?
                	sizeof(struct floor) : cdbfl->len) );
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
 *  Traverse the room file...
 */
void ForEachRoom(void (*CallBack)(struct quickroom *EachRoom)) {
	struct quickroom qrbuf;
	struct cdbdata *cdbqr;

	cdb_rewind(CDB_QUICKROOM);

	while(cdbqr = cdb_next_item(CDB_QUICKROOM), cdbqr != NULL) {
		bzero(&qrbuf, sizeof(struct quickroom));
		memcpy(&qrbuf, cdbqr->ptr,
			( (cdbqr->len > sizeof(struct quickroom)) ?
			sizeof(struct quickroom) : cdbqr->len) );
		cdb_free(cdbqr);
		if (qrbuf.QRflags & QR_INUSE) (*CallBack)(&qrbuf);
		}
	}



/*
 * get_msglist()  -  retrieve room message pointers
 */
void get_msglist(struct quickroom *whichroom) {
	struct cdbdata *cdbfr;
	char dbkey[256];
	int a;

	sprintf(dbkey, "%s%ld", whichroom->QRname, whichroom->QRgen);
	for (a=0; a<strlen(dbkey); ++a) dbkey[a]=tolower(dbkey[a]);

	if (CC->msglist != NULL) {
		free(CC->msglist);
		}
	CC->msglist = NULL;
	CC->num_msgs = 0;

	cdbfr = cdb_fetch(CDB_MSGLISTS, dbkey, strlen(dbkey));

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
void put_msglist(struct quickroom *whichroom) {
	char dbkey[256];
	int a;

	sprintf(dbkey, "%s%ld", whichroom->QRname, whichroom->QRgen);
	for (a=0; a<strlen(dbkey); ++a) dbkey[a]=tolower(dbkey[a]);

	cdb_store(CDB_MSGLISTS, dbkey, strlen(dbkey),
		CC->msglist, CC->num_msgs * sizeof(long));
	}


/*
 * delete_msglist()  -  delete room message pointers
 */
void delete_msglist(struct quickroom *whichroom) {
	char dbkey[256];
	int a;

	sprintf(dbkey, "%s%ld", whichroom->QRname, whichroom->QRgen);
	for (a=0; a<strlen(dbkey); ++a) dbkey[a]=tolower(dbkey[a]);

	cdb_delete(CDB_MSGLISTS, dbkey, strlen(dbkey));
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
void cmd_lrms_backend(struct quickroom *qrbuf) {
	if ( ((CtdlRoomAccess(qrbuf, &CC->usersupp)
	     & (UA_KNOWN | UA_ZAPPED)))
	&& ((qrbuf->QRfloor == (CC->FloorBeingSearched))
	   ||((CC->FloorBeingSearched)<0)) ) 
		cprintf("%s|%u|%d\n",
			qrbuf->QRname,qrbuf->QRflags,qrbuf->QRfloor);
	}

void cmd_lrms(char *argbuf)
{
	CC->FloorBeingSearched = (-1);
	if (strlen(argbuf)>0) CC->FloorBeingSearched = extract_int(argbuf,0);

	if (!(CC->logged_in)) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
		}

	if (getuser(&CC->usersupp,CC->curr_user)) {
		cprintf("%d Can't locate user!\n",ERROR+INTERNAL_ERROR);
		return;
		}

	cprintf("%d Accessible rooms:\n",LISTING_FOLLOWS);

	ForEachRoom(cmd_lrms_backend);	
	cprintf("000\n");
	}



/* 
 * cmd_lkra()   -  List all known rooms
 */
void cmd_lkra_backend(struct quickroom *qrbuf) {
	if ( ((CtdlRoomAccess(qrbuf, &CC->usersupp)
	     & (UA_KNOWN)))
	&& ((qrbuf->QRfloor == (CC->FloorBeingSearched))
	   ||((CC->FloorBeingSearched)<0)) ) 
		cprintf("%s|%u|%d\n",
			qrbuf->QRname,qrbuf->QRflags,qrbuf->QRfloor);
	}

void cmd_lkra(char *argbuf)
{
	CC->FloorBeingSearched = (-1);
	if (strlen(argbuf)>0) CC->FloorBeingSearched = extract_int(argbuf,0);

	if (!(CC->logged_in)) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
		}

	if (getuser(&CC->usersupp,CC->curr_user)) {
		cprintf("%d Can't locate user!\n",ERROR+INTERNAL_ERROR);
		return;
		}

	cprintf("%d Known rooms:\n",LISTING_FOLLOWS);

	ForEachRoom(cmd_lkra_backend);	
	cprintf("000\n");
	}



/* 
 * cmd_lkrn()   -  List all known rooms with new messages
 */
void cmd_lkrn_backend(struct quickroom *qrbuf) {
	int ra;

	ra = CtdlRoomAccess(qrbuf, &CC->usersupp);
	if ( (ra & UA_KNOWN)
	   && (ra & UA_HASNEWMSGS)
	   && ((qrbuf->QRfloor == (CC->FloorBeingSearched))
	      ||((CC->FloorBeingSearched)<0)) )
		cprintf("%s|%u|%d\n",
			qrbuf->QRname,qrbuf->QRflags,qrbuf->QRfloor);
	}

void cmd_lkrn(char *argbuf)
{
	CC->FloorBeingSearched = (-1);
	if (strlen(argbuf)>0) CC->FloorBeingSearched = extract_int(argbuf,0);

	if (!(CC->logged_in)) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
		}

	if (getuser(&CC->usersupp,CC->curr_user)) {
		cprintf("%d Can't locate user!\n",ERROR+INTERNAL_ERROR);
		return;
		}

	cprintf("%d Rooms w/ new msgs:\n",LISTING_FOLLOWS);

	ForEachRoom(cmd_lkrn_backend);	
	cprintf("000\n");
	}



/* 
 * cmd_lkro()   -  List all known rooms
 */
void cmd_lkro_backend(struct quickroom *qrbuf) {
	int ra;

	ra = CtdlRoomAccess(qrbuf, &CC->usersupp);
	if ( (ra & UA_KNOWN)
	   && ((ra & UA_HASNEWMSGS)==0)
	   && ((qrbuf->QRfloor == (CC->FloorBeingSearched))
	      ||((CC->FloorBeingSearched)<0)) )
		cprintf("%s|%u|%d\n",
			qrbuf->QRname,qrbuf->QRflags,qrbuf->QRfloor);
	}

void cmd_lkro(char *argbuf)
{
	CC->FloorBeingSearched = (-1);
	if (strlen(argbuf)>0) CC->FloorBeingSearched = extract_int(argbuf,0);

	if (!(CC->logged_in)) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
		}

	if (getuser(&CC->usersupp,CC->curr_user)) {
		cprintf("%d Can't locate user!\n",ERROR+INTERNAL_ERROR);
		return;
		}

	cprintf("%d Rooms w/o new msgs:\n",LISTING_FOLLOWS);

	ForEachRoom(cmd_lkro_backend);	
	cprintf("000\n");
	}



/* 
 * cmd_lzrm()   -  List all forgotten rooms
 */
void cmd_lzrm_backend(struct quickroom *qrbuf) {
	int ra;

	ra = CtdlRoomAccess(qrbuf, &CC->usersupp);
	if ( (ra & UA_GOTOALLOWED)
	   && (ra & UA_ZAPPED)
	   && ((qrbuf->QRfloor == (CC->FloorBeingSearched))
	      ||((CC->FloorBeingSearched)<0)) )
		cprintf("%s|%u|%d\n",
			qrbuf->QRname,qrbuf->QRflags,qrbuf->QRfloor);
	}

void cmd_lzrm(char *argbuf)
{
	CC->FloorBeingSearched = (-1);
	if (strlen(argbuf)>0) CC->FloorBeingSearched = extract_int(argbuf,0);

	if (!(CC->logged_in)) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
		}

	if (getuser(&CC->usersupp,CC->curr_user)) {
		cprintf("%d Can't locate user!\n",ERROR+INTERNAL_ERROR);
		return;
		}

	cprintf("%d Zapped rooms:\n",LISTING_FOLLOWS);

	ForEachRoom(cmd_lzrm_backend);	
	cprintf("000\n");
	}



void usergoto(char *where, int display_result)
{
	int a;
	int new_messages = 0;
	int total_messages = 0;
	int info = 0;
	int rmailflag;
	int raideflag;
	int newmailcount = 0;
	struct visit vbuf;

	strcpy(CC->cs_room, where);
	getroom(&CC->quickroom, CC->cs_room);
	lgetuser(&CC->usersupp,CC->curr_user);
	CtdlGetRelationship(&vbuf, &CC->usersupp, &CC->quickroom);

	vbuf.v_flags = vbuf.v_flags & ~V_FORGET & ~V_LOCKOUT;
	vbuf.v_flags = vbuf.v_flags | V_ACCESS;

	CtdlSetRelationship(&vbuf, &CC->usersupp, &CC->quickroom);
	lputuser(&CC->usersupp,CC->curr_user);

	/* check for new mail */
	newmailcount = NewMailCount();

	/* set info to 1 if the user needs to read the room's info file */
	if (CC->quickroom.QRinfo > vbuf.v_lastseen) info = 1;

	get_mm();
	get_msglist(&CC->quickroom);
	for (a=0; a<CC->num_msgs; ++a) {
		if (MessageFromList(a)>0L) {
			++total_messages;
			if (MessageFromList(a) > vbuf.v_lastseen) {
				++new_messages;
				}
			}
		}


	if (0) rmailflag = 1; /* FIX to handle mail rooms!!! */
	else rmailflag = 0;

	if ( (CC->quickroom.QRroomaide == CC->usersupp.usernum)
	   || (CC->usersupp.axlevel>=6) )  raideflag = 1;
	else raideflag = 0;

	if (display_result) cprintf("%d%c%s|%d|%d|%d|%d|%ld|%ld|%d|%d|%d|%d\n",
		OK,check_express(),
		CC->quickroom.QRname,
		new_messages, total_messages,
		info,CC->quickroom.QRflags,
		CC->quickroom.QRhighest,
		vbuf.v_lastseen,
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
	int c;
	int ok = 0;
	int ra;
	char bbb[ROOMNAMELEN],towhere[32],password[20];

	if ((!(CC->logged_in)) && (!(CC->internal_pgm))) {
		cprintf("%d not logged in\n",ERROR+NOT_LOGGED_IN);
		return;
		}

	extract(towhere,gargs,0);
	extract(password,gargs,1);

	c=0;
	getuser(&CC->usersupp,CC->curr_user);

	if (!strcasecmp(towhere, "_BASEROOM_"))
		strcpy(towhere, BASEROOM);

	if (!strcasecmp(towhere, "_MAIL_"))
		strcpy(towhere, MAILROOM);

	if (!strcasecmp(towhere, "_BITBUCKET_"))
		strcpy(towhere, config.c_twitroom);


	/* let internal programs go directly to any room */
	if (((CC->internal_pgm))&&(!strcasecmp(bbb,towhere))) {
		usergoto(towhere, 1);
		return;
		}

	if (getroom(&QRscratch, towhere) == 0) {

		/* See if there is an existing user/room relationship */
		ra = CtdlRoomAccess(&QRscratch, &CC->usersupp);

		/* normal clients have to pass through security */
		if (ra & UA_GOTOALLOWED) ok = 1;

		if (ok==1) {
			if (  (QRscratch.QRflags&QR_PASSWORDED) &&
				((ra & UA_KNOWN) == 0) &&
				(strcasecmp(QRscratch.QRpasswd,password))
				) {
					cprintf("%d wrong or missing passwd\n",
						ERROR+PASSWORD_REQUIRED);
					return;
					}
			else {
				usergoto(towhere, 1);
				return;
				}
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

		if ( (CC->quickroom.QRflags & QR_INUSE)
			&& (CtdlRoomAccess(&CC->quickroom, &temp) & UA_KNOWN)
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

	getroom(&CC->quickroom, CC->cs_room);
	getuser(&CC->usersupp, CC->curr_user);

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
		if (strcasecmp(flnm,"filedir")) {
			sprintf(buf,"%s/files/%s/%s",
				BBSDIR,CC->quickroom.QRdirname,flnm);
			stat(buf,&statbuf);
			strcpy(comment,"");
			fseek(fd,0L,0);
			while ((fgets(buf,256,fd)!=NULL)
			    &&(strlen(comment)==0)) {
				buf[strlen(buf)-1] = 0;
				if ((!strncasecmp(buf,flnm,strlen(flnm)))
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

	if ( (!strcasecmp(CC->cs_room, BASEROOM))
	     || (!strcasecmp(CC->cs_room, AIDEROOM)) ) {
		cprintf("%d Can't edit this room.\n",ERROR+NOT_HERE);
		return;
		}

	getroom(&CC->quickroom, CC->cs_room);
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

	if ( (!strcasecmp(CC->cs_room, BASEROOM))
	     || (!strcasecmp(CC->cs_room, AIDEROOM)) ) {
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

	lgetroom(&CC->quickroom, CC->cs_room);
	extract(buf,args,0); buf[ROOMNAMELEN]=0;
	strncpy(CC->quickroom.QRname,buf,ROOMNAMELEN-1);
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

	/* Kick everyone out if the client requested it (by changing the
	 * room's generation number)
	 */
	if (extract_int(args,4)) {
		time(&CC->quickroom.QRgen);
		}

	old_floor = CC->quickroom.QRfloor;
	if (num_parms(args)>=6) {
		CC->quickroom.QRfloor = extract_int(args,5);
		}

	lputroom(&CC->quickroom, CC->cs_room);

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

	if ( (!strcasecmp(CC->cs_room, BASEROOM))
	     || (!strcasecmp(CC->cs_room, AIDEROOM)) ) {
		cprintf("%d Can't edit this room.\n",ERROR+NOT_HERE);
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

	if (getuser(&usbuf,new_ra)!=0) {
		newu = (-1L);
		}
	else {
		newu = usbuf.usernum;
		}

	lgetroom(&CC->quickroom, CC->cs_room);
	post_notice = 0;
	if (CC->quickroom.QRroomaide != newu) {
		post_notice = 1;
		}
	CC->quickroom.QRroomaide = newu;
	lputroom(&CC->quickroom, CC->cs_room);

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
 * Generate an associated file name for a room
 */
void assoc_file_name(char *buf, struct quickroom *qrbuf, char *prefix) {
	int a;

	sprintf(buf, "./prefix/%s.%ld", qrbuf->QRname, qrbuf->QRgen);
	for (a=0; a<strlen(buf); ++a) {
		if (buf[a]==32) buf[a]='.';
		}
	}

/* 
 * retrieve info file for this room
 */
void cmd_rinf(void) {
	char filename[128];
	char buf[256];
	FILE *info_fp;
	
	assoc_file_name(filename, &CC->quickroom, "info");
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

	if ( (!strcasecmp(CC->cs_room, BASEROOM))
	     || (!strcasecmp(CC->cs_room, AIDEROOM)) ) {
		cprintf("%d Can't edit this room.\n",ERROR+NOT_HERE);
		return;
		}

	if (kill_ok) {

		/* Delete the info file */
		assoc_file_name(aaa, &CC->quickroom, "info");
		unlink(aaa);

		/* first flag the room record as not in use */
		lgetroom(&CC->quickroom, CC->cs_room);
		CC->quickroom.QRflags=0;

		/* then delete the messages in the room */
		get_msglist(&CC->quickroom);
		if (CC->num_msgs > 0) for (a=0; a < CC->num_msgs; ++a) {
			MsgToDelete = MessageFromList(a);
			cdb_delete(CDB_MSGMAIN, &MsgToDelete, sizeof(long));
			}
		put_msglist(&CC->quickroom);
		free(CC->msglist);
		CC->num_msgs = 0;
		delete_msglist(&CC->quickroom);
		lputroom(&CC->quickroom, CC->cs_room);

		/*    FIX FIX FIX
		 * To do at this location in the code:
		 * 1. Delete the associated files (info, image)
		 * 2. Delete the room record from the database
		 */ 

		/* then decrement the reference count for the floor */
		lgetfloor(&flbuf,(int)CC->quickroom.QRfloor);
		flbuf.f_ref_count = flbuf.f_ref_count - 1;
		lputfloor(&flbuf,(int)CC->quickroom.QRfloor);

		/* tell the world what we did */
		sprintf(aaa,"%s> killed by %s",CC->quickroom.QRname,CC->curr_user);
		aide_message(aaa);
		usergoto(BASEROOM, 0);
		cprintf("%d '%s' deleted.\n",OK,CC->quickroom.QRname);
		}
	else {
		cprintf("%d ok to delete.\n",OK);
		}
	}


/*
 * Internal code to create a new room (returns room flags)
 *
 * Room types:	0=public, 1=guessname, 2=passworded, 3=inv-only, 4=mailbox
 */
unsigned create_room(char *new_room_name,
			int new_room_type,
			char *new_room_pass,
			int new_room_floor) {

	struct quickroom qrbuf;
	struct floor flbuf;
	struct visit vbuf;

	if (getroom(&qrbuf, new_room_name)==0) return(0); /* already exists */

	bzero(&qrbuf, sizeof(struct quickroom));
	strncpy(qrbuf.QRname,new_room_name,ROOMNAMELEN);
	strncpy(qrbuf.QRpasswd,new_room_pass,9);
	qrbuf.QRflags = QR_INUSE;
	if (new_room_type > 0) qrbuf.QRflags=(qrbuf.QRflags|QR_PRIVATE);
	if (new_room_type == 1) qrbuf.QRflags=(qrbuf.QRflags|QR_GUESSNAME);
	if (new_room_type == 2) qrbuf.QRflags=(qrbuf.QRflags|QR_PASSWORDED);
	if (new_room_type == 4) qrbuf.QRflags=(qrbuf.QRflags|QR_MAILBOX);
	qrbuf.QRroomaide = (-1L);
	if ((new_room_type > 0)&&(CREATAIDE==1))
		qrbuf.QRroomaide=CC->usersupp.usernum;
	qrbuf.QRhighest = 0L;
	time(&qrbuf.QRgen);
	qrbuf.QRfloor = new_room_floor;

	/* save what we just did... */
	putroom(&qrbuf, qrbuf.QRname);

	/* bump the reference count on whatever floor the room is on */
	lgetfloor(&flbuf,(int)qrbuf.QRfloor);
	flbuf.f_ref_count = flbuf.f_ref_count + 1;
	lputfloor(&flbuf,(int)qrbuf.QRfloor);

	/* be sure not to kick the creator out of the room! */
	lgetuser(&CC->usersupp,CC->curr_user);
	CtdlGetRelationship(&vbuf, &CC->usersupp, &qrbuf);
	vbuf.v_flags = vbuf.v_flags & ~V_FORGET & ~V_LOCKOUT;
	vbuf.v_flags = vbuf.v_flags | V_ACCESS;
	CtdlSetRelationship(&vbuf, &CC->usersupp, &qrbuf);
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
	new_room_name[ROOMNAMELEN-1] = 0;
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

	if ((strlen(new_room_name)==0) && (cre8_ok==0)) {
		cprintf("%d Ok to create rooms.\n", OK);
		return;
		}

	/* Check to make sure the requested room name doesn't already exist */
	if (getroom(&qrbuf, new_room_name)==0) {
		cprintf("%d '%s' already exists.\n",
			ERROR,qrbuf.QRname);
		return;
		}

	if ((new_room_type < 0) || (new_room_type > 3)) {
		cprintf("%d Invalid room type.\n",ERROR);
		return;
		}

	if (cre8_ok == 0) {
		cprintf("%d OK to create '%s'\n", OK, new_room_name);
		return;
		}

	newflags = create_room(new_room_name,
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

	cprintf("%d '%s' has been created.\n",OK,qrbuf.QRname);
	}



void cmd_einf(char *ok)
{	/* enter info file for current room */
	FILE *fp;
	char infofilename[64];
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

	assoc_file_name(infofilename, &CC->quickroom, "info");

	fp=fopen(infofilename,"w");
	do {
		client_gets(buf);
		if (strcmp(buf,"000")) fprintf(fp,"%s\n",buf);
		} while(strcmp(buf,"000"));
	fclose(fp);

	/* now update the room index so people will see our new info */
	lgetroom(&CC->quickroom,CC->cs_room); /* lock so no one steps on us */
	CC->quickroom.QRinfo = CC->quickroom.QRhighest + 1L;
	lputroom(&CC->quickroom,CC->cs_room);
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
		if ( (!strcasecmp(flbuf.f_name,new_floor_name))
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
