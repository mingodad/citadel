/*
 * $Id$
 *
 * Transparently handle the upgrading of server data formats.
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
#include <limits.h>
#include "citadel.h"
#include "server.h"
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "control.h"
#include "dynloader.h"
#include "database.h"
#include "room_ops.h"
#include "user_ops.h"
#include "msgbase.h"
#include "tools.h"
#include "serv_upgrade.h"

void do_pre555_usersupp_upgrade(void) {
        struct pre555usersupp usbuf;
	struct usersupp newus;
        struct cdbdata *cdbus;
	char tempfilename[PATH_MAX];
	FILE *fp, *tp;
	static char vcard[1024];

	lprintf(5, "Upgrading user file\n");
	fp = tmpfile();
	if (fp == NULL) {
		lprintf(1, "%s\n", strerror(errno));
		exit(errno);
	}
	strcpy(tempfilename, tmpnam(NULL));

	/* First, back out all old version records to a flat file */
        cdb_rewind(CDB_USERSUPP);
        while(cdbus = cdb_next_item(CDB_USERSUPP), cdbus != NULL) {
                memset(&usbuf, 0, sizeof(struct pre555usersupp));
                memcpy(&usbuf, cdbus->ptr,
                       	( (cdbus->len > sizeof(struct pre555usersupp)) ?
                       	sizeof(struct pre555usersupp) : cdbus->len) );
                cdb_free(cdbus);
		fwrite(&usbuf, sizeof(struct pre555usersupp), 1, fp);
	}

	/* ...and overwrite the records with new format records */
	rewind(fp);
	while (fread(&usbuf, sizeof(struct pre555usersupp), 1, fp) > 0) {
	    if (strlen(usbuf.fullname) > 0) {
		lprintf(9, "Upgrading <%s>\n", usbuf.fullname);
		memset(&newus, 0, sizeof(struct usersupp));

		newus.uid = usbuf.USuid;
		strcpy(newus.password, usbuf.password);
		newus.flags = usbuf.flags;
		newus.timescalled = (long) usbuf.timescalled;
		newus.posted = (long) usbuf.posted;
		newus.axlevel = (CIT_UBYTE) usbuf.axlevel;
		newus.usernum = (long) usbuf.usernum;
		newus.lastcall = (long) usbuf.lastcall;
		newus.USuserpurge = (int) usbuf.USuserpurge;
		strcpy(newus.fullname, usbuf.fullname);
		newus.USscreenwidth = (CIT_UBYTE) usbuf.USscreenwidth;
		newus.USscreenheight = (CIT_UBYTE) usbuf.USscreenheight;

		putuser(&newus);

		/* write the vcard */
		snprintf(vcard, sizeof vcard,
			"Content-type: text/x-vcard\n\n"
			"begin:vcard\n"
			"n:%s\n"
			"tel;home:%s\n"
			"email;internet:%s\n"
			"adr:;;%s;%s;%s;%s;USA\n"
			"end:vcard\n",
			usbuf.USname,
			usbuf.USphone,
			usbuf.USemail,
			usbuf.USaddr,
			usbuf.UScity,
			usbuf.USstate,
			usbuf.USzip);

		tp = fopen(tempfilename, "w");
		fwrite(vcard, strlen(vcard)+1, 1, tp);
		fclose(tp);

        	CtdlWriteObject(USERCONFIGROOM, "text/x-vcard",
			tempfilename, &newus, 0, 1, CM_SKIP_HOOKS);
		unlink(tempfilename);
	    }
	}

	fclose(fp);	/* this file deletes automatically */
}








void check_server_upgrades(void) {

	get_control();
	lprintf(5, "Server-hosted upgrade level is %d.%02d\n",
		(CitControl.version / 100),
		(CitControl.version % 100) );

	if (CitControl.version < REV_LEVEL) {
		lprintf(5, "Server hosted updates need to be processed at "
				"this time.  Please wait...\n");
	}
	else {
		return;
	}

	if (CitControl.version < 555) do_pre555_usersupp_upgrade();

	CitControl.version = REV_LEVEL;
	put_control();
}











/* 
 * Back end processing function for cmd_bmbx
 */
void cmd_bmbx_backend(struct quickroom *qrbuf, void *data) {
	static struct RoomProcList *rplist = NULL;
	struct RoomProcList *ptr;
	struct quickroom qr;

	/* Lazy programming here.  Call this function as a ForEachRoom backend
	 * in order to queue up the room names, or call it with a null room
	 * to make it do the processing.
	 */
	if (qrbuf != NULL) {
		ptr = (struct RoomProcList *)
			mallok(sizeof (struct RoomProcList));
		if (ptr == NULL) return;

		safestrncpy(ptr->name, qrbuf->QRname, sizeof ptr->name);
		ptr->next = rplist;
		rplist = ptr;
		return;
	}

	while (rplist != NULL) {

		if (lgetroom(&qr, rplist->name) == 0) {
			lprintf(9, "Processing <%s>...\n", rplist->name);
			if ( (qr.QRflags & QR_MAILBOX) == 0) {
				lprintf(9, "  -- not a mailbox\n");
			}
			else {

				qr.QRgen = time(NULL);
				lprintf(9, "  -- bumped!\n");
			}
			lputroom(&qr);
		}

		ptr = rplist;
		rplist = rplist->next;
		phree(ptr);
	}
}

/*
 * quick fix command to bump mailbox generation numbers
 */
void cmd_bmbx(char *argbuf) {
	int really_do_this  = 0;

	if (CtdlAccessCheck(ac_internal)) return;
	really_do_this = extract_int(argbuf, 0);

	if (really_do_this != 1) {
		cprintf("%d You didn't really want to do that.\n", OK);
		return;
	}

	ForEachRoom(cmd_bmbx_backend, NULL);
	cmd_bmbx_backend(NULL, NULL);

	cprintf("%d Mailbox generation numbers bumped.\n", OK);
	return;

}













char *Dynamic_Module_Init(void)
{
	check_server_upgrades();
	CtdlRegisterProtoHook(cmd_bmbx, "BMBX", "Bump mailboxes");
	return "$Id$";
}
