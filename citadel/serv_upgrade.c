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
#include "serv_extensions.h"
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
		newus.axlevel = (cit_uint8_t) usbuf.axlevel;
		newus.usernum = (long) usbuf.usernum;
		newus.lastcall = (long) usbuf.lastcall;
		newus.USuserpurge = (int) usbuf.USuserpurge;
		strcpy(newus.fullname, usbuf.fullname);
		newus.USscreenwidth = (cit_uint8_t) usbuf.USscreenwidth;
		newus.USscreenheight = (cit_uint8_t) usbuf.USscreenheight;

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
				lprintf(9, "  -- fixed!\n");
			}
			lputroom(&qr);
		}

		ptr = rplist;
		rplist = rplist->next;
		phree(ptr);
	}
}

/*
 * quick fix to bump mailbox generation numbers
 */
void bump_mailbox_generation_numbers(void) {
	lprintf(5, "Applying security fix to mailbox rooms\n");
	ForEachRoom(cmd_bmbx_backend, NULL);
	cmd_bmbx_backend(NULL, NULL);
	return;
}


/* 
 * Back end processing function for convert_bbsuid_to_minusone()
 */
void cbtm_backend(struct usersupp *usbuf, void *data) {
	static struct UserProcList *uplist = NULL;
	struct UserProcList *ptr;
	struct usersupp us;

	/* Lazy programming here.  Call this function as a ForEachUser backend
	 * in order to queue up the room names, or call it with a null user
	 * to make it do the processing.
	 */
	if (usbuf != NULL) {
		ptr = (struct UserProcList *)
			mallok(sizeof (struct UserProcList));
		if (ptr == NULL) return;

		safestrncpy(ptr->user, usbuf->fullname, sizeof ptr->user);
		ptr->next = uplist;
		uplist = ptr;
		return;
	}

	while (uplist != NULL) {

		if (lgetuser(&us, uplist->user) == 0) {
			lprintf(9, "Processing <%s>...\n", uplist->user);
			if (us.uid == BBSUID) {
				us.uid = (-1);
			}
			lputuser(&us);
		}

		ptr = uplist;
		uplist = uplist->next;
		phree(ptr);
	}
}

/*
 * quick fix to change all BBSUID users to (-1)
 */
void convert_bbsuid_to_minusone(void) {
	lprintf(5, "Applying uid changes\n");
	ForEachUser(cbtm_backend, NULL);
	cbtm_backend(NULL, NULL);
	return;
}


/*
 * This field was originally used for something else, so when we upgrade
 * we have to initialize it to 0 in case there was trash in that space.
 */
void initialize_c_rfc822_strict_from(void) {
	get_config();
	config.c_rfc822_strict_from = 0;
	put_config();
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
	if (CitControl.version < 591) bump_mailbox_generation_numbers();
	if (CitControl.version < 606) initialize_c_rfc822_strict_from();
	if (CitControl.version < 608) convert_bbsuid_to_minusone();

	CitControl.version = REV_LEVEL;
	put_control();
}











char *serv_upgrade_init(void)
{
	check_server_upgrades();
	return "$Id$";
}
