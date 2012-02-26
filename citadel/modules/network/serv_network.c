/*
 * This module handles shared rooms, inter-Citadel mail, and outbound
 * mailing list processing.
 *
 * Copyright (c) 2000-2011 by the citadel.org team
 *
 *  This program is open source software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * ** NOTE **   A word on the S_NETCONFIGS semaphore:
 * This is a fairly high-level type of critical section.  It ensures that no
 * two threads work on the netconfigs files at the same time.  Since we do
 * so many things inside these, here are the rules:
 *  1. begin_critical_section(S_NETCONFIGS) *before* begin_ any others.
 *  2. Do *not* perform any I/O with the client during these sections.
 *
 */

/*
 * Duration of time (in seconds) after which pending list subscribe/unsubscribe
 * requests that have not been confirmed will be deleted.
 */
#define EXP	259200	/* three days */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <pwd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
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
#ifdef HAVE_SYSCALL_H
# include <syscall.h>
#else 
# if HAVE_SYS_SYSCALL_H
#  include <sys/syscall.h>
# endif
#endif

#include <sys/wait.h>
#include <string.h>
#include <limits.h>
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "user_ops.h"
#include "database.h"
#include "msgbase.h"
#include "internet_addressing.h"
#include "serv_network.h"
#include "clientsocket.h"
#include "file_ops.h"
#include "citadel_dirs.h"
#include "threads.h"

#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif

#include "context.h"
#include "netconfig.h"
#include "netspool.h"
#include "netmail.h"
#include "ctdl_module.h"

/* comes from lookup3.c from libcitadel... */
extern uint32_t hashlittle( const void *key, size_t length, uint32_t initval);

typedef struct __roomlists {
	RoomProcList *rplist;
	HashList *RoomsInterestedIn;
}roomlists;
/*
 * When we do network processing, it's accomplished in two passes; one to
 * gather a list of rooms and one to actually do them.  It's ok that rplist
 * is global; we have a mutex that keeps it safe.
 */
struct RoomProcList *rplist = NULL;

int GetNetworkedRoomNumbers(const char *DirName, HashList *DirList)
{
	DIR *filedir = NULL;
	struct dirent *d;
	struct dirent *filedir_entry;
	long RoomNR;
	long Count = 0;
		
	filedir = opendir (DirName);
	if (filedir == NULL) {
		return 0;
	}

	d = (struct dirent *)malloc(offsetof(struct dirent, d_name) + PATH_MAX + 1);
	if (d == NULL) {
		return 0;
	}

	while ((readdir_r(filedir, d, &filedir_entry) == 0) &&
	       (filedir_entry != NULL))
	{
		RoomNR = atol(filedir_entry->d_name);
		if (RoomNR != 0) {
			Count++;
			Put(DirList, LKEY(RoomNR), &Count, reference_free_handler);
		}
	}
	free(d);
	closedir(filedir);
	return Count;
}




/*
 * Check the use table.  This is a list of messages which have recently
 * arrived on the system.  It is maintained and queried to prevent the same
 * message from being entered into the database multiple times if it happens
 * to arrive multiple times by accident.
 */
int network_usetable(struct CtdlMessage *msg) {

	char msgid[SIZ];
	struct cdbdata *cdbut;
	struct UseTable ut;

	/* Bail out if we can't generate a message ID */
	if (msg == NULL) {
		return(0);
	}
	if (msg->cm_fields['I'] == NULL) {
		return(0);
	}
	if (IsEmptyStr(msg->cm_fields['I'])) {
		return(0);
	}

	/* Generate the message ID */
	strcpy(msgid, msg->cm_fields['I']);
	if (haschar(msgid, '@') == 0) {
		strcat(msgid, "@");
		if (msg->cm_fields['N'] != NULL) {
			strcat(msgid, msg->cm_fields['N']);
		}
		else {
			return(0);
		}
	}

	cdbut = cdb_fetch(CDB_USETABLE, msgid, strlen(msgid));
	if (cdbut != NULL) {
		cdb_free(cdbut);
		syslog(LOG_DEBUG, "network_usetable() : we already have %s\n", msgid);
		return(1);
	}

	/* If we got to this point, it's unique: add it. */
	strcpy(ut.ut_msgid, msgid);
	ut.ut_timestamp = time(NULL);
	cdb_store(CDB_USETABLE, msgid, strlen(msgid), &ut, sizeof(struct UseTable) );
	return(0);
}










