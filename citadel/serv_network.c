/*
 * $Id$ 
 *
 * This module will eventually replace netproc and some of its utilities.  In
 * the meantime, it serves as a mailing list manager.
 *
 * Copyright (C) 2000-2001 by Art Cancro and others.
 * This code is released under the terms of the GNU General Public License.
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
#include "dynloader.h"
#include "room_ops.h"
#include "user_ops.h"
#include "policy.h"
#include "database.h"
#include "msgbase.h"
#include "tools.h"
#include "internet_addressing.h"
#include "serv_network.h"


/*
 * When we do network processing, it's accomplished in two passes; one to
 * gather a list of rooms and one to actually do them.  It's ok that rplist
 * is global; this process *only* runs as part of the housekeeping loop and
 * therefore only one will run at a time.
 */
struct RoomProcList {
        struct RoomProcList *next;
        char name[ROOMNAMELEN];
};

struct RoomProcList *rplist = NULL;




void cmd_gnet(char *argbuf) {
	char filename[SIZ];
	char buf[SIZ];
	FILE *fp;

	if (CtdlAccessCheck(ac_room_aide)) return;
	assoc_file_name(filename, &CC->quickroom, "netconfigs");
	cprintf("%d Network settings for room #%ld <%s>\n",
		LISTING_FOLLOWS,
		CC->quickroom.QRnumber, CC->quickroom.QRname);

	fp = fopen(filename, "r");
	if (fp != NULL) {
		while (fgets(buf, sizeof buf, fp) != NULL) {
			buf[strlen(buf)-1] = 0;
			cprintf("%s\n", buf);
		}
		fclose(fp);
	}

	cprintf("000\n");
}


void cmd_snet(char *argbuf) {
	char tempfilename[SIZ];
	char filename[SIZ];
	char buf[SIZ];
	FILE *fp;

	if (CtdlAccessCheck(ac_room_aide)) return;
	safestrncpy(tempfilename, tmpnam(NULL), sizeof tempfilename);
	assoc_file_name(filename, &CC->quickroom, "netconfigs");

	fp = fopen(tempfilename, "w");
	if (fp == NULL) {
		cprintf("%d Cannot open %s: %s\n",
			ERROR+INTERNAL_ERROR,
			tempfilename,
			strerror(errno));
	}

	cprintf("%d %s\n", SEND_LISTING, tempfilename);
	while (client_gets(buf), strcmp(buf, "000")) {
		fprintf(fp, "%s\n", buf);
	}
	fclose(fp);

	/* Now copy the temp file to its permanent location
	 * (We use /bin/mv instead of link() because they may be on
	 * different filesystems)
	 */
	unlink(filename);
	snprintf(buf, sizeof buf, "/bin/mv %s %s", tempfilename, filename);
	system(buf);
}



/*
 * Spools out one message from the list.
 */
