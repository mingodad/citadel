/*
 * $Id$
 *
 * This is the "Art Vandelay" module.  It is an importer/exporter.
 *
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <pwd.h>
#include <errno.h>
#include <sys/types.h>

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

#include <sys/wait.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "database.h"
#include "msgbase.h"
#include "user_ops.h"
#include "room_ops.h"
#include "control.h"
#include "euidindex.h"


#include "ctdl_module.h"




#define END_OF_MESSAGE	"---eom---dbd---"

char artv_tempfilename1[PATH_MAX];
char artv_tempfilename2[PATH_MAX];
FILE *artv_global_message_list;

void artv_export_users_backend(struct ctdluser *buf, void *data) {
	client_write("user\n", 5);
/*
#include "artv_serialize.h"
#include "dtds/user-defs.h"
#include "undef_data.h"
*/
	cprintf("%d\n", buf->version);
	cprintf("%ld\n", (long)buf->uid);
	cprintf("%s\n", buf->password);
	cprintf("%u\n", buf->flags);
	cprintf("%ld\n", buf->timescalled);
	cprintf("%ld\n", buf->posted);
	cprintf("%d\n", buf->axlevel);
	cprintf("%ld\n", buf->usernum);
	cprintf("%ld\n", (long)buf->lastcall);
	cprintf("%d\n", buf->USuserpurge);
	cprintf("%s\n", buf->fullname);
	cprintf("%d\n", buf->USscreenwidth);
	cprintf("%d\n", buf->USscreenheight);
}

void artv_dump_users_backend(struct ctdluser *buf, void *data) {
	client_write("user\n", 5);

#include "artv_dump.h"
#include "dtds/user-defs.h"
#include "undef_data.h"
	cprintf("\n");
}


INLINE int cprintdot (long *iterations)
{
	int retval = 0;
	
	retval += client_write(".", 1);
	++(*iterations);
	if ((*iterations) % 64 == 0)
		retval += client_write("\n", 1);
	return retval;
}



void artv_export_users(void) {
	ForEachUser(artv_export_users_backend, NULL);
}

void artv_dump_users(void) {
	ForEachUser(artv_dump_users_backend, NULL);
}


void artv_export_room_msg(long msgnum, void *userdata) {
	cprintf("%ld\n", msgnum);
	fprintf(artv_global_message_list, "%ld\n", msgnum);
}

void artv_dump_room_msg(long msgnum, void *userdata) {
	cprintf(" msgnum: %ld\n", msgnum);
	fprintf(artv_global_message_list, "%ld\n", msgnum);
	cprintf("\n");
}//// TODO


void artv_export_rooms_backend(struct ctdlroom *buf, void *data) {
	client_write("room\n", 5);
/*
#include "artv_serialize.h"
#include "dtds/room-defs.h"
#include "undef_data.h"
*/
	cprintf("%s\n", buf->QRname);
	cprintf("%s\n", buf->QRpasswd);
	cprintf("%ld\n", buf->QRroomaide);
	cprintf("%ld\n", buf->QRhighest);
	cprintf("%ld\n", (long)buf->QRgen);
	cprintf("%u\n", buf->QRflags);
	cprintf("%s\n", buf->QRdirname);
	cprintf("%ld\n", buf->QRinfo);
	cprintf("%d\n", buf->QRfloor);
	cprintf("%ld\n", (long)buf->QRmtime);
	cprintf("%d\n", buf->QRep.expire_mode);
	cprintf("%d\n", buf->QRep.expire_value);
	cprintf("%ld\n", buf->QRnumber);
	cprintf("%d\n", buf->QRorder);
	cprintf("%u\n", buf->QRflags2);
	cprintf("%d\n", buf->QRdefaultview);

	getroom(&CC->room, buf->QRname);
	/* format of message list export is all message numbers output
	 * one per line terminated by a 0.
	 */
//*/
	getroom(&CC->room, buf->QRname);
	/* format of message list export is all message numbers output
	 * one per line terminated by a 0.
	 */
	CtdlForEachMessage(MSGS_ALL, 0L, NULL, NULL, NULL,
		artv_export_room_msg, NULL);
	cprintf("0\n");

}

