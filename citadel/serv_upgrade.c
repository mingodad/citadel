/*
 * This module imports an "unpacked" system.  The unpacked data may come from
 * an older version of Citadel, or a different hardware architecture, or
 * whatever.  You should only run an import when your installed system is
 * brand new and _empty_ !!
 *
 * $Id$
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <pwd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <string.h>
#include <limits.h>
#include <pthread.h>
#include "citadel.h"
#include "server.h"
#include <syslog.h>
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "dynloader.h"
#include "room_ops.h"
#include "user_ops.h"
#include "database.h"
#include "control.h"

extern struct CitContext *ContextList;
FILE *imfp, *exfp;

#define MODULE_NAME 	"Import an unpacked system"
#define MODULE_AUTHOR	"Art Cancro"
#define MODULE_EMAIL	"ajc@uncnsrd.mt-kisco.ny.us"
#define MAJOR_VERSION	0
#define MINOR_VERSION	3

static struct DLModule_Info info = {
	MODULE_NAME,
	MODULE_AUTHOR,
	MODULE_EMAIL,
	MAJOR_VERSION,
	MINOR_VERSION
	};



void fpgetfield(FILE *fp, char *string)
{
	int a,b;
	strcpy(string,"");
	a=0;
	do {
		b=getc(fp);
		if (b<1) {
			string[a]=0;
			return;
			}
		string[a]=b;
		++a;
		} while (b!=0);
	}


void import_message(long msgnum, long msglen) {
	char *msgtext;

	msgtext = malloc(msglen);
	if (msgtext == NULL) {
		lprintf(3, "ERROR: cannot allocate memory\n");
		lprintf(3, "Your data files are now corrupt.\n");
		fclose(imfp);
		exit(1);
		}

	fread(msgtext, msglen, 1, imfp);
	cdb_store(CDB_MSGMAIN, &msgnum, sizeof(long), msgtext, msglen);
	free(msgtext);
	}

void imp_floors(void) {
	char key[256], tag[256], tval[256];
	struct floor fl;
	int floornum = 0;


	while(fpgetfield(imfp, key), strcasecmp(key, "endsection")) {

		if (!strcasecmp(key, "floor")) {
			bzero(&fl, sizeof(struct floor));

			while(fpgetfield(imfp, tag),
			     strcasecmp(tag, "endfloor")) {
				fpgetfield(imfp, tval);

				if (!strcasecmp(tag, "f_flags")) 
					fl.f_flags = atoi(tval);
				if (!strcasecmp(tag, "f_name")) {
					lprintf(9, "Floor <%s>\n", tval);
					strcpy(fl.f_name, tval);	
					}
				if (!strcasecmp(tag, "f_ref_count")) 
					fl.f_ref_count = atoi(tval);
				if (!strcasecmp(tag, "f_expire_mode")) 
					fl.f_ep.expire_mode = atoi(tval);
				if (!strcasecmp(tag, "f_expire_value")) 
					fl.f_ep.expire_value = atoi(tval);
				}

			putfloor(&fl, floornum);
			++floornum;
			}
		else {
			lprintf(3, "ERROR: invalid floor section.\n");
			lprintf(3, "Your data files are now corrupt.\n");
			fclose(imfp);
			return;
			}
		}
	}



void imp_rooms(void) {
	char key[256];
	char tag[256], tval[256];
	struct quickroom qr;
	long *msglist;
	int num_msgs = 0;
	long msgnum, msglen;
	
	while(fpgetfield(imfp, key), strcasecmp(key, "endsection")) {
		if (!strcasecmp(key, "room")) {
			bzero(&qr, sizeof(struct quickroom));
			msglist = NULL;
			num_msgs = 0;
			lprintf(9, "Room ");

			while(fpgetfield(imfp, tag),
			     strcasecmp(tag, "endroom")) {
				if (strcasecmp(tag, "message")) {
					fpgetfield(imfp, tval);
					}
				else {
					strcpy(tval, "");
					}

				if (!strcasecmp(tag, "qrname")) {
					strcpy(qr.QRname, tval);
					lprintf(9, "<%s> ", qr.QRname);
					}
				if (!strcasecmp(tag, "qrnumber"))
					qr.QRnumber = atol(tval);
				if (!strcasecmp(tag, "qrpasswd"))
					strcpy(qr.QRpasswd, tval);
				if (!strcasecmp(tag, "qrroomaide"))
					qr.QRroomaide = atol(tval);
				if (!strcasecmp(tag, "qrhighest"))
					qr.QRhighest = atol(tval);
				if (!strcasecmp(tag, "qrgen"))
					qr.QRgen = atol(tval);
				if (!strcasecmp(tag, "qrflags"))
					qr.QRflags = atoi(tval);
				if (!strcasecmp(tag, "qrdirname"))
					strcpy(qr.QRdirname, tval);
				if (!strcasecmp(tag, "qrinfo"))
					qr.QRinfo = atol(tval);
				if (!strcasecmp(tag, "qrfloor"))
					qr.QRfloor = atoi(tval);
				if (!strcasecmp(tag, "qrmtime"))
					qr.QRmtime = atol(tval);
				if (!strcasecmp(tag, "qrepmode"))
					qr.QRep.expire_mode = atoi(tval);
				if (!strcasecmp(tag, "qrepvalue"))
					qr.QRep.expire_value = atoi(tval);
				if (!strcasecmp(tag, "message")) {
					fpgetfield(imfp, tval);
					msgnum = atol(tval);
					fpgetfield(imfp, tval);
					msglen = atol(tval);
					import_message(msgnum, msglen);
					++num_msgs;
					msglist = realloc(msglist,
						(sizeof(long)*num_msgs) );
					msglist[num_msgs - 1] = msgnum;
					}

				}

			lprintf(9, "(%d messages)\n", num_msgs);
			if (qr.QRflags&QR_INUSE) {
				putroom(&qr, qr.QRname);
				}

			if (num_msgs > 0) {
				if (qr.QRflags&QR_INUSE) {
					CC->msglist = msglist;
					CC->num_msgs = num_msgs;
					put_msglist(&qr);
					}
				free(msglist);
				}

			}
		else {
			lprintf(3, "ERROR: invalid room section.\n");
			lprintf(3, "Your data files are now corrupt.\n");
			fclose(imfp);
			return;
			}
		}
	}



void import_a_user(void) {
	char key[256], value[256];
	struct usersupp us;

	bzero(&us, sizeof(struct usersupp));	
	while(fpgetfield(imfp, key), strcasecmp(key, "enduser")) {
		if (strcasecmp(key, "mail")) {
			fpgetfield(imfp, value);
			}
		else {
			strcpy(value, "");
			}

		if (!strcasecmp(key, "usuid"))
			us.USuid = atoi(value);
		if (!strcasecmp(key, "password")) {
			strcpy(us.password, value);
			}

		if (!strcasecmp(key, "flags"))
			us.flags = atoi(value);
		if (!strcasecmp(key, "timescalled"))
			us.timescalled = atoi(value);
		if (!strcasecmp(key, "posted"))
			us.posted = atoi(value);
		if (!strcasecmp(key, "fullname")) {
			strcpy(us.fullname, value);
			lprintf(9, "User <%s> ", us.fullname);
			}
		if (!strcasecmp(key, "axlevel"))
			us.axlevel = atoi(value);
		if (!strcasecmp(key, "usscreenwidth"))
			us.USscreenwidth = atoi(value);
		if (!strcasecmp(key, "usscreenheight"))
			us.USscreenheight = atoi(value);
		if (!strcasecmp(key, "usernum")) {
			us.usernum = atol(value);
			lprintf(9, "<#%ld> ", us.usernum);
			}
		if (!strcasecmp(key, "lastcall"))
			us.lastcall = atol(value);
		if (!strcasecmp(key, "usname"))
			strcpy(us.USname, value);
		if (!strcasecmp(key, "usaddr"))
			strcpy(us.USaddr, value);
		if (!strcasecmp(key, "uscity"))
			strcpy(us.UScity, value);
		if (!strcasecmp(key, "usstate"))
			strcpy(us.USstate, value);
		if (!strcasecmp(key, "uszip"))
			strcpy(us.USzip, value);
		if (!strcasecmp(key, "usphone"))
			strcpy(us.USphone, value);
		if (!strcasecmp(key, "usemail"))
			strcpy(us.USemail, value);
		if (!strcasecmp(key, "ususerpurge")) {
			us.USuserpurge = atoi(value);
			}
		}
	
	putuser(&us, us.fullname);

	lprintf(9, "\n");
	}


void imp_usersupp(void) {
	char key[256], value[256];
	
	while(fpgetfield(imfp, key), strcasecmp(key, "endsection")) {
		if (strcasecmp(key, "user")) {
			fpgetfield(imfp, value);
			}
		else {
			strcpy(value, "");
			}

		if (!strcasecmp(key, "user")) {
			import_a_user();
			}
		}		
	}


void imp_globals(void) {
	char key[256], value[256];

	get_control();
	while(fpgetfield(imfp, key), strcasecmp(key, "endsection")) {
		fpgetfield(imfp, value);
		lprintf(9, " %s = %s\n", key, value);

		if (!strcasecmp(key, "mmhighest"))
			CitControl.MMhighest = atol(value);
		if (!strcasecmp(key, "mmnextuser"))
			CitControl.MMnextuser = atol(value);
		if (!strcasecmp(key, "mmnextroom"))
			CitControl.MMnextroom = atol(value);

		}
	put_control();
	}



void imp_config(void) { 
	char key[256], value[256];
	FILE *fp;

	while(fpgetfield(imfp, key), strcasecmp(key, "endsection")) {
		fpgetfield(imfp, value);
		lprintf(9, " %s = %s\n", key, value);

		if (!strcasecmp(key, "c_nodename"))
			strcpy(config.c_nodename, value);
		if (!strcasecmp(key, "c_fqdn"))
			strcpy(config.c_fqdn, value);
		if (!strcasecmp(key, "c_humannode"))
			strcpy(config.c_humannode, value);
		if (!strcasecmp(key, "c_phonenum"))
			strcpy(config.c_phonenum, value);
		if (!strcasecmp(key, "c_phonenum"))
			strcpy(config.c_phonenum, value);
		if (!strcasecmp(key, "c_bbsuid"))
			config.c_bbsuid = atoi(value);
		if (!strcasecmp(key, "c_creataide"))
			config.c_creataide = atoi(value);
		if (!strcasecmp(key, "c_sleeping"))
			config.c_sleeping = atoi(value);
		if (!strcasecmp(key, "c_initax"))
			config.c_initax = atoi(value);
		if (!strcasecmp(key, "c_regiscall"))
			config.c_regiscall = atoi(value);
		if (!strcasecmp(key, "c_twitdetect"))
			config.c_twitdetect = atoi(value);
		if (!strcasecmp(key, "c_twitroom"))
			strcpy(config.c_twitroom, value);
		if (!strcasecmp(key, "c_moreprompt"))
			strcpy(config.c_moreprompt, value);
		if (!strcasecmp(key, "c_restrict"))
			config.c_restrict = atoi(value);
		if (!strcasecmp(key, "c_bbs_city"))
			strcpy(config.c_bbs_city, value);
		if (!strcasecmp(key, "c_sysadm"))
			strcpy(config.c_sysadm, value);
		if (!strcasecmp(key, "c_bucket_dir"))
			strcpy(config.c_bucket_dir, value);
		if (!strcasecmp(key, "c_setup_level"))
			config.c_setup_level = atoi(value);
		if (!strcasecmp(key, "c_maxsessions"))
			config.c_maxsessions = atoi(value);
		if (!strcasecmp(key, "c_net_password"))
			strcpy(config.c_net_password, value);
		if (!strcasecmp(key, "c_port_number"))
			config.c_port_number = atoi(value);
		if (!strcasecmp(key, "c_expire_policy"))
			config.c_ep.expire_mode = atoi(value);
		if (!strcasecmp(key, "c_expire_value"))
			config.c_ep.expire_value = atoi(value);
		if (!strcasecmp(key, "c_userpurge"))
			config.c_userpurge = atoi(value);
		}

	fp = fopen("citadel.config", "wb");
	fwrite(&config, sizeof(struct config), 1, fp);
	fclose(fp);
	}
		
		



void imp_ssv(void) {
	char key[256], value[256];
	int ssv_maxfloors = MAXFLOORS;
	
	while(fpgetfield(imfp, key), strcasecmp(key, "endsection")) {
		fpgetfield(imfp, value);
		lprintf(9, " %s = %s\n", key, value);
		
		if (!strcasecmp(key, "maxfloors")) {
			ssv_maxfloors = atol(value);
			if (ssv_maxfloors > MAXFLOORS) {
				lprintf(3, "ERROR: maxfloors is %d, need %d\n",
					ssv_maxfloors, MAXFLOORS);
				fclose(imfp);
				return;
				}
			}
		}
	}

void imp_visits(void) {
	char key[256], value[256];
	struct usersupp us;
	struct quickroom qr;
	struct visit visit;

	while(fpgetfield(imfp, key), strcasecmp(key, "endsection")) {
		lprintf(9, "%s: ", key);
		while(fpgetfield(imfp, key),
		    strcasecmp(key, "endvisit")) {
			fpgetfield(imfp, value);
			if (!strcasecmp(key, "vrnum")) {
				qr.QRnumber = atol(value);
				visit.v_roomnum = atol(value);
				}
			else if (!strcasecmp(key, "vgen")) {
				qr.QRgen = atol(value);
				visit.v_roomgen = atol(value);
				}
			else if (!strcasecmp(key, "vunum")) {
				us.usernum = atol(value);
				visit.v_usernum = atol(value);
				}
			else if (!strcasecmp(key, "lastseen")) {
				visit.v_lastseen = atol(value);
				}
			else if (!strcasecmp(key, "flags")) {
				visit.v_flags = atol(value);
				}
			}
		lprintf(9, "<%ld><%ld><%ld> <%d> <%ld>\n",
			visit.v_roomnum, visit.v_roomgen,
			visit.v_usernum,
			visit.v_flags, visit.v_lastseen);
		CtdlSetRelationship(&visit, &us, &qr);
		}

	}


void import_databases(void) {
	char section[256];

	lprintf(9, " ** IMPORTING ** \n");
	while (fpgetfield(imfp, section), strcasecmp(section, "endfile")) {
		lprintf(9, "Section: <%s>\n", section);

		if (!strcasecmp(section, "ssv")) 		imp_ssv();
		else if (!strcasecmp(section, "config"))	imp_config();
		else if (!strcasecmp(section, "globals"))	imp_globals();
		else if (!strcasecmp(section, "usersupp"))	imp_usersupp();
		else if (!strcasecmp(section, "rooms"))		imp_rooms();
		else if (!strcasecmp(section, "floors"))	imp_floors();
		else if (!strcasecmp(section, "visits"))	imp_visits();
		else {
			lprintf(3, "ERROR: invalid import section.\n");
			lprintf(3, "Your data files are now corrupt.\n");
			fclose(imfp);
			return;
			}

		}

	}



void do_import(char *argbuf) {
	char import_filename[PATH_MAX];
	
	extract(import_filename, argbuf, 0);
	imfp = fopen(import_filename, "rb");
	if (imfp == NULL) {
		lprintf(9, "Cannot open %s: %s\n",
			import_filename, strerror(errno));
		cprintf("%d Cannot open file\n", ERROR);
		return;
		}

	import_databases();
	lprintf(9, "Defragmenting databases (this may take a while)...\n");
	defrag_databases();
	lprintf(1, "Import is finished.  Shutting down Citadel...\n");
	cprintf("%d Import finished.  Shutting down Citadel...\n", OK);
	master_cleanup();
	}


void dump_message(long msg_num) {
	struct cdbdata *dmsgtext;

	dmsgtext = cdb_fetch(CDB_MSGMAIN, &msg_num, sizeof(long));
	
	if (dmsgtext == NULL) {
		lprintf(9, "%d Can't find message %ld\n", msg_num);
		return;
		}

	fprintf(exfp, "message%c%ld%c", 0, msg_num, 0);
	fprintf(exfp, "%ld%c", (long)dmsgtext->len, 0);
	fwrite(dmsgtext->ptr, dmsgtext->len, 1, exfp);

	cdb_free(dmsgtext);
	}


void export_a_room(struct quickroom *qr) {
	int b = 0;
	int msgcount = 0;

	lprintf(9,"<%s>\n", qr->QRname);
	fprintf(exfp, "room%c", 0);
	fprintf(exfp, "qrname%c%s%c", 0, qr->QRname, 0);
	fprintf(exfp, "qrnumber%c%ld%c", 0, qr->QRnumber, 0);
	fprintf(exfp, "qrpasswd%c%s%c", 0, qr->QRpasswd, 0);
	fprintf(exfp, "qrroomaide%c%ld%c", 0, qr->QRroomaide, 0);
	fprintf(exfp, "qrhighest%c%ld%c", 0, qr->QRhighest, 0);
	fprintf(exfp, "qrgen%c%ld%c", 0, qr->QRgen, 0);
	fprintf(exfp, "qrflags%c%d%c", 0, qr->QRflags, 0);
	fprintf(exfp, "qrdirname%c%s%c", 0, qr->QRdirname, 0);
	fprintf(exfp, "qrinfo%c%ld%c", 0, qr->QRinfo, 0);
	fprintf(exfp, "qrfloor%c%d%c", 0, qr->QRfloor, 0);
	fprintf(exfp, "qrmtime%c%ld%c", 0, qr->QRmtime, 0);
	fprintf(exfp, "qrepmode%c%d%c", 0, qr->QRep.expire_mode, 0);
	fprintf(exfp, "qrepvalue%c%d%c", 0, qr->QRep.expire_value, 0);

	get_msglist(qr);
	if (CC->num_msgs > 0) for (b=0; b<(CC->num_msgs); ++b) {
		++msgcount;
		lprintf(9, "Message #%ld\n", MessageFromList(b));
		dump_message(MessageFromList(b));
		}

	fprintf(exfp, "endroom%c", 0);
	}


void export_rooms(void) {
	lprintf(9,"Rooms\n");
	fprintf(exfp, "rooms%c", 0);
	ForEachRoom(export_a_room);
	fprintf(exfp, "endsection%c", 0);
	}



void export_floors(void) {
	int floornum;
	struct floor fl;

	fprintf(exfp, "floors%c", 0);
	for (floornum=0; floornum<MAXFLOORS; ++floornum) {
		getfloor(&fl, floornum);
		fprintf(exfp, "floor%c", 0);
		fprintf(exfp, "f_flags%c%d%c", 0, fl.f_flags, 0);
		fprintf(exfp, "f_name%c%s%c", 0, fl.f_name, 0);
		fprintf(exfp, "f_ref_count%c%d%c", 0, fl.f_ref_count, 0);
		fprintf(exfp, "f_expire_mode%c%d%c", 0, fl.f_ep.expire_mode, 0);
		fprintf(exfp, "f_expire_value%c%d%c", 0,
				fl.f_ep.expire_value, 0);
		fprintf(exfp, "endfloor%c", 0);
		}
	fprintf(exfp, "endsection%c", 0);
	}




void export_a_user(struct usersupp *us) {

	lprintf(9, "User <%s> ", us->fullname);

	fprintf(exfp, "user%c", 0);
	fprintf(exfp, "usuid%c%d%c", 0, us->USuid, 0);
	fprintf(exfp, "password%c%s%c", 0, us->password, 0);
	fprintf(exfp, "flags%c%d%c", 0, us->flags, 0);
	fprintf(exfp, "timescalled%c%d%c", 0, us->timescalled, 0);
	fprintf(exfp, "posted%c%d%c", 0, us->posted, 0);
	fprintf(exfp, "fullname%c%s%c", 0, us->fullname, 0);
	fprintf(exfp, "axlevel%c%d%c", 0, us->axlevel, 0);
	fprintf(exfp, "usscreenwidth%c%d%c", 0, us->USscreenwidth, 0);
	fprintf(exfp, "usscreenheight%c%d%c", 0, us->USscreenheight, 0);
	fprintf(exfp, "usernum%c%ld%c", 0, us->usernum, 0);
	fprintf(exfp, "lastcall%c%ld%c", 0, us->lastcall, 0);
	fprintf(exfp, "usname%c%s%c", 0, us->USname, 0);
	fprintf(exfp, "usaddr%c%s%c", 0, us->USaddr, 0);
	fprintf(exfp, "uscity%c%s%c", 0, us->UScity, 0);
	fprintf(exfp, "usstate%c%s%c", 0, us->USstate, 0);
	fprintf(exfp, "uszip%c%s%c", 0, us->USzip, 0);
	fprintf(exfp, "usphone%c%s%c", 0, us->USphone, 0);
	fprintf(exfp, "usemail%c%s%c", 0, us->USemail, 0);
	fprintf(exfp, "ususerpurge%c%d%c", 0, us->USuserpurge, 0);

	lprintf(9, "\n");
	fprintf(exfp, "enduser%c", 0);
	}


void export_usersupp(void) {
	lprintf(9, "Users\n");
	fprintf(exfp, "usersupp%c", 0);

	ForEachUser(export_a_user);

	fprintf(exfp, "endsection%c", 0);
	}
	

void do_export(char *argbuf) {
	char export_filename[PATH_MAX];
	
	extract(export_filename, argbuf, 0);
	exfp = fopen(export_filename, "wb");
	if (exfp == NULL) {
		lprintf(9, "Cannot open %s: %s\n",
			export_filename, strerror(errno));
		cprintf("%d Cannot open file\n", ERROR);
		return;
		}

	/* structure size variables */
	lprintf(9, "Structure size variables\n");
	fprintf(exfp, "ssv%c", 0);
	fprintf(exfp, "maxfloors%c%d%c", 0, MAXFLOORS, 0);
	fprintf(exfp, "endsection%c", 0);

	/* Write out the server config */
	lprintf(9,"Server config\n");
	fprintf(exfp, "config%c", 0);
	fprintf(exfp, "c_nodename%c%s%c", 0, config.c_nodename, 0);
	fprintf(exfp, "c_fqdn%c%s%c", 0, config.c_fqdn, 0);
	fprintf(exfp, "c_humannode%c%s%c", 0, config.c_humannode, 0);
	fprintf(exfp, "c_phonenum%c%s%c", 0, config.c_phonenum, 0);
	fprintf(exfp, "c_bbsuid%c%d%c", 0, config.c_bbsuid, 0);
	fprintf(exfp, "c_creataide%c%d%c", 0, config.c_creataide, 0);
	fprintf(exfp, "c_sleeping%c%d%c", 0, config.c_sleeping, 0);
	fprintf(exfp, "c_initax%c%d%c", 0, config.c_initax, 0);
	fprintf(exfp, "c_regiscall%c%d%c", 0, config.c_regiscall, 0);
	fprintf(exfp, "c_twitdetect%c%d%c", 0, config.c_twitdetect, 0);
	fprintf(exfp, "c_twitroom%c%s%c", 0, config.c_twitroom, 0);
	fprintf(exfp, "c_moreprompt%c%s%c", 0, config.c_moreprompt, 0);
	fprintf(exfp, "c_restrict%c%d%c", 0, config.c_restrict, 0);
	fprintf(exfp, "c_bbs_city%c%s%c", 0, config.c_bbs_city, 0);
	fprintf(exfp, "c_sysadm%c%s%c", 0, config.c_sysadm, 0);
	fprintf(exfp, "c_bucket_dir%c%s%c", 0, config.c_bucket_dir, 0);
	fprintf(exfp, "c_setup_level%c%d%c", 0, config.c_setup_level, 0);
	fprintf(exfp, "c_maxsessions%c%d%c", 0, config.c_maxsessions, 0);
	fprintf(exfp, "c_net_password%c%s%c", 0, config.c_net_password, 0);
	fprintf(exfp, "c_port_number%c%d%c", 0, config.c_port_number, 0);
	fprintf(exfp, "c_expire_policy%c%d%c", 0, config.c_ep.expire_mode, 0);
	fprintf(exfp, "c_expire_value%c%d%c", 0, config.c_ep.expire_value, 0);
	fprintf(exfp, "c_userpurge%c%d%c", 0, config.c_userpurge, 0);
	fprintf(exfp, "endsection%c", 0);

	/* Now some global stuff */
	lprintf(9, "Globals\n");
	get_control();
	fprintf(exfp, "globals%c", 0);
	fprintf(exfp, "mmhighest%c%ld%c", 0, CitControl.MMhighest, 0);
	fprintf(exfp, "mmnextuser%c%ld%c", 0, CitControl.MMnextuser, 0);
	fprintf(exfp, "mmnextroom%c%ld%c", 0, CitControl.MMnextroom, 0);
	fprintf(exfp, "mmflags%c%d%c", 0, CitControl.MMflags, 0);
	fprintf(exfp, "endsection%c", 0);

	/* Export all of the databases */
	export_rooms();
	export_floors();
	export_usersupp();

	fprintf(exfp, "endfile%c", 0);
	fclose(exfp);
	lprintf(1, "Export is finished.\n");
	cprintf("%d Export is finished.\n", OK);
	}




struct DLModule_Info *Dynamic_Module_Init(void) {
	CtdlRegisterProtoHook(do_import,
				"IMPO",
				"Import an unpacked system");
	CtdlRegisterProtoHook(do_export,
				"EXPO",
				"Export the system");
	return &info;
	}