/*
 * Send the *entire* contents of the current room to one specific network node,
 * ignoring anything we know about which messages have already undergone
 * network processing.  This can be used to bring a new node into sync.
 */
int network_sync_to(char *target_node) {
	SpoolControl sc;
	int num_spooled = 0;
	int found_node = 0;
	char buf[256];
	char sc_type[256];
	char sc_node[256];
	char sc_room[256];
	char filename[PATH_MAX];
	FILE *fp;

	/* Grab the configuration line we're looking for */
	assoc_file_name(filename, sizeof filename, &CC->room, ctdl_netcfg_dir);
	begin_critical_section(S_NETCONFIGS);
	fp = fopen(filename, "r");
	if (fp == NULL) {
		end_critical_section(S_NETCONFIGS);
		return(-1);
	}
	while (fgets(buf, sizeof buf, fp) != NULL) {
		buf[strlen(buf)-1] = 0;
		extract_token(sc_type, buf, 0, '|', sizeof sc_type);
		extract_token(sc_node, buf, 1, '|', sizeof sc_node);
		extract_token(sc_room, buf, 2, '|', sizeof sc_room);
		if ( (!strcasecmp(sc_type, "ignet_push_share"))
		   && (!strcasecmp(sc_node, target_node)) ) {
			found_node = 1;
			
			/* Concise syntax because we don't need a full linked-list */
			memset(&sc, 0, sizeof(SpoolControl));
			sc.ignet_push_shares = (maplist *)
				malloc(sizeof(maplist));
			sc.ignet_push_shares->next = NULL;
			safestrncpy(sc.ignet_push_shares->remote_nodename,
				sc_node,
				sizeof sc.ignet_push_shares->remote_nodename);
			safestrncpy(sc.ignet_push_shares->remote_roomname,
				sc_room,
				sizeof sc.ignet_push_shares->remote_roomname);
		}
	}
	fclose(fp);
	end_critical_section(S_NETCONFIGS);

	if (!found_node) return(-1);

	/* Send ALL messages */
	num_spooled = CtdlForEachMessage(MSGS_ALL, 0L, NULL, NULL, NULL,
		network_spool_msg, &sc);

	/* Concise cleanup because we know there's only one node in the sc */
	free(sc.ignet_push_shares);

	syslog(LOG_NOTICE, "Synchronized %d messages to <%s>\n",
		num_spooled, target_node);
	return(num_spooled);
}


/*
 * Implements the NSYN command
 */
void cmd_nsyn(char *argbuf) {
	int num_spooled;
	char target_node[256];

	if (CtdlAccessCheck(ac_aide)) return;

	extract_token(target_node, argbuf, 0, '|', sizeof target_node);
	num_spooled = network_sync_to(target_node);
	if (num_spooled >= 0) {
		cprintf("%d Spooled %d messages.\n", CIT_OK, num_spooled);
	}
	else {
		cprintf("%d No such room/node share exists.\n",
			ERROR + ROOM_NOT_FOUND);
	}
}



/*
 * Batch up and send all outbound traffic from the current room
 */
void network_queue_interesting_rooms(struct ctdlroom *qrbuf, void *data) {
	int i;
	struct RoomProcList *ptr;
	long QRNum = qrbuf->QRnumber;
	void *v;
	roomlists *RP = (roomlists*) data;

	if (!GetHash(RP->RoomsInterestedIn, LKEY(QRNum), &v))
		return;

	ptr = (struct RoomProcList *) malloc(sizeof (struct RoomProcList));
	if (ptr == NULL) return;

	ptr->namelen = strlen(qrbuf->QRname);
	if (ptr->namelen > ROOMNAMELEN)
		ptr->namelen = ROOMNAMELEN - 1;

	memcpy (ptr->name, qrbuf->QRname, ptr->namelen);
	ptr->name[ptr->namelen] = '\0';
	ptr->QRNum = qrbuf->QRnumber;

	for (i = 0; i < ptr->namelen; i++)
	{
		ptr->lcname[i] = tolower(ptr->name[i]);
	}

	ptr->lcname[ptr->namelen] = '\0';
	ptr->key = hashlittle(ptr->lcname, ptr->namelen, 9872345);
	ptr->next = RP->rplist;
	RP->rplist = ptr;
}

/*
 * Batch up and send all outbound traffic from the current room
 */