void network_spool_msg(long msgnum, void *userdata) {
	struct SpoolControl *sc;
	struct namelist *nptr;
	int err;
	char *instr = NULL;
	char *newpath = NULL;
	size_t instr_len = SIZ;
	struct CtdlMessage *msg;
	struct CtdlMessage *imsg;
	struct ser_ret sermsg;
	FILE *fp;
	char filename[SIZ];

	sc = (struct SpoolControl *)userdata;

	/*
	 * Process mailing list recipients
	 */
	if (sc->listrecps != NULL) {
	
		/* First, copy it to the spoolout room */
		err = CtdlSaveMsgPointerInRoom(SMTP_SPOOLOUT_ROOM, msgnum, 0);
		if (err != 0) return;

		/* 
		 * Figure out how big a buffer we need to allocate
	 	 */
		for (nptr = sc->listrecps; nptr != NULL; nptr = nptr->next) {
			instr_len = instr_len + strlen(nptr->name);
		}
	
		/*
	 	 * allocate...
	 	 */
		lprintf(9, "Generating delivery instructions\n");
		instr = mallok(instr_len);
		if (instr == NULL) {
			lprintf(1, "Cannot allocate %d bytes for instr...\n",
				instr_len);
			abort();
		}
		sprintf(instr,
			"Content-type: %s\n\nmsgid|%ld\nsubmitted|%ld\n"
			"bounceto|postmaster@%s\n" ,
			SPOOLMIME, msgnum, time(NULL), config.c_fqdn );
	
		/* Generate delivery instructions for each recipient */
		for (nptr = sc->listrecps; nptr != NULL; nptr = nptr->next) {
			sprintf(&instr[strlen(instr)], "remote|%s|0||\n",
				nptr->name);
		}
	
		/*
	 	 * Generate a message from the instructions
	 	 */
       		imsg = mallok(sizeof(struct CtdlMessage));
		memset(imsg, 0, sizeof(struct CtdlMessage));
		imsg->cm_magic = CTDLMESSAGE_MAGIC;
		imsg->cm_anon_type = MES_NORMAL;
		imsg->cm_format_type = FMT_RFC822;
		imsg->cm_fields['A'] = strdoop("Citadel");
		imsg->cm_fields['M'] = instr;
	
		/* Save delivery instructions in spoolout room */
		CtdlSaveMsg(imsg, "", SMTP_SPOOLOUT_ROOM, MES_LOCAL);
		CtdlFreeMessage(imsg);
	}
	
	/*
	 * Process IGnet push shares
	 */
	if (sc->ignet_push_shares != NULL) {
	
		msg = CtdlFetchMessage(msgnum);
		if (msg != NULL) {

			/* Prepend our node name to the Path field whenever
			 * sending a message to another IGnet node
			 */
			if (msg->cm_fields['P'] == NULL) {
				msg->cm_fields['P'] = strdoop("username");
			}
			newpath = mallok(strlen(msg->cm_fields['P']) + 
					strlen(config.c_nodename) + 2);
			sprintf(newpath, "%s!%s", config.c_nodename,
					msg->cm_fields['P']);
			phree(msg->cm_fields['P']);
			msg->cm_fields['P'] = newpath;

			/*
			 * Force the message to appear in the correct room
			 * on the far end by setting the C field correctly
			 */
			if (msg->cm_fields['C'] != NULL) {
				phree(msg->cm_fields['C']);
			}
			msg->cm_fields['C'] = strdoop(CC->quickroom.QRname);

			/* 
			 * Now serialize it for transmission
			 */
			serialize_message(&sermsg, msg);
			CtdlFreeMessage(msg);

			/* Now send it to every node */
			for (nptr = sc->ignet_push_shares; nptr != NULL;
			    nptr = nptr->next) {

				/* FIXME check for valid node name */
				/* FIXME check for split horizon */

				/* Send the message */
				sprintf(filename, "./network/spoolout/%s",
					nptr->name);
				fp = fopen(filename, "ab");
				if (fp != NULL) {
					fwrite(sermsg.ser, sermsg.len, 1, fp);
					fclose(fp);
				}
			}


		}

	}

	/* update lastsent */
	sc->lastsent = msgnum;
}
	



/*
 * Batch up and send all outbound traffic from the current room
 */