void artv_dump_rooms_backend(struct ctdlroom *buf, void *data) {
	client_write("room\n", 5);

#include "artv_dump.h"
#include "dtds/room-defs.h"
#include "undef_data.h"

	getroom(&CC->room, buf->QRname);
	/* format of message list export is all message numbers output
	 * one per line terminated by a 0.
	 */
//*/
	getroom(&CC->room, buf->QRname);
	/* format of message list export is all message numbers output
	 * one per line terminated by a 0.
	 */
	CtdlForEachMessage(MSGS_ALL, 0L, NULL, NULL, NULL,
		artv_dump_room_msg, NULL);
	cprintf("\n\n");

}



void artv_export_rooms(void) {
	char cmd[SIZ];
	artv_global_message_list = fopen(artv_tempfilename1, "w");
	if (artv_global_message_list != NULL) {
		ForEachRoom(artv_export_rooms_backend, NULL);
		fclose(artv_global_message_list);
	}

	/*
	 * Process the 'global' message list.  (Sort it and remove dups.
	 * Dups are ok because a message may be in more than one room, but
	 * this will be handled by exporting the reference count, not by
	 * exporting the message multiple times.)
	 */
	snprintf(cmd, sizeof cmd, "sort <%s >%s", artv_tempfilename1, artv_tempfilename2);
	system(cmd);
	snprintf(cmd, sizeof cmd, "uniq <%s >%s", artv_tempfilename2, artv_tempfilename1);
	system(cmd);
}

void artv_dump_rooms(void) {
	char cmd[SIZ];
	artv_global_message_list = fopen(artv_tempfilename1, "w");
	if (artv_global_message_list != NULL) {
		ForEachRoom(artv_dump_rooms_backend, NULL);
		fclose(artv_global_message_list);
	}

	/*
	 * Process the 'global' message list.  (Sort it and remove dups.
	 * Dups are ok because a message may be in more than one room, but
	 * this will be handled by exporting the reference count, not by
	 * exporting the message multiple times.)
	 */
	snprintf(cmd, sizeof cmd, "sort <%s >%s", artv_tempfilename1, artv_tempfilename2);
	system(cmd);
	snprintf(cmd, sizeof cmd, "uniq <%s >%s", artv_tempfilename2, artv_tempfilename1);
	system(cmd);
}


void artv_export_floors(void) {
        struct floor qfbuf, *buf;
        int i;

        for (i=0; i < MAXFLOORS; ++i) {
		client_write("floor\n", 6);
		cprintf("%d\n", i);
                getfloor(&qfbuf, i);
		buf = &qfbuf;
/*
#include "artv_serialize.h"
#include "dtds/floor-defs.h"
#include "undef_data.h"
/*/
		cprintf("%u\n", buf->f_flags);
		cprintf("%s\n", buf->f_name);
		cprintf("%d\n", buf->f_ref_count);
		cprintf("%d\n", buf->f_ep.expire_mode);
		cprintf("%d\n", buf->f_ep.expire_value);
//*/
	}
}

void artv_dump_floors(void) {
        struct floor qfbuf, *buf;
        int i;

        for (i=0; i < MAXFLOORS; ++i) {
		client_write("floor\n", 6);
		cprintf("%d\n", i);
                getfloor(&qfbuf, i);
		buf = &qfbuf;

#include "artv_serialize.h"
#include "dtds/floor-defs.h"
#include "undef_data.h"
	}
}





/* 
 *  Traverse the visits file...
 */