void network_queue_room(struct ctdlroom *qrbuf, void *data) {
	int i;
	struct RoomProcList *ptr;

	if (qrbuf->QRdefaultview == VIEW_QUEUE)
		return;
	ptr = (struct RoomProcList *) malloc(sizeof (struct RoomProcList));
	if (ptr == NULL) return;

	ptr->namelen = strlen(qrbuf->QRname);
	if (ptr->namelen > ROOMNAMELEN)
		ptr->namelen = ROOMNAMELEN - 1;

	memcpy (ptr->name, qrbuf->QRname, ptr->namelen);
	ptr->name[ptr->namelen] = '\0';
	ptr->QRNum = qrbuf->QRnumber;

	for (i = 0; i < ptr->namelen; i++)
	{
		ptr->lcname[i] = tolower(ptr->name[i]);
	}
	ptr->lcname[ptr->namelen] = '\0';
	ptr->key = hashlittle(ptr->lcname, ptr->namelen, 9872345);

	begin_critical_section(S_RPLIST);
	ptr->next = rplist;
	rplist = ptr;
	end_critical_section(S_RPLIST);
}

void destroy_network_queue_room(RoomProcList *rplist)
{
	struct RoomProcList *cur, *p;

	cur = rplist;
	while (cur != NULL)
	{
		p = cur->next;
		free (cur);
		cur = p;		
	}
}

void destroy_network_queue_room_locked (void)
{
	begin_critical_section(S_RPLIST);
	destroy_network_queue_room(rplist);
	end_critical_section(S_RPLIST);
}



/*
 * Bounce a message back to the sender
 */
void network_bounce(struct CtdlMessage *msg, char *reason) {
	char *oldpath = NULL;
	char buf[SIZ];
	char bouncesource[SIZ];
	char recipient[SIZ];
	struct recptypes *valid = NULL;
	char force_room[ROOMNAMELEN];
	static int serialnum = 0;
	size_t size;

	syslog(LOG_DEBUG, "entering network_bounce()\n");

	if (msg == NULL) return;

	snprintf(bouncesource, sizeof bouncesource, "%s@%s", BOUNCESOURCE, config.c_nodename);

	/* 
	 * Give it a fresh message ID
	 */
	if (msg->cm_fields['I'] != NULL) {
		free(msg->cm_fields['I']);
	}
	snprintf(buf, sizeof buf, "%ld.%04lx.%04x@%s",
		(long)time(NULL), (long)getpid(), ++serialnum, config.c_fqdn);
	msg->cm_fields['I'] = strdup(buf);

	/*
	 * FIXME ... right now we're just sending a bounce; we really want to
	 * include the text of the bounced message.
	 */
	if (msg->cm_fields['M'] != NULL) {
		free(msg->cm_fields['M']);
	}
	msg->cm_fields['M'] = strdup(reason);
	msg->cm_format_type = 0;

	/*
	 * Turn the message around
	 */
	if (msg->cm_fields['R'] == NULL) {
		free(msg->cm_fields['R']);
	}

	if (msg->cm_fields['D'] == NULL) {
		free(msg->cm_fields['D']);
	}

	snprintf(recipient, sizeof recipient, "%s@%s",
		msg->cm_fields['A'], msg->cm_fields['N']);

	if (msg->cm_fields['A'] == NULL) {
		free(msg->cm_fields['A']);
	}

	if (msg->cm_fields['N'] == NULL) {
		free(msg->cm_fields['N']);
	}

	if (msg->cm_fields['U'] == NULL) {
		free(msg->cm_fields['U']);
	}

	msg->cm_fields['A'] = strdup(BOUNCESOURCE);
	msg->cm_fields['N'] = strdup(config.c_nodename);
	msg->cm_fields['U'] = strdup("Delivery Status Notification (Failure)");

	/* prepend our node to the path */
	if (msg->cm_fields['P'] != NULL) {
		oldpath = msg->cm_fields['P'];
		msg->cm_fields['P'] = NULL;
	}
	else {
		oldpath = strdup("unknown_user");
	}
	size = strlen(oldpath) + SIZ;
	msg->cm_fields['P'] = malloc(size);
	snprintf(msg->cm_fields['P'], size, "%s!%s", config.c_nodename, oldpath);
	free(oldpath);

	/* Now submit the message */
	valid = validate_recipients(recipient, NULL, 0);
	if (valid != NULL) if (valid->num_error != 0) {
		free_recipients(valid);
		valid = NULL;
	}
	if ( (valid == NULL) || (!strcasecmp(recipient, bouncesource)) ) {
		strcpy(force_room, config.c_aideroom);
	}
	else {
		strcpy(force_room, "");
	}
	if ( (valid == NULL) && IsEmptyStr(force_room) ) {
		strcpy(force_room, config.c_aideroom);
	}
	CtdlSubmitMsg(msg, valid, force_room, 0);

	/* Clean up */
	if (valid != NULL) free_recipients(valid);
	CtdlFreeMessage(msg);
	syslog(LOG_DEBUG, "leaving network_bounce()\n");
}







