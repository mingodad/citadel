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
FILE *imfp;

#define MODULE_NAME 	"Import an unpacked system"
#define MODULE_AUTHOR	"Art Cancro"
#define MODULE_EMAIL	"ajc@uncnsrd.mt-kisco.ny.us"
#define MAJOR_VERSION	0
#define MINOR_VERSION	2

static struct DLModule_Info info = {
  MODULE_NAME,
  MODULE_AUTHOR,
  MODULE_EMAIL,
  MAJOR_VERSION,
  MINOR_VERSION
};




void fpgetfield(fp,string)
FILE *fp;
char string[];
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

void imp_floors() {
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



void imp_rooms() {
	char key[256];
	char tag[256], tval[256];
	int roomnum = 0;
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
					lprintf(9, "<%s>", qr.QRname);
					}
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
				if (!strcasecmp(tag, "message")) {
					lprintf(9, ".");
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

			lprintf(9, "\n");
			if ((roomnum!=1)&&(qr.QRflags&QR_INUSE)) {
				putroom(&qr, qr.QRname);
				}

			if (num_msgs > 0) {
				if ((roomnum!=1)&&(qr.QRflags&QR_INUSE)) {
					CC->msglist = msglist;
					CC->num_msgs = num_msgs;
					put_msglist(&qr);
					}
				free(msglist);
				}

			++roomnum;

			}
		else {
			lprintf(3, "ERROR: invalid room section.\n");
			lprintf(3, "Your data files are now corrupt.\n");
			fclose(imfp);
			return;
			}
		}
	}





void import_a_user() {
	char key[256], value[256];
	char vkey[256], vvalue[256];
	struct usersupp us;
	struct quickroom qr;
	struct visit vbuf;

	bzero(&us, sizeof(struct usersupp));	
	while(fpgetfield(imfp, key), strcasecmp(key, "enduser")) {
		if ((strcasecmp(key, "mail"))
		   &&(strcasecmp(key, "visit")) ) {
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
			lprintf(9, "User <%s>", us.fullname);
			}
		if (!strcasecmp(key, "axlevel"))
			us.axlevel = atoi(value);
		if (!strcasecmp(key, "usscreenwidth"))
			us.USscreenwidth = atoi(value);
		if (!strcasecmp(key, "usscreenheight"))
			us.USscreenheight = atoi(value);
		if (!strcasecmp(key, "usernum"))
			us.usernum = atol(value);
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
		if (!strcasecmp(key, "visit")) {
			lprintf(9,"v");
			bzero(&vbuf, sizeof(struct visit));
			bzero(&qr, sizeof(struct quickroom));
			while(fpgetfield(imfp, vkey),
			  strcasecmp(vkey, "endvisit")) {
				fpgetfield(imfp, vvalue);
				if (!strcasecmp(vkey, "vname"))	
					strcpy(qr.QRname, vvalue);
				if (!strcasecmp(vkey, "vgen"))	{
					qr.QRgen = atol(vvalue);
					CtdlGetRelationship(&vbuf, &us, &qr);
					}
				if (!strcasecmp(vkey, "lastseen"))	
					vbuf.v_lastseen = atol(vvalue);
				if (!strcasecmp(vkey, "flags"))
					vbuf.v_flags = atoi(vvalue);
				}
			CtdlSetRelationship(&vbuf, &us, &qr);
			}
		}
	
	putuser(&us, us.fullname);

	lprintf(9, "\n");
	}


void imp_usersupp() {
	char key[256], value[256];
	
	while(fpgetfield(imfp, key), strcasecmp(key, "endsection")) {
		if (strcasecmp(key, "user")) {
			fpgetfield(imfp, value);
			}
		else {
			strcpy(value, "");
			}
		lprintf(9, " %s = %s\n", key, value);

		if (!strcasecmp(key, "user")) {
			import_a_user();
			}
		}		
	}





void imp_globals() {
	char key[256], value[256];

	get_control();
	while(fpgetfield(imfp, key), strcasecmp(key, "endsection")) {
		fpgetfield(imfp, value);
		lprintf(9, " %s = %s\n", key, value);

		if (!strcasecmp(key, "mmhighest"))
			CitControl.MMhighest = atol(value);
		if (!strcasecmp(key, "mmnextuser"))
			CitControl.MMnextuser = atol(value);

		}
	put_control();
	}



void imp_config() { 
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
		if (!strcasecmp(key, "c_defent"))
			config.c_defent = atoi(value);
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
		}

	fp = fopen("citadel.config", "wb");
	fwrite(&config, sizeof(struct config), 1, fp);
	fclose(fp);
	}
		
		



void imp_ssv() {
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







void import_databases() {
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
	

	if (num_parms(argbuf) != 1) {
		cprintf("%d usage: IMPO unpacked_filename\n", ERROR);
		return;
		}
	extract(import_filename, argbuf, 0);
	imfp = fopen(import_filename, "rb");
	if (imfp == NULL) {
		lprintf(9, "Cannot open %s: %s\n",
			import_filename, strerror(errno));
		cprintf("%d Cannot open file\n", ERROR);
		return;
		}

	import_databases();
	cprintf("%d ok\n", OK);
	}


struct DLModule_Info *Dynamic_Module_Init(void) {
	CtdlRegisterProtoHook(do_import,
				"IMPO",
				"Import an unpacked Cit5");
	return &info;
	}