void artv_export_visits(void) {
	struct visit vbuf;
	struct cdbdata *cdbv;

	cdb_rewind(CDB_VISIT);

	while (cdbv = cdb_next_item(CDB_VISIT), cdbv != NULL) {
		memset(&vbuf, 0, sizeof(struct visit));
		memcpy(&vbuf, cdbv->ptr,
		       ((cdbv->len > sizeof(struct visit)) ?
			sizeof(struct visit) : cdbv->len));
		cdb_free(cdbv);

		client_write("visit\n", 6);
		cprintf("%ld\n", vbuf.v_roomnum);
		cprintf("%ld\n", vbuf.v_roomgen);
		cprintf("%ld\n", vbuf.v_usernum);

		if (!IsEmptyStr(vbuf.v_seen)) {
			cprintf("%s\n", vbuf.v_seen);
		}
		else {
			cprintf("%ld\n", vbuf.v_lastseen);
		}

		cprintf("%s\n", vbuf.v_answered);
		cprintf("%u\n", vbuf.v_flags);
		cprintf("%d\n", vbuf.v_view);
	}
}

/* 
 *  Traverse the visits file...
 */
void artv_dump_visits(void) {
	struct visit vbuf;
	struct cdbdata *cdbv;

	cdb_rewind(CDB_VISIT);

	while (cdbv = cdb_next_item(CDB_VISIT), cdbv != NULL) {
		memset(&vbuf, 0, sizeof(struct visit));
		memcpy(&vbuf, cdbv->ptr,
		       ((cdbv->len > sizeof(struct visit)) ?
			sizeof(struct visit) : cdbv->len));
		cdb_free(cdbv);

		client_write("---visit---\n", 12);
		cprintf(" Room-Num: %ld\n", vbuf.v_roomnum);
		cprintf(" Room-Gen%ld\n", vbuf.v_roomgen);
		cprintf(" User-Num%ld\n", vbuf.v_usernum);

		if (!IsEmptyStr(vbuf.v_seen)) {
			cprintf(" Seen: %s\n", vbuf.v_seen);
		}
		else {
			cprintf(" LastSeen: %ld\n", vbuf.v_lastseen);
		}

		cprintf(" Answered: %s\n", vbuf.v_answered);
		cprintf(" Flags: %u\n", vbuf.v_flags);
		cprintf(" View: %d\n", vbuf.v_view);
	}
}


void artv_export_message(long msgnum) {
	struct MetaData smi;
	struct CtdlMessage *msg;
	struct ser_ret smr;
	FILE *fp;
	char buf[SIZ];
	char tempfile[PATH_MAX];

	msg = CtdlFetchMessage(msgnum, 1);
	if (msg == NULL) return;	/* fail silently */

	client_write("message\n", 8);
	GetMetaData(&smi, msgnum);
	cprintf("%ld\n", msgnum);
	cprintf("%d\n", smi.meta_refcount);
	cprintf("%s\n", smi.meta_content_type);

	serialize_message(&smr, msg);
	CtdlFreeMessage(msg);

	/* write it in base64 */
	CtdlMakeTempFileName(tempfile, sizeof tempfile);
	snprintf(buf, sizeof buf, "%s -e >%s", file_base64, tempfile);
	fp = popen(buf, "w");
	fwrite(smr.ser, smr.len, 1, fp);
	pclose(fp);

	free(smr.ser);

	fp = fopen(tempfile, "r");
	unlink(tempfile);
	if (fp != NULL) {
		while (fgets(buf, sizeof(buf), fp) != NULL) {
			buf[strlen(buf)-1] = 0;
			cprintf("%s\n", buf);
		}
		fclose(fp);
	}
	cprintf("%s\n", END_OF_MESSAGE);
}

void artv_dump_message(long msgnum) {
	struct MetaData smi;
	struct CtdlMessage *msg;

	msg = CtdlFetchMessage(msgnum, 1);
	if (msg == NULL) return;	/* fail silently */

	client_write("message\n", 8);
	GetMetaData(&smi, msgnum);
	cprintf(" MessageNum: %ld\n", msgnum);
	cprintf(" MetaRefcount: %d\n", smi.meta_refcount);
	cprintf(" MetaContentType: %s\n", smi.meta_content_type);

	dump_message(msg, 80);
	CtdlFreeMessage(msg);

	cprintf("%s\n", END_OF_MESSAGE);
}