/*
 * network_do_queue()
 * 
 * Run through the rooms doing various types of network stuff.
 */
void network_do_queue(void) {
	static int doing_queue = 0;
	static time_t last_run = 0L;
	int full_processing = 1;
	char *working_ignetcfg;
	NetMap *the_netmap = NULL;
	int netmap_changed = 0;
	roomlists RL;

	/*
	 * Run the full set of processing tasks no more frequently
	 * than once every n seconds
	 */
	if ( (time(NULL) - last_run) < config.c_net_freq ) {
		full_processing = 0;
		syslog(LOG_DEBUG, "Network full processing in %ld seconds.\n",
			config.c_net_freq - (time(NULL)- last_run)
		);
	}

	/*
	 * This is a simple concurrency check to make sure only one queue run
	 * is done at a time.  We could do this with a mutex, but since we
	 * don't really require extremely fine granularity here, we'll do it
	 * with a static variable instead.
	 */
	if (doing_queue) {
		return;
	}
	doing_queue = 1;

	begin_critical_section(S_RPLIST);
	RL.rplist = rplist;
	rplist = NULL;
	end_critical_section(S_RPLIST);

	RL.RoomsInterestedIn = NewHash(1, lFlathash);
	if (full_processing &&
	    (GetNetworkedRoomNumbers(ctdl_netcfg_dir, RL.RoomsInterestedIn)==0))
	{
		doing_queue = 0;
		DeleteHash(&RL.RoomsInterestedIn);
		if (RL.rplist == NULL)
			return;
	}
	/* Load the IGnet Configuration into memory */
	working_ignetcfg = load_working_ignetcfg();

	/*
	 * Load the network map and filter list into memory.
	 */
	if (!server_shutting_down)
		the_netmap = read_network_map();
	if (!server_shutting_down)
		load_network_filter_list();

	/* 
	 * Go ahead and run the queue
	 */
	if (full_processing && !server_shutting_down) {
		syslog(LOG_DEBUG, "network: loading outbound queue\n");
		CtdlForEachRoom(network_queue_interesting_rooms, &RL);
	}

	if ((RL.rplist != NULL) && (!server_shutting_down)) {
		RoomProcList *ptr, *cmp;
		ptr = RL.rplist;
		syslog(LOG_DEBUG, "network: running outbound queue\n");
		while (ptr != NULL && !server_shutting_down) {
			
			cmp = ptr->next;

			while (cmp != NULL) {
				if ((cmp->namelen > 0) &&
				    (cmp->key == ptr->key) &&
				    (cmp->namelen == ptr->namelen) &&
				    (strcmp(cmp->lcname, ptr->lcname) == 0))
				{
					cmp->namelen = 0;
				}
				cmp = cmp->next;
			}

			if (ptr->namelen > 0) {
				network_spoolout_room(ptr, 
						      working_ignetcfg,
						      the_netmap);
			}
			ptr = ptr->next;
		}
	}

	/* If there is anything in the inbound queue, process it */
	if (!server_shutting_down) {
		network_do_spoolin(working_ignetcfg, 
				   &the_netmap,
				   &netmap_changed);
	}

	/* Save the network map back to disk */
	write_network_map(the_netmap, netmap_changed);

	/* Free the filter list in memory */
	free_netfilter_list();

	network_consolidate_spoolout(working_ignetcfg, the_netmap);
	free(working_ignetcfg);

	syslog(LOG_DEBUG, "network: queue run completed\n");

	if (full_processing) {
		last_run = time(NULL);
	}
	DeleteHash(&RL.RoomsInterestedIn);
	destroy_network_queue_room(RL.rplist);
	doing_queue = 0;
}




int network_room_handler (struct ctdlroom *room)
{
	network_queue_room(room, NULL);
	return 0;
}


/*
 * Module entry point
 */
CTDL_MODULE_INIT(network)
{
	if (!threading)
	{
		CtdlRegisterProtoHook(cmd_nsyn, "NSYN", "Synchronize room to node");
		CtdlRegisterRoomHook(network_room_handler);
		CtdlRegisterCleanupHook(destroy_network_queue_room_locked);
		CtdlRegisterSessionHook(network_do_queue, EVT_TIMER);
	}
	return "network";
}
