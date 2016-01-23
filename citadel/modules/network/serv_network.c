/*
 * This module handles shared rooms, inter-Citadel mail, and outbound
 * mailing list processing.
 *
 * Copyright (c) 2000-2016 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * ** NOTE **   A word on the S_NETCONFIGS semaphore:
 * This is a fairly high-level type of critical section.  It ensures that no
 * two threads work on the netconfigs files at the same time.  Since we do
 * so many things inside these, here are the rules:
 *  1. begin_critical_section(S_NETCONFIGS) *before* begin_ any others.
 *  2. Do *not* perform any I/O with the client during these sections.
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
} roomlists;


/*
 * When we do network processing, it's accomplished in two passes; one to
 * gather a list of rooms and one to actually do them.  It's ok that rplist
 * is global; we have a mutex that keeps it safe.
 */
struct RoomProcList *rplist = NULL;


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

	/* Load the netconfig for this room */
	pRNCFG = CtdlGetNetCfgForRoom(CCC->room.QRnumber);
	if (pRNCFG == NULL) {					// no netconfig at all?
		return -1;
	}
	if (pRNCFG->NetConfigs[ignet_push_share] == NULL)	// no ignet push shares?
	{
		FreeRoomNetworkStruct(&pRNCFG);
		return -1;
	}

	/* Search for an ignet_oush_share configuration bearing the target node's name */
	for (pCfgLine = pRNCFG->NetConfigs[ignet_push_share]; pCfgLine != NULL; pCfgLine = pCfgLine->next)
	{
		if (!strcmp(ChrPtr(pCfgLine->Value[0]), target_node))
			break;
	}

	/* If we aren't sharing with that node, bail out */
	if (pCfgLine == NULL)
	{
		FreeRoomNetworkStruct(&pRNCFG);
		return -1;
	}

	/* If we got here, we're good to go ... make up a dummy spoolconfig and roll with it */

	begin_critical_section(S_NETCONFIGS);
	memset(&sc, 0, sizeof(SpoolControl));
	memset(&OneRNCFG, 0, sizeof(OneRoomNetCfg));
	sc.RNCfg = &OneRNCFG;
	sc.RNCfg->NetConfigs[ignet_push_share] = DuplicateOneGenericCfgLine(pCfgLine);
	sc.Users[ignet_push_share] = NewStrBufPlain(NULL, (StrLength(pCfgLine->Value[0]) + StrLength(pCfgLine->Value[1]) + 10) );
	StrBufAppendBuf(sc.Users[ignet_push_share], pCfgLine->Value[0], 0);
	StrBufAppendBufPlain(sc.Users[ignet_push_share], HKEY(","), 0);
	StrBufAppendBuf(sc.Users[ignet_push_share], pCfgLine->Value[1], 0);
	CalcListID(&sc);
	end_critical_section(S_NETCONFIGS);

	sc.working_ignetcfg = CtdlLoadIgNetCfg();
	sc.the_netmap = CtdlReadNetworkMap();

	/* Send ALL messages */
	num_spooled = CtdlForEachMessage(MSGS_ALL, 0L, NULL, NULL, NULL, network_spool_msg, &sc);

	/* Concise cleanup because we know there's only one node in the sc */
	DeleteGenericCfgLine(NULL, &sc.RNCfg->NetConfigs[ignet_push_share]);

	DeleteHash(&sc.working_ignetcfg);
	DeleteHash(&sc.the_netmap);
	free_spoolcontrol_struct_members(&sc);

	QN_syslog(LOG_NOTICE, "Synchronized %d messages to <%s>", num_spooled, target_node);
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
		cprintf("%d No such room/node share exists.\n", ERROR + ROOM_NOT_FOUND);
	}
}

RoomProcList *CreateRoomProcListEntry(struct ctdlroom *qrbuf, OneRoomNetCfg *OneRNCFG)
{
	int i;
	struct RoomProcList *ptr;

	ptr = (struct RoomProcList *) malloc(sizeof (struct RoomProcList));
	if (ptr == NULL) {
		return NULL;
	}

	ptr->namelen = strlen(qrbuf->QRname);
	if (ptr->namelen > ROOMNAMELEN) {
		ptr->namelen = ROOMNAMELEN - 1;
	}

	memcpy (ptr->name, qrbuf->QRname, ptr->namelen);
	ptr->name[ptr->namelen] = '\0';
	ptr->QRNum = qrbuf->QRnumber;

	for (i = 0; i < ptr->namelen; i++)
	{
		ptr->lcname[i] = tolower(ptr->name[i]);
	}

	ptr->lcname[ptr->namelen] = '\0';
	ptr->key = hashlittle(ptr->lcname, ptr->namelen, 9872345);
	return ptr;
}

/*
 * Batch up and send all outbound traffic from the current room
 */
