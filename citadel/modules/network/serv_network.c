/*
 * This module handles shared rooms, inter-Citadel mail, and outbound
 * mailing list processing.
 *
 * Copyright (c) 2000-2012 by the citadel.org team
 *
 *  This program is open source software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
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
#include "context.h"
#include "ctdl_module.h"
#include "netspool.h"
#include "netmail.h"

int NetQDebugEnabled = 0;
struct CitContext networker_spool_CC;

/* comes from lookup3.c from libcitadel... */
extern uint32_t hashlittle( const void *key, size_t length, uint32_t initval);

typedef struct __roomlists {
	RoomProcList *rplist;
}roomlists;
/*
 * When we do network processing, it's accomplished in two passes; one to
 * gather a list of rooms and one to actually do them.  It's ok that rplist
 * is global; we have a mutex that keeps it safe.
 */
struct RoomProcList *rplist = NULL;



/*
 * Check the use table.  This is a list of messages which have recently
 * arrived on the system.  It is maintained and queried to prevent the same
 * message from being entered into the database multiple times if it happens
 * to arrive multiple times by accident.
 */
int network_usetable(struct CtdlMessage *msg)
{
	StrBuf *msgid;
	struct CitContext *CCC = CC;
	time_t now;

	/* Bail out if we can't generate a message ID */
	if ((msg == NULL) || (msg->cm_fields['I'] == NULL) || (IsEmptyStr(msg->cm_fields['I'])))
	{
		return(0);
	}

	/* Generate the message ID */
	msgid = NewStrBufPlain(msg->cm_fields['I'], -1);
	if (haschar(ChrPtr(msgid), '@') == 0) {
		StrBufAppendBufPlain(msgid, HKEY("@"), 0);
		if (msg->cm_fields['N'] != NULL) {
			StrBufAppendBufPlain(msgid, msg->cm_fields['N'], -1, 0);
		}
		else {
			FreeStrBuf(&msgid);
			return(0);
		}
	}
	now = time(NULL);
	if (CheckIfAlreadySeen("Networker Import",
			       msgid,
			       now, 0,
			       eCheckUpdate,
			       CCC->cs_pid, 0) != 0)
	{
		FreeStrBuf(&msgid);
		return(1);
	}
	FreeStrBuf(&msgid);

	return(0);
}



/*
 * Send the *entire* contents of the current room to one specific network node,
 * ignoring anything we know about which messages have already undergone
 * network processing.  This can be used to bring a new node into sync.
 */
int network_sync_to(char *target_node, long len)
{
	struct CitContext *CCC = CC;
	OneRoomNetCfg OneRNCFG;
	OneRoomNetCfg *pRNCFG;
	const RoomNetCfgLine *pCfgLine;
	SpoolControl sc;
	int num_spooled = 0;

	/* Grab the configuration line we're looking for */
	begin_critical_section(S_NETCONFIGS);
	pRNCFG = CtdlGetNetCfgForRoom(CCC->room.QRnumber);
	if ((pRNCFG == NULL) ||
	    (pRNCFG->NetConfigs[ignet_push_share] == NULL))
	{
		return -1;
	}

	pCfgLine = pRNCFG->NetConfigs[ignet_push_share];
	while (pCfgLine != NULL)
	{
		if (!strcmp(ChrPtr(pCfgLine->Value[0]), target_node))
			break;
		pCfgLine = pCfgLine->next;
	}
	if (pCfgLine == NULL)
	{
		return -1;
	}

	memset(&sc, 0, sizeof(SpoolControl));
	memset(&OneRNCFG, 0, sizeof(OneRoomNetCfg));
	sc.RNCfg = &OneRNCFG;
	sc.RNCfg->NetConfigs[ignet_push_share] = DuplicateOneGenericCfgLine(pCfgLine);
	sc.Users[ignet_push_share] = NewStrBufPlain(NULL,
						    StrLength(pCfgLine->Value[0]) +
						    StrLength(pCfgLine->Value[1]) + 10);
	StrBufAppendBuf(sc.Users[ignet_push_share], 
			pCfgLine->Value[0],
			0);
	StrBufAppendBufPlain(sc.Users[ignet_push_share], 
			     HKEY(","),
			     0);

	StrBufAppendBuf(sc.Users[ignet_push_share], 
			pCfgLine->Value[1],
			0);
	CalcListID(&sc);

	end_critical_section(S_NETCONFIGS);

	sc.working_ignetcfg = CtdlLoadIgNetCfg();
	sc.the_netmap = CtdlReadNetworkMap();

	/* Send ALL messages */
	num_spooled = CtdlForEachMessage(MSGS_ALL, 0L, NULL, NULL, NULL,
		network_spool_msg, &sc);

	/* Concise cleanup because we know there's only one node in the sc */
	DeleteGenericCfgLine(NULL/*TODO*/, &sc.RNCfg->NetConfigs[ignet_push_share]);

	DeleteHash(&sc.working_ignetcfg);
	DeleteHash(&sc.the_netmap);
	free_spoolcontrol_struct_members(&sc);

	QN_syslog(LOG_NOTICE, "Synchronized %d messages to <%s>\n",
		  num_spooled, target_node);
	return(num_spooled);
}