void artv_export_openids(void) {
	struct cdbdata *cdboi;
	long usernum;

	cdb_rewind(CDB_OPENID);
	while (cdboi = cdb_next_item(CDB_OPENID), cdboi != NULL) {
		if (cdboi->len > sizeof(long)) {
			client_write("openid\n", 7);
			memcpy(&usernum, cdboi->ptr, sizeof(long));
			cprintf("%s\n", (cdboi->ptr)+sizeof(long) );
			cprintf("%ld\n", usernum);
		}
		cdb_free(cdboi);
	}
}




void artv_export_messages(void) {
	char buf[SIZ];
	long msgnum;
	int count = 0;
	t_context *Ctx;

	Ctx = CC;
	artv_global_message_list = fopen(artv_tempfilename1, "r");
	if (artv_global_message_list != NULL) {
		CtdlLogPrintf(CTDL_INFO, "Opened %s\n", artv_tempfilename1);
		while ((Ctx->kill_me != 1) && 
		       (fgets(buf, sizeof(buf), artv_global_message_list) != NULL)) {
			msgnum = atol(buf);
			if (msgnum > 0L) {
				artv_export_message(msgnum);
				++count;
			}
		}
		fclose(artv_global_message_list);
	}
	if (Ctx->kill_me != 1)
		CtdlLogPrintf(CTDL_INFO, "Exported %d messages.\n", count);
	else
		CtdlLogPrintf(CTDL_ERR, "Export aborted due to client disconnect! \n");
}

void artv_dump_messages(void) {
	char buf[SIZ];
	long msgnum;
	int count = 0;
	t_context *Ctx;

	Ctx = CC;
	artv_global_message_list = fopen(artv_tempfilename1, "r");
	if (artv_global_message_list != NULL) {
		CtdlLogPrintf(CTDL_INFO, "Opened %s\n", artv_tempfilename1);
		while ((Ctx->kill_me != 1) && 
		       (fgets(buf, sizeof(buf), artv_global_message_list) != NULL)) {
			msgnum = atol(buf);
			if (msgnum > 0L) {
				artv_dump_message(msgnum);
				++count;
			}
		}
		fclose(artv_global_message_list);
	}
	if (Ctx->kill_me != 1)
		CtdlLogPrintf(CTDL_INFO, "Exported %d messages.\n", count);
	else
		CtdlLogPrintf(CTDL_ERR, "Export aborted due to client disconnect! \n");
}




void artv_do_export(void) {
	struct config *buf;
	buf = &config;
	t_context *Ctx;

	Ctx = CC;
	cprintf("%d Exporting all Citadel databases.\n", LISTING_FOLLOWS);
	Ctx->dont_term = 1;
	cprintf("version\n%d\n", REV_LEVEL);

	/* export the config file (this is done using x-macros) */
	client_write("config\n", 7);

#include "artv_serialize.h"
#include "dtds/config-defs.h"
#include "undef_data.h"
	client_write("\n", 1);
	
	/* Export the control file */
	get_control();
	client_write("control\n", 8);
	cprintf("%ld\n", CitControl.MMhighest);
	cprintf("%u\n", CitControl.MMflags);
	cprintf("%ld\n", CitControl.MMnextuser);
	cprintf("%ld\n", CitControl.MMnextroom);
	cprintf("%d\n", CitControl.version);
	if (Ctx->kill_me != 1)
		artv_export_users();
	if (Ctx->kill_me != 1)
		artv_export_openids();
	if (Ctx->kill_me != 1)
		artv_export_rooms();
	if (Ctx->kill_me != 1)
		artv_export_floors();
	if (Ctx->kill_me != 1)
		artv_export_visits();
	if (Ctx->kill_me != 1)
		artv_export_messages();
	client_write("000\n", 4);
	Ctx->dont_term = 0;
}