void network_queue_interesting_rooms(struct ctdlroom *qrbuf, void *data, OneRoomNetCfg *OneRNCfg)
{
	struct RoomProcList *ptr;
	roomlists *RP = (roomlists*) data;

	if (!HaveSpoolConfig(OneRNCfg)) {
		return;
	}

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
int network_room_handler(struct ctdlroom *qrbuf)
{
	struct RoomProcList *ptr;
	OneRoomNetCfg *RNCfg;

	if (qrbuf->QRdefaultview == VIEW_QUEUE) {
		return 1;
	}

	RNCfg = CtdlGetNetCfgForRoom(qrbuf->QRnumber);
	if (RNCfg == NULL) {
		return 1;
	}

	if (!HaveSpoolConfig(RNCfg)) {
		FreeRoomNetworkStruct(&RNCfg);
		return 1;
	}

	ptr = CreateRoomProcListEntry(qrbuf, RNCfg);
	if (ptr == NULL) {
		FreeRoomNetworkStruct(&RNCfg);
		return 1;
	}

	begin_critical_section(S_RPLIST);
	ptr->next = rplist;
	rplist = ptr;
	end_critical_section(S_RPLIST);
	FreeRoomNetworkStruct(&RNCfg);
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
	if ( (time(NULL) - last_run) < CtdlGetConfigLong("c_net_freq") )
	{
		full_processing = 0;
		syslog(LOG_DEBUG, "Network full processing in %ld seconds.",
		       CtdlGetConfigLong("c_net_freq") - (time(NULL)- last_run)
		);
	}

	become_session(&networker_spool_CC);
	begin_critical_section(S_RPLIST);
	RL.rplist = rplist;
	rplist = NULL;
	end_critical_section(S_RPLIST);

	// TODO hm, check whether we have a config at all here?
	/* Load the IGnet Configuration into memory */
	working_ignetcfg = CtdlLoadIgNetCfg();

	/*
	 * Load the network map and filter list into memory.
	 */
	if (!server_shutting_down) {
		the_netmap = CtdlReadNetworkMap();
	}

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
				InspectQueuedRoom(&sc, ptr, working_ignetcfg, the_netmap);
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

	/* combine single message files into one spool entry per remote node. */
	network_consolidate_spoolout(working_ignetcfg, the_netmap);

	/* shut down. */

	DeleteHash(&the_netmap);

	DeleteHash(&working_ignetcfg);

	QNM_syslog(LOG_DEBUG, "network: queue run completed");

	if (full_processing) {
		last_run = time(NULL);
	}
	destroy_network_queue_room(RL.rplist);
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


int ignet_aftersave(struct CtdlMessage *msg, recptypes *recps)
{
	/* For IGnet mail, we have to save a new copy into the spooler for
	 * each recipient, with the R and D fields set to the recipient and
	 * destination-node.  This has two ugly side effects: all other
	 * recipients end up being unlisted in this recipient's copy of the
	 * message, and it has to deliver multiple messages to the same
	 * node.  We'll revisit this again in a year or so when everyone has
	 * a network spool receiver that can handle the new style messages.
	 */
	if ((recps != NULL) && (recps->num_ignet > 0))
	{
		char *recipient;
		int rv = 0;
		struct ser_ret smr;
		FILE *network_fp = NULL;
		char submit_filename[128];
		static int seqnum = 1;
		int i;
		char *hold_R, *hold_D, *RBuf, *DBuf;
		long hrlen, hdlen, rblen, dblen, count, rlen;
		CitContext *CCC = MyContext();

		CM_GetAsField(msg, eRecipient, &hold_R, &hrlen);;
		CM_GetAsField(msg, eDestination, &hold_D, &hdlen);;

		count = num_tokens(recps->recp_ignet, '|');
		rlen = strlen(recps->recp_ignet);
		recipient = malloc(rlen + 1);
		RBuf = malloc(rlen + 1);
		DBuf = malloc(rlen + 1);
		for (i=0; i<count; ++i) {
			extract_token(recipient, recps->recp_ignet, i, '|', rlen + 1);

			rblen = extract_token(RBuf, recipient, 0, '@', rlen + 1);
			dblen = extract_token(DBuf, recipient, 1, '@', rlen + 1);
		
			CM_SetAsField(msg, eRecipient, &RBuf, rblen);;
			CM_SetAsField(msg, eDestination, &DBuf, dblen);;
			CtdlSerializeMessage(&smr, msg);
			if (smr.len > 0) {
				snprintf(submit_filename, sizeof submit_filename,
					 "%s/netmail.%04lx.%04x.%04x",
					 ctdl_netin_dir,
					 (long) getpid(),
					 CCC->cs_pid,
					 ++seqnum);

				network_fp = fopen(submit_filename, "wb+");
				if (network_fp != NULL) {
					rv = fwrite(smr.ser, smr.len, 1, network_fp);
					if (rv == -1) {
						MSG_syslog(LOG_EMERG, "CtdlSubmitMsg(): Couldn't write network spool file: %s",
							   strerror(errno));
					}
					fclose(network_fp);
				}
				free(smr.ser);
			}
			CM_GetAsField(msg, eRecipient, &RBuf, &rblen);;
			CM_GetAsField(msg, eDestination, &DBuf, &dblen);;
		}
		free(RBuf);
		free(DBuf);
		free(recipient);
		CM_SetAsField(msg, eRecipient, &hold_R, hrlen);
		CM_SetAsField(msg, eDestination, &hold_D, hdlen);
		return 1;
	}
	return 0;
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
		CtdlRegisterMessageHook(ignet_aftersave, EVT_AFTERSAVE);

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