/*
 * Implements the NSYN command
 */
void cmd_nsyn(char *argbuf) {
	int num_spooled;
	long len;
	char target_node[256];

	if (CtdlAccessCheck(ac_aide)) return;

	len = extract_token(target_node, argbuf, 0, '|', sizeof target_node);
	num_spooled = network_sync_to(target_node, len);
	if (num_spooled >= 0) {
		cprintf("%d Spooled %d messages.\n", CIT_OK, num_spooled);
	}
	else {
		cprintf("%d No such room/node share exists.\n",
			ERROR + ROOM_NOT_FOUND);
	}
}

RoomProcList *CreateRoomProcListEntry(struct ctdlroom *qrbuf, OneRoomNetCfg *OneRNCFG)
{
	int i;
	struct RoomProcList *ptr;

	ptr = (struct RoomProcList *) malloc(sizeof (struct RoomProcList));
	if (ptr == NULL) return NULL;

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
	ptr->lastsent = OneRNCFG->lastsent;
	ptr->OneRNCfg = OneRNCFG;
	return ptr;
}

/*
 * Batch up and send all outbound traffic from the current room
 */
void network_queue_interesting_rooms(struct ctdlroom *qrbuf, void *data, OneRoomNetCfg *OneRNCfg)
{
	struct RoomProcList *ptr;
	roomlists *RP = (roomlists*) data;

	if (!HaveSpoolConfig(OneRNCfg))
		return;

	ptr = CreateRoomProcListEntry(qrbuf, OneRNCfg);

	if (ptr != NULL)
	{
		ptr->next = RP->rplist;
		RP->rplist = ptr;
	}
}

/*
 * Batch up and send all outbound traffic from the current room
 */