void artv_do_dump(void) {
	struct config *buf;
	buf = &config;
	t_context *Ctx;

	Ctx = CC;
	cprintf("%d dumping Citadel structures.\n", LISTING_FOLLOWS);

	cprintf("version\n%d\n", REV_LEVEL);

	/* export the config file (this is done using x-macros) */
	client_write("config\n", 7);

#include "artv_dump.h"
#include "dtds/config-defs.h"
#include "undef_data.h"

	/* Export the control file */
	get_control();
	client_write("control\n", 8);
	cprintf(" MMhighest: %ld\n", CitControl.MMhighest);
	cprintf(" MMflags: %u\n", CitControl.MMflags);
	cprintf(" MMnextuser: %ld\n", CitControl.MMnextuser);
	cprintf(" MMnextroom: %ld\n", CitControl.MMnextroom);
	cprintf(" version: %d\n\n", CitControl.version);
	if (Ctx->kill_me != 1)
		artv_dump_users();
	if (Ctx->kill_me != 1)
		artv_dump_rooms();
	if (Ctx->kill_me != 1)
		artv_dump_floors();
	if (Ctx->kill_me != 1)
		artv_dump_visits();
	if (Ctx->kill_me != 1)
		artv_dump_messages();

	client_write("000\n", 4);
}



void artv_import_config(void) {
	char cbuf[SIZ];
	struct config *buf;
	buf = &config;

	CtdlLogPrintf(CTDL_DEBUG, "Importing config file\n");

#include "artv_deserialize.h"
#include "dtds/config-defs.h"
#include "undef_data.h"

	config.c_enable_fulltext = 0;	/* always disable */
	put_config();
	CtdlLogPrintf(CTDL_INFO, "Imported config file\n");
}


void artv_import_control(void) {
	char buf[SIZ];

	CtdlLogPrintf(CTDL_DEBUG, "Importing control file\n");
	client_getln(buf, sizeof buf);	CitControl.MMhighest = atol(buf);
	client_getln(buf, sizeof buf);	CitControl.MMflags = atoi(buf);
	client_getln(buf, sizeof buf);	CitControl.MMnextuser = atol(buf);
	client_getln(buf, sizeof buf);	CitControl.MMnextroom = atol(buf);
	client_getln(buf, sizeof buf);	CitControl.version = atoi(buf);
	CitControl.MMfulltext = (-1L);	/* always flush */
	put_control();
	CtdlLogPrintf(CTDL_INFO, "Imported control file\n");
}


void artv_import_user(void) {
	char cbuf[SIZ];
	struct ctdluser usbuf, *buf;
	buf = &usbuf;
/*
#include "artv_deserialize.h"
#include "dtds/user-defs.h"
#include "undef_data.h"

/*/
	client_getln(cbuf, sizeof cbuf);	buf->version = atoi(cbuf);
	client_getln(cbuf, sizeof cbuf);	buf->uid = atoi(cbuf);
	client_getln(buf->password, sizeof buf->password);
	client_getln(cbuf, sizeof cbuf);	buf->flags = atoi(cbuf);
	client_getln(cbuf, sizeof cbuf);	buf->timescalled = atol(cbuf);
	client_getln(cbuf, sizeof cbuf);	buf->posted = atol(cbuf);
	client_getln(cbuf, sizeof cbuf);	buf->axlevel = atoi(cbuf);
	client_getln(cbuf, sizeof cbuf);	buf->usernum = atol(cbuf);
	client_getln(cbuf, sizeof cbuf);	buf->lastcall = atol(cbuf);
	client_getln(cbuf, sizeof cbuf);	buf->USuserpurge = atoi(cbuf);
	client_getln(buf->fullname, sizeof buf->fullname);
	client_getln(cbuf, sizeof cbuf);	buf->USscreenwidth = atoi(cbuf);
	client_getln(cbuf, sizeof cbuf);	buf->USscreenheight = atoi(cbuf);
//*/
	putuser(buf);
}


