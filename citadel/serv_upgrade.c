/* $Id$ */
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
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif
#include "citadel.h"
#include "server.h"
#include <syslog.h>
#include <time.h>
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "control.h"
#include "dynloader.h"
#include "database.h"
#include "user_ops.h"
#include "msgbase.h"
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
		sprintf(vcard,
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

        	CtdlWriteObject(CONFIGROOM, "text/x-vcard",
			tempfilename, &newus, 0, 1);    
		unlink(tempfilename);

	}

	fclose(fp);	/* this file deletes automatically */
}








void check_server_upgrades(void) {

	get_control();
	lprintf(5, "Server-hosted upgrade level is %d.%02d\n",
		(CitControl.version / 100),
		(CitControl.version % 100) );

	if (CitControl.version < config.c_setup_level) {
		lprintf(5, "Server hosted updates need to be processed at "
				"this time.  Please wait...\n");
	}
	else {
		return;
	}


	if (CitControl.version < 555) do_pre555_usersupp_upgrade();


	CitControl.version = config.c_setup_level;
	put_control();
}




char *Dynamic_Module_Init(void)
{
	check_server_upgrades();
	return "$Id$";
}