int network_room_handler (struct ctdlroom *qrbuf)
{
	struct RoomProcList *ptr;
	OneRoomNetCfg* RNCfg;

	if (qrbuf->QRdefaultview == VIEW_QUEUE)
		return 1;

	RNCfg = CtdlGetNetCfgForRoom(qrbuf->QRnumber);
	if (RNCfg == NULL)
		return 1;

	if (!HaveSpoolConfig(RNCfg))
		return 1;

	ptr = CreateRoomProcListEntry(qrbuf, RNCfg);
	if (ptr == NULL)
		return 1;

	ptr->OneRNCfg = NULL;
	begin_critical_section(S_RPLIST);
	ptr->next = rplist;
	rplist = ptr;
	end_critical_section(S_RPLIST);
	return 1;
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
void network_bounce(struct CtdlMessage *msg, char *reason)
{
	struct CitContext *CCC = CC;
	char *oldpath = NULL;
	char buf[SIZ];
	char bouncesource[SIZ];
	char recipient[SIZ];
	struct recptypes *valid = NULL;
	char force_room[ROOMNAMELEN];
	static int serialnum = 0;
	size_t size;

	QNM_syslog(LOG_DEBUG, "entering network_bounce()\n");

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
		msg->cm_fields['R'] = NULL;
	}

	if (msg->cm_fields['D'] == NULL) {
		free(msg->cm_fields['D']);
		msg->cm_fields['D'] = NULL;
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
	QNM_syslog(LOG_DEBUG, "leaving network_bounce()\n");
}



/*
 * network_do_queue()
 * 
 * Run through the rooms doing various types of network stuff.
 */
void network_do_queue(void)
{
	struct CitContext *CCC = CC;
	static time_t last_run = 0L;
	int full_processing = 1;
	HashList *working_ignetcfg;
	HashList *the_netmap = NULL;
	int netmap_changed = 0;
	roomlists RL;
	SpoolControl *sc = NULL;
	SpoolControl *pSC;

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

	become_session(&networker_spool_CC);
	begin_critical_section(S_RPLIST);
	RL.rplist = rplist;
	rplist = NULL;
	end_critical_section(S_RPLIST);
///TODO hm, check whether we have a config at all here?
	/* Load the IGnet Configuration into memory */
	working_ignetcfg = CtdlLoadIgNetCfg();

	/*
	 * Load the network map and filter list into memory.
	 */
	if (!server_shutting_down)
		the_netmap = CtdlReadNetworkMap();
#if 0
	/* filterlist isn't supported anymore
	if (!server_shutting_down)
		load_network_filter_list();
	*/
#endif

	/* 
	 * Go ahead and run the queue
	 */
	if (full_processing && !server_shutting_down) {
		QNM_syslog(LOG_DEBUG, "network: loading outbound queue");
		CtdlForEachNetCfgRoom(network_queue_interesting_rooms, &RL, maxRoomNetCfg);
	}

	if ((RL.rplist != NULL) && (!server_shutting_down)) {
		RoomProcList *ptr, *cmp;
		ptr = RL.rplist;
		QNM_syslog(LOG_DEBUG, "network: running outbound queue");
		while (ptr != NULL && !server_shutting_down) {
			
			cmp = ptr->next;
			/* filter duplicates from the list... */
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
				InspectQueuedRoom(&sc,
						  ptr, 
						  working_ignetcfg,
						  the_netmap);
			}
			ptr = ptr->next;
		}
	}


	pSC = sc;
	while (pSC != NULL)
	{
		network_spoolout_room(pSC);
		pSC = pSC->next;
	}

	pSC = sc;
	while (pSC != NULL)
	{
		sc = pSC->next;
		free_spoolcontrol_struct(&pSC);
		pSC = sc;
	}
	/* If there is anything in the inbound queue, process it */
	if (!server_shutting_down) {
		network_do_spoolin(working_ignetcfg, 
				   the_netmap,
				   &netmap_changed);
	}

	/* Free the filter list in memory */
	free_netfilter_list();

	/* Save the network map back to disk */
	if (netmap_changed) {
		StrBuf *MapStr = CtdlSerializeNetworkMap(the_netmap);
		char *pMapStr = SmashStrBuf(&MapStr);
		CtdlPutSysConfig(IGNETMAP, pMapStr);
		free(pMapStr);
	}

	/* combine singe message files into one spool entry per remote node. */
	network_consolidate_spoolout(working_ignetcfg, the_netmap);

	/* shut down. */

	DeleteHash(&the_netmap);

	DeleteHash(&working_ignetcfg);

	QNM_syslog(LOG_DEBUG, "network: queue run completed");

	if (full_processing) {
		last_run = time(NULL);
	}
	destroy_network_queue_room(RL.rplist);
	SaveChangedConfigs();

}








void network_logout_hook(void)
{
	CitContext *CCC = MyContext();

	/*
	 * If we were talking to a network node, we're not anymore...
	 */
	if (!IsEmptyStr(CCC->net_node)) {
		CtdlNetworkTalkingTo(CCC->net_node, strlen(CCC->net_node), NTT_REMOVE);
		CCC->net_node[0] = '\0';
	}
}
void network_cleanup_function(void)
{
	struct CitContext *CCC = CC;

	if (!IsEmptyStr(CCC->net_node)) {
		CtdlNetworkTalkingTo(CCC->net_node, strlen(CCC->net_node), NTT_REMOVE);
		CCC->net_node[0] = '\0';
	}
}


/*
 * Module entry point
 */

void SetNetQDebugEnabled(const int n)
{
	NetQDebugEnabled = n;
}

CTDL_MODULE_INIT(network)
{
	if (!threading)
	{
		CtdlFillSystemContext(&networker_spool_CC, "CitNetSpool");
		CtdlRegisterDebugFlagHook(HKEY("networkqueue"), SetNetQDebugEnabled, &NetQDebugEnabled);
		CtdlRegisterSessionHook(network_cleanup_function, EVT_STOP, PRIO_STOP + 30);
                CtdlRegisterSessionHook(network_logout_hook, EVT_LOGOUT, PRIO_LOGOUT + 10);
		CtdlRegisterProtoHook(cmd_nsyn, "NSYN", "Synchronize room to node");
		CtdlRegisterRoomHook(network_room_handler);
		CtdlRegisterCleanupHook(destroy_network_queue_room_locked);
		CtdlRegisterSessionHook(network_do_queue, EVT_TIMER, PRIO_QUEUE + 10);
	}
	return "network";
}