void artv_import_room(long *iterations) {
	char cbuf[SIZ];
	struct ctdlroom qrbuf, *buf;
	long msgnum;
	int msgcount = 0;

	buf = &qrbuf;
/*
#include "artv_deserialize.h"
#include "dtds/room-defs.h"
#include "undef_data.h"

/*/
	client_getln(buf->QRname, sizeof buf->QRname);
	client_getln(buf->QRpasswd, sizeof buf->QRpasswd);
	client_getln(cbuf, sizeof cbuf);	buf->QRroomaide = atol(cbuf);
	client_getln(cbuf, sizeof cbuf);	buf->QRhighest = atol(cbuf);
	client_getln(cbuf, sizeof cbuf);	buf->QRgen = atol(cbuf);
	client_getln(cbuf, sizeof cbuf);	buf->QRflags = atoi(cbuf);
	client_getln(buf->QRdirname, sizeof buf->QRdirname);
	client_getln(cbuf, sizeof cbuf);	buf->QRinfo = atol(cbuf);
	client_getln(cbuf, sizeof cbuf);	buf->QRfloor = atoi(cbuf);
	client_getln(cbuf, sizeof cbuf);	buf->QRmtime = atol(cbuf);
	client_getln(cbuf, sizeof cbuf);	buf->QRep.expire_mode = atoi(cbuf);
	client_getln(cbuf, sizeof cbuf);	buf->QRep.expire_value = atoi(cbuf);
	client_getln(cbuf, sizeof cbuf);	buf->QRnumber = atol(cbuf);
	client_getln(cbuf, sizeof cbuf);	buf->QRorder = atoi(cbuf);
	client_getln(cbuf, sizeof cbuf);	buf->QRflags2 = atoi(cbuf);
	client_getln(cbuf, sizeof cbuf);	buf->QRdefaultview = atoi(cbuf);
//*/
	putroom(buf);
	CtdlLogPrintf(CTDL_INFO, "Imported room <%s>\n", qrbuf.QRname);
	/* format of message list export is all message numbers output
	 * one per line terminated by a 0.
	 */
	while ((client_getln(cbuf, sizeof cbuf) >= 0) && (msgnum = atol(cbuf))) {
		CtdlSaveMsgPointerInRoom(qrbuf.QRname, msgnum, 0, NULL);
		cprintdot(iterations);
		++msgcount;
		if (CtdlThreadCheckStop())
			break;
	}
	CtdlLogPrintf(CTDL_INFO, "(%d messages)\n", msgcount);
}


void artv_import_floor(void) {
        struct floor flbuf, *buf;
        int i;
	char cbuf[SIZ];

	buf = & flbuf;
	memset(buf, 0, sizeof(buf));
	client_getln(cbuf, sizeof cbuf);	i = atoi(cbuf);
/*
#include "artv_deserialize.h"
#include "dtds/floor-defs.h"
#include "undef_data.h"
/*/
	client_getln(cbuf, sizeof cbuf);	buf->f_flags = atoi(cbuf);
	client_getln(buf->f_name, sizeof buf->f_name);
	client_getln(cbuf, sizeof cbuf);	buf->f_ref_count = atoi(cbuf);
	client_getln(cbuf, sizeof cbuf);	buf->f_ep.expire_mode = atoi(cbuf);
	client_getln(cbuf, sizeof cbuf);	buf->f_ep.expire_value = atoi(cbuf);
//*/
	putfloor(buf, i);
	CtdlLogPrintf(CTDL_INFO, "Imported floor #%d (%s)\n", i, flbuf.f_name);
}


void artv_import_visit(void) {
	struct visit vbuf;
	char buf[SIZ];
	int i;
	int is_textual_seen = 0;

	client_getln(buf, sizeof buf);	vbuf.v_roomnum = atol(buf);
	client_getln(buf, sizeof buf);	vbuf.v_roomgen = atol(buf);
	client_getln(buf, sizeof buf);	vbuf.v_usernum = atol(buf);

	client_getln(buf, sizeof buf);
	vbuf.v_lastseen = atol(buf);
	for (i=0; buf[i]; ++i) if (!isdigit(buf[i])) is_textual_seen = 1;
	if (is_textual_seen)	strcpy(vbuf.v_seen, buf);

	client_getln(vbuf.v_answered, sizeof vbuf.v_answered);
	client_getln(buf, sizeof buf);	vbuf.v_flags = atoi(buf);
	client_getln(buf, sizeof buf);	vbuf.v_view = atoi(buf);
	put_visit(&vbuf);
	CtdlLogPrintf(CTDL_INFO, "Imported visit %ld/%ld/%ld\n",
		vbuf.v_roomnum, vbuf.v_roomgen, vbuf.v_usernum);
}


