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
#include <sys/time.h>
#include <sys/wait.h>
#include <string.h>
#include <limits.h>
#include "citadel.h"
#include "server.h"
#include <time.h>
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "dynloader.h"
#include "database.h"
#include "msgbase.h"
#include "tools.h"
#include "user_ops.h"
#include "room_ops.h"
#include "control.h"

void artv_export_users_backend(struct usersupp *usbuf, void *data) {
	cprintf("user\n");
	cprintf("%d\n", usbuf->version);
	cprintf("%d\n", usbuf->uid);
	cprintf("%s\n", usbuf->password);
	cprintf("%u\n", usbuf->flags);
	cprintf("%ld\n", usbuf->timescalled);
	cprintf("%ld\n", usbuf->posted);
	cprintf("%d\n", usbuf->axlevel);
	cprintf("%ld\n", usbuf->usernum);
	cprintf("%ld\n", usbuf->lastcall);
	cprintf("%d\n", usbuf->USuserpurge);
	cprintf("%s\n", usbuf->fullname);
	cprintf("%d\n", usbuf->USscreenwidth);
	cprintf("%d\n", usbuf->USscreenheight);
	cprintf("%d\n", usbuf->moderation_filter);
}


void artv_export_users(void) {
	ForEachUser(artv_export_users_backend, NULL);
}


void artv_export_room_msg(long msgnum) {
	cprintf("%ld\n", msgnum);
}


void artv_export_rooms_backend(struct quickroom *qrbuf, void *data) {
	cprintf("room\n");
	cprintf("%s\n", qrbuf->QRname);
	cprintf("%s\n", qrbuf->QRpasswd);
	cprintf("%ld\n", qrbuf->QRroomaide);
	cprintf("%ld\n", qrbuf->QRhighest);
	cprintf("%ld\n", qrbuf->QRgen);
	cprintf("%u\n", qrbuf->QRflags);
	cprintf("%s\n", qrbuf->QRdirname);
	cprintf("%ld\n", qrbuf->QRinfo);
	cprintf("%d\n", qrbuf->QRfloor);
	cprintf("%ld\n", qrbuf->QRmtime);
	cprintf("%d\n", qrbuf->QRep.expire_mode);
	cprintf("%d\n", qrbuf->QRep.expire_value);
	cprintf("%ld\n", qrbuf->QRnumber);
	cprintf("%d\n", qrbuf->QRorder);

	getroom(&CC->quickroom, qrbuf->QRname);
	/* format of message list export is all message numbers output
	 * one per line terminated by a 0.
	 */
	CtdlForEachMessage(MSGS_ALL, 0L, (-127), NULL, NULL,
		artv_export_room_msg);
	cprintf("0\n");

}



void artv_export_rooms(void) {
	ForEachRoom(artv_export_rooms_backend, NULL);
}


void artv_export_floors(void) {
        struct floor flbuf;
        int i;

        for (i=0; i < MAXFLOORS; ++i) {
		cprintf("floor\n");
		cprintf("%d\n", i);
                getfloor(&flbuf, i);
		cprintf("%u\n", flbuf.f_flags);
		cprintf("%s\n", flbuf.f_name);
		cprintf("%d\n", flbuf.f_ref_count);
		cprintf("%d\n", flbuf.f_ep.expire_mode);
		cprintf("%d\n", flbuf.f_ep.expire_value);
	}
}





/* 
 *  Traverse the room file...
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

		cprintf("visit\n");
		cprintf("%ld\n", vbuf.v_roomnum);
		cprintf("%ld\n", vbuf.v_roomgen);
		cprintf("%ld\n", vbuf.v_usernum);
		cprintf("%ld\n", vbuf.v_lastseen);
		cprintf("%u\n", vbuf.v_flags);
	}
}









void artv_do_export(void) {
	cprintf("%d Yikes.\n", LISTING_FOLLOWS);

	/* export the config file */
	cprintf("config\n");
	cprintf("%s\n", config.c_nodename);
	cprintf("%s\n", config.c_fqdn);
	cprintf("%s\n", config.c_humannode);
	cprintf("%s\n", config.c_phonenum);
	cprintf("%d\n", config.c_bbsuid);
	cprintf("%d\n", config.c_creataide);
	cprintf("%d\n", config.c_sleeping);
	cprintf("%d\n", config.c_initax);
	cprintf("%d\n", config.c_regiscall);
	cprintf("%d\n", config.c_twitdetect);
	cprintf("%s\n", config.c_twitroom);
	cprintf("%s\n", config.c_moreprompt);
	cprintf("%d\n", config.c_restrict);
	cprintf("%ld\n", config.c_msgbase);
	cprintf("%s\n", config.c_bbs_city);
	cprintf("%s\n", config.c_sysadm);
	cprintf("%s\n", config.c_bucket_dir);
	cprintf("%d\n", config.c_setup_level);
	cprintf("%d\n", config.c_maxsessions);
	cprintf("%s\n", config.c_net_password);
	cprintf("%d\n", config.c_port_number);
	cprintf("%d\n", config.c_ipgm_secret);
	cprintf("%d\n", config.c_ep.expire_mode);
	cprintf("%d\n", config.c_ep.expire_value);
	cprintf("%d\n", config.c_userpurge);
	cprintf("%d\n", config.c_roompurge);
	cprintf("%s\n", config.c_logpages);
	cprintf("%d\n", config.c_createax);
	cprintf("%ld\n", config.c_maxmsglen);
	cprintf("%d\n", config.c_min_workers);
	cprintf("%d\n", config.c_max_workers);
	cprintf("%d\n", config.c_pop3_port);
	cprintf("%d\n", config.c_smtp_port);
	cprintf("%d\n", config.c_default_filter);

	/* Export the control file */
	get_control();
	cprintf("control\n");
	cprintf("%ld\n", CitControl.MMhighest);
	cprintf("%u\n", CitControl.MMflags);
	cprintf("%ld\n", CitControl.MMnextuser);
	cprintf("%ld\n", CitControl.MMnextroom);
	cprintf("%d\n", CitControl.version);

	artv_export_users();
	artv_export_rooms();
	artv_export_floors();
	artv_export_visits();

	cprintf("000\n");
}




void artv_do_import(void) {
	cprintf("%d command not yet implemented\n", ERROR);

}



void cmd_artv(char *cmdbuf) {
	char cmd[256];

	if (CtdlAccessCheck(ac_aide)) return;	/* FIXME should be intpgm */

	extract(cmd, cmdbuf, 0);
	if (!strcasecmp(cmd, "export")) artv_do_export();
	else if (!strcasecmp(cmd, "import")) artv_do_import();
	else cprintf("%d illegal command\n", ERROR);
}




char *Dynamic_Module_Init(void)
{
	CtdlRegisterProtoHook(cmd_artv, "ARTV", "import/export data store");
	return "$Id$";
}
