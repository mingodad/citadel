/* $Id$ */
/* cc import.c database.o control.o -lgdbm -o import */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <pthread.h>
#include "citadel.h"
#include "server.h"

int ssv_maxrooms = 0;
int ssv_maxfloors = 0;
FILE *imfp;


/**** stubs which need to be defined for database.c to work ****/

struct config config;
struct CitContext MyCC;

struct CitContext *MyContext() {
	return(&MyCC);
	}

void begin_critical_section(int c) { }
void end_critical_section(int c) { }

void lprintf(int loglevel, const char *format, ...) {   
        va_list arg_ptr;   
  
	va_start(arg_ptr, format);   
	vfprintf(stderr, format, arg_ptr);   
	va_end(arg_ptr);   
        }


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

void imp_ssv() {
	char key[256], value[256];
	
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


void imp_config() { 
	char key[256], value[256];
	FILE *fp;

	bzero(&config, sizeof(struct config));	
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
		}

	fp = fopen("citadel.config", "wb");
	fwrite(&config, sizeof(struct config), 1, fp);
	fclose(fp);
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

void import_message(long msgnum, long msglen) {
	long count;
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

void import_a_user() {
	char key[256], value[256], list[256];
	char lcasename[256];
	struct usersupp us;
	int a;
	long *mbox = NULL;
	int mbox_size = 0;
	long msgnum;
	long msglen;

	bzero(&us, sizeof(struct usersupp));	
	while(fpgetfield(imfp, key), strcasecmp(key, "enduser")) {
		if ((strcasecmp(key, "mail"))
		   &&(strcasecmp(key, "lastseen"))
		   &&(strcasecmp(key, "generation"))
		   &&(strcasecmp(key, "forget")) ) {
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
		if (!strcasecmp(key, "lastseen"))
			for (a=0; a<ssv_maxrooms; ++a) {
				fpgetfield(imfp, list);
				us.lastseen[a] = atol(list);
				}
		if (!strcasecmp(key, "generation"))
			for (a=0; a<ssv_maxrooms; ++a) {
				fpgetfield(imfp, list);
				us.generation[a] = atol(list);
				}
		if (!strcasecmp(key, "forget"))
			for (a=0; a<ssv_maxrooms; ++a) {
				fpgetfield(imfp, list);
				us.forget[a] = atol(list);
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
		if (!strcasecmp(key, "mail")) {
			lprintf(9, ".");
			fpgetfield(imfp, list);
			msgnum = atol(list);
			fpgetfield(imfp, list);
			msglen = atol(list);
			import_message(msgnum, msglen);
			++mbox_size;
			mbox = realloc(mbox, (sizeof(long)*mbox_size) );
			mbox[mbox_size - 1] = msgnum;
			}
		}
	
	for (a=0; a<=strlen(us.fullname); ++a) {
		lcasename[a] = tolower(us.fullname[a]);
		}
	cdb_store(CDB_USERSUPP,
		lcasename, strlen(lcasename),
		&us, sizeof(struct usersupp));
	if (mbox_size > 0)  {
		cdb_store(CDB_MAILBOXES, 
			&us.usernum, sizeof(long),
			mbox, (mbox_size * sizeof(long)) );
		free(mbox);
		}
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

			cdb_store(CDB_FLOORTAB,
				&floornum, sizeof(int),
				&fl, sizeof(struct floor) );
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
	char key[256], value[256];
	char tag[256], tval[256];
	int roomnum = 0;
	struct quickroom qr;
	long *msglist;
	int num_msgs = 0;
	long msgnum, msglen;
	char cdbkey[256];
	
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
				cdb_store(CDB_QUICKROOM,
					&roomnum, sizeof(int),
					&qr, sizeof(struct quickroom) );
				}

			if (num_msgs > 0) {
				if ((roomnum!=1)&&(qr.QRflags&QR_INUSE)) {
                		 	cdb_store(CDB_MSGLISTS,
						&roomnum, sizeof(int),
						msglist,
						(sizeof(long)*num_msgs) );
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



void import_databases() {
	char section[256];

	lprintf(9, " ** IMPORTING ** \n");
	imfp = fopen("/appl/citadel/exported", "rb");
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




void main() {
	open_databases();
	import_databases();
	close_databases();
	exit(0);
	}