void artv_import_openid(void) {
	char buf[SIZ];
	long usernum;
	char openid[1024];
	char *data;
	int data_len;

	client_getln(openid, sizeof openid);
	client_getln(buf, sizeof buf);	usernum = atol(buf);
	if (IsEmptyStr(openid)) return;

	data_len = sizeof(long) + strlen(openid) + 1;
	data = malloc(data_len);

	memcpy(data, &usernum, sizeof(long));
	memcpy(&data[sizeof(long)], openid, strlen(openid) + 1);

	cdb_store(CDB_OPENID, openid, strlen(openid), data, data_len);
	free(data);

	CtdlLogPrintf(CTDL_INFO, "Imported OpenID %s for user #%ld\n", openid, usernum);
}


void artv_import_message(long *iterations, char **b64buf, size_t *b64size, char **plain, size_t *plain_size) {
	struct MetaData smi;
	long msgnum;
	long msglen;
	char buf[SIZ];
	size_t b64len = 0;
	char *tbuf, *tbuf2;
	size_t mlen;
	
	memset(&smi, 0, sizeof(struct MetaData));
	client_getln(buf, sizeof buf);	msgnum = atol(buf);
				smi.meta_msgnum = msgnum;
	client_getln(buf, sizeof buf);	smi.meta_refcount = atoi(buf);
	client_getln(smi.meta_content_type, sizeof smi.meta_content_type);

	CtdlLogPrintf(CTDL_INFO, "message #%ld\n", msgnum);

	/* decode base64 message text */
	while (client_getln(buf, sizeof buf) >= 0 && strcasecmp(buf, END_OF_MESSAGE)) {
		if (CtdlThreadCheckStop())
			return;
			
		cprintdot(iterations);
		
		/**
		 * Grow the buffers if we need to
		 */
		mlen = strlen (buf);
		if (b64len + mlen > *b64size)
		{
			tbuf = realloc (*b64buf, *b64size + SIZ);
			tbuf2 = realloc (*plain, *plain_size + SIZ);
			if (tbuf && tbuf2)
			{
				*b64buf = tbuf;
				*plain = tbuf2;
				*b64size += SIZ;
				*plain_size += SIZ;
			}
			else
			{
				CtdlLogPrintf(CTDL_DEBUG, "ARTV import: realloc() failed.\n");
				cprintf("\nMemory allocation failure.\n");
				return;
			}
		}
		strcat (*b64buf, buf);
		b64len += mlen;
	}
	
	/**
	 * FIXME: This is an ideal place for a "sub thread". What we should do is create a new thread
	 * This new thread would be given the base64 encoded message, it would then decode it and store
	 * it. This would allow the thread that is reading from the client to continue doing so, basically
	 * backgrounding the decode and store operation. This would increase the speed of the import from
	 * the users perspective.
	 */
	
	/**
	 * Decode and store the message
	 * If this decode and store takes more than 5 seconds the sendcommand WD timer may expire.
	 */
	msglen = CtdlDecodeBase64(*plain, *b64buf, b64len);
	CtdlLogPrintf(CTDL_DEBUG, "msglen = %ld\n", msglen);
	cdb_store(CDB_MSGMAIN, &msgnum, sizeof(long), *plain, msglen);
	PutMetaData(&smi);
	CtdlLogPrintf(CTDL_INFO, "Imported message %ld\n", msgnum);
	
}