void network_spoolout_room(char *room_to_spool) {
	char filename[SIZ];
	char buf[SIZ];
	char instr[SIZ];
	FILE *fp;
	struct SpoolControl sc;
	/* struct namelist *digestrecps = NULL; */
	struct namelist *nptr;

	lprintf(7, "Spooling <%s>\n", room_to_spool);
	if (getroom(&CC->quickroom, room_to_spool) != 0) {
		lprintf(1, "ERROR: cannot load <%s>\n", room_to_spool);
		return;
	}

	memset(&sc, 0, sizeof(struct SpoolControl));
	assoc_file_name(filename, &CC->quickroom, "netconfigs");

	fp = fopen(filename, "r");
	if (fp == NULL) {
		lprintf(7, "Outbound batch processing skipped for <%s>\n",
			CC->quickroom.QRname);
		return;
	}

	lprintf(5, "Outbound batch processing started for <%s>\n",
		CC->quickroom.QRname);

	while (fgets(buf, sizeof buf, fp) != NULL) {
		buf[strlen(buf)-1] = 0;

		extract(instr, buf, 0);
		if (!strcasecmp(instr, "lastsent")) {
			sc.lastsent = extract_long(buf, 1);
		}
		else if (!strcasecmp(instr, "listrecp")) {
			nptr = (struct namelist *)
				mallok(sizeof(struct namelist));
			nptr->next = sc.listrecps;
			extract(nptr->name, buf, 1);
			sc.listrecps = nptr;
		}
		else if (!strcasecmp(instr, "ignet_push_share")) {
			nptr = (struct namelist *)
				mallok(sizeof(struct namelist));
			nptr->next = sc.ignet_push_shares;
			extract(nptr->name, buf, 1);
			sc.ignet_push_shares = nptr;
		}


	}
	fclose(fp);


	/* Do something useful */
	CtdlForEachMessage(MSGS_GT, sc.lastsent, (-63), NULL, NULL,
		network_spool_msg, &sc);


	/* Now rewrite the config file */
	fp = fopen(filename, "w");
	if (fp == NULL) {
		lprintf(1, "ERROR: cannot open %s: %s\n",
			filename, strerror(errno));
	}
	else {
		fprintf(fp, "lastsent|%ld\n", sc.lastsent);

		/* Write out the listrecps while freeing from memory at the
		 * same time.  Am I clever or what?  :)
		 */
		while (sc.listrecps != NULL) {
			fprintf(fp, "listrecp|%s\n", sc.listrecps->name);
			nptr = sc.listrecps->next;
			phree(sc.listrecps);
			sc.listrecps = nptr;
		}
		while (sc.ignet_push_shares != NULL) {
			fprintf(fp, "ignet_push_share|%s\n",
				sc.ignet_push_shares->name);
			nptr = sc.ignet_push_shares->next;
			phree(sc.ignet_push_shares);
			sc.ignet_push_shares = nptr;
		}

		fclose(fp);
	}

	lprintf(5, "Outbound batch processing finished for <%s>\n",
		CC->quickroom.QRname);
}


/*
 * Batch up and send all outbound traffic from the current room
 */
void network_queue_room(struct quickroom *qrbuf, void *data) {
	struct RoomProcList *ptr;

	ptr = (struct RoomProcList *) mallok(sizeof (struct RoomProcList));
	if (ptr == NULL) return;

	safestrncpy(ptr->name, qrbuf->QRname, sizeof ptr->name);
	ptr->next = rplist;
	rplist = ptr;
}
	

/*
 * network_do_queue()
 * 
 * Run through the rooms doing various types of network stuff.
 */
void network_do_queue(void) {
	static int doing_queue = 0;
	static time_t last_run = 0L;
	struct RoomProcList *ptr;

#define NETWORK_QUEUE_FREQUENCY 3600	/* one hour ... FIXME put in config */
	/*
	 * Run no more frequently than once every n seconds
	 */
	if ( (time(NULL) - last_run) < NETWORK_QUEUE_FREQUENCY ) return;

	/*
	 * This is a simple concurrency check to make sure only one queue run
	 * is done at a time.  We could do this with a mutex, but since we
	 * don't really require extremely fine granularity here, we'll do it
	 * with a static variable instead.
	 */
	if (doing_queue) return;
	doing_queue = 1;
	last_run = time(NULL);

	/* 
	 * Go ahead and run the queue
	 */
	lprintf(7, "network: loading outbound queue\n");
	ForEachRoom(network_queue_room, NULL);

	lprintf(7, "network: running outbound queue\n");
	while (rplist != NULL) {
		network_spoolout_room(rplist->name);
		ptr = rplist;
		rplist = rplist->next;
		phree(ptr);
	}

	lprintf(7, "network: queue run completed\n");
	doing_queue = 0;
}


/*
 * Module entry point
 */
char *Dynamic_Module_Init(void)
{
	CtdlRegisterProtoHook(cmd_gnet, "GNET", "Get network config");
	CtdlRegisterProtoHook(cmd_snet, "SNET", "Get network config");
	CtdlRegisterSessionHook(network_do_queue, EVT_TIMER);
	return "$Id$";
}