void artv_do_import(void) {
	char buf[SIZ];
	char abuf[SIZ];
	char s_version[SIZ];
	int version;
	long iterations;
	char *b64mes = NULL;
	char *plain = NULL;
	size_t b64size, plain_size;
	
	unbuffer_output();

	/* Prepare buffers for base 64 decoding of messages.
	*/
	b64mes = malloc(SIZ);
	if (b64mes == NULL)
	{
		cprintf("%d Malloc failed in import/export.\n",
			ERROR + RESOURCE_BUSY);
		return;
	}
	b64mes[0] = 0;
	b64size=SIZ;
	plain = malloc(SIZ);
	if (plain == NULL)
	{
		cprintf("%d Malloc failed in import/export.\n",
			ERROR + RESOURCE_BUSY);
		free(b64mes);
		return;
	}
	plain[0] = 0;
	plain_size = SIZ;
	
	CC->dont_term = 1;

	cprintf("%d sock it to me\n", SEND_LISTING);
	abuf[0] = '\0';
	unbuffer_output();
	iterations = 0;
	while (client_getln(buf, sizeof buf) >= 0 && strcmp(buf, "000")) {
		if (CtdlThreadCheckStop())
			break;	// Should we break or return?
		
		if (buf[0] == '\0')
			continue;
			
		CtdlLogPrintf(CTDL_DEBUG, "import keyword: <%s>\n", buf);
		if ((abuf[0] == '\0') || (strcasecmp(buf, abuf))) {
			cprintf ("\n\nImporting datatype %s\n", buf);
			strncpy (abuf, buf, SIZ);	
			iterations = 0;
		}
		else {
			cprintdot(&iterations);
		}
		
		if (!strcasecmp(buf, "version")) {
			client_getln(s_version, sizeof s_version);
			version = atoi(s_version);
			if ((version<EXPORT_REV_MIN) || (version>REV_LEVEL)) {
				CtdlLogPrintf(CTDL_ERR, "Version mismatch in ARTV import; aborting\n");
				break;
			}
		}
		else if (!strcasecmp(buf, "config")) artv_import_config();
		else if (!strcasecmp(buf, "control")) artv_import_control();
		else if (!strcasecmp(buf, "user")) artv_import_user();
		else if (!strcasecmp(buf, "room")) artv_import_room(&iterations);
		else if (!strcasecmp(buf, "floor")) artv_import_floor();
		else if (!strcasecmp(buf, "visit")) artv_import_visit();
		else if (!strcasecmp(buf, "openid")) artv_import_openid();
		else if (!strcasecmp(buf, "message"))
		{
			b64mes[0] = 0;
			plain[0] = 0;
			artv_import_message(&iterations, &b64mes, &b64size, &plain, &plain_size);
		}
		else break;
	}
	free (b64mes);
	free (plain);
	
	CtdlLogPrintf(CTDL_INFO, "Invalid keyword <%s>.  Flushing input.\n", buf);
	while (client_getln(buf, sizeof buf) >= 0 && strcmp(buf, "000"))  ;;
	rebuild_euid_index();
	rebuild_usersbynumber();
	CC->dont_term = 0;
}



void cmd_artv(char *cmdbuf) {
	char cmd[32];
	
	if (CtdlAccessCheck(ac_internal)) return;
	
	if (CtdlTrySingleUser())
	{
		CtdlMakeTempFileName(artv_tempfilename1, sizeof artv_tempfilename1);
		CtdlMakeTempFileName(artv_tempfilename2, sizeof artv_tempfilename2);

		extract_token(cmd, cmdbuf, 0, '|', sizeof cmd);
		if (!strcasecmp(cmd, "export")) artv_do_export();
		else if (!strcasecmp(cmd, "import")) artv_do_import();
		else if (!strcasecmp(cmd, "dump")) artv_do_dump();
		else cprintf("%d illegal command\n", ERROR + ILLEGAL_VALUE);

		unlink(artv_tempfilename1);
		unlink(artv_tempfilename2);
		
		CtdlEndSingleUser();
	}
	else
	{
		cprintf("%d The importer/exporter is already running.\n",
			ERROR + RESOURCE_BUSY);
	}
}




CTDL_MODULE_INIT(vandelay)
{
	if (!threading)
	{
		CtdlRegisterProtoHook(cmd_artv, "ARTV", "import/export data store");
	}
	
	/* return our Subversion id for the Log */
	return "$Id$";
}
