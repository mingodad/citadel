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


/*
 * We build a map of the Citadel network during network runs.
 */
struct NetMap {
	struct NetMap *next;
	char nodename[SIZ];
	time_t lastcontact;
	char nexthop[SIZ];
};

struct NetMap *the_netmap = NULL;



/* 
 * Read the network map from its configuration file into memory.
 */
void read_network_map(void) {
	char *serialized_map = NULL;
	int i;
	char buf[SIZ];
	struct NetMap *nmptr;

	serialized_map = CtdlGetSysConfig(IGNETMAP);
	if (serialized_map == NULL) return;	/* if null, no entries */

	/* Use the string tokenizer to grab one line at a time */
	for (i=0; i<num_tokens(serialized_map, '\n'); ++i) {
		extract_token(buf, serialized_map, i, '\n');
		nmptr = (struct NetMap *) mallok(sizeof(struct NetMap));
		extract(nmptr->nodename, buf, 0);
		nmptr->lastcontact = extract_long(buf, 1);
		extract(nmptr->nexthop, buf, 2);
		nmptr->next = the_netmap;
		the_netmap = nmptr;
	}

	phree(serialized_map);
}


/*
 * Write the network map from memory back to the configuration file.
 */
void write_network_map(void) {
	char *serialized_map = NULL;
	struct NetMap *nmptr;

	serialized_map = strdoop("");

	if (the_netmap != NULL) {
		for (nmptr = the_netmap; nmptr != NULL; nmptr = nmptr->next) {
			serialized_map = reallok(serialized_map,
						(strlen(serialized_map)+SIZ) );
			if (strlen(nmptr->nodename) > 0) {
				sprintf(&serialized_map[strlen(serialized_map)],
					"%s|%ld|%s\n",
					nmptr->nodename,
					nmptr->lastcontact,
					nmptr->nexthop);
			}
		}
	}

	CtdlPutSysConfig(IGNETMAP, serialized_map);
	phree(serialized_map);

	/* Now free the list */
	while (the_netmap != NULL) {
		nmptr = the_netmap->next;
		phree(the_netmap);
		the_netmap = nmptr;
	}
}



/* 
 * Check the network map and determine whether the supplied node name is
 * valid.  If it is not a neighbor node, supply the name of a neighbor node
 * which is the next hop.  If it *is* a neighbor node, we also fill in the
 * shared secret.
 */
int is_valid_node(char *nexthop, char *secret, char *node) {
	char *ignetcfg = NULL;
	int i;
	char linebuf[SIZ];
	char buf[SIZ];
	int retval;
	struct NetMap *nmptr;

	if (node == NULL) {
		return(-1);
	}

	/*
	 * First try the neighbor nodes
	 */
	ignetcfg = CtdlGetSysConfig(IGNETCFG);
	if (ignetcfg == NULL) {
		if (nexthop != NULL) {
			strcpy(nexthop, "");
		}
		return(-1);
	}

	retval = (-1);
	if (nexthop != NULL) {
		strcpy(nexthop, "");
	}

	/* Use the string tokenizer to grab one line at a time */
	for (i=0; i<num_tokens(ignetcfg, '\n'); ++i) {
		extract_token(linebuf, ignetcfg, i, '\n');
		extract(buf, linebuf, 0);
		if (!strcasecmp(buf, node)) {
			if (nexthop != NULL) {
				strcpy(nexthop, "");
			}
			if (secret != NULL) {
				extract(secret, linebuf, 1);
			}
			retval = 0;
		}
	}

	phree(ignetcfg);
	if (retval == 0) {
		return(retval);		/* yup, it's a direct neighbor */
	}

	/*	
	 * If we get to this point we have to see if we know the next hop
	 */
	if (the_netmap != NULL) {
		for (nmptr = the_netmap; nmptr != NULL; nmptr = nmptr->next) {
			if (!strcasecmp(nmptr->nodename, node)) {
				if (nexthop != NULL) {
					strcpy(nexthop, nmptr->nexthop);
				}
				return(0);
			}
		}
	}

	/*
	 * If we get to this point, the supplied node name is bogus.
	 */
	lprintf(5, "Invalid node name <%s>\n", node);
	return(-1);
}





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
	int i;
	char *instr = NULL;
	char *newpath = NULL;
	size_t instr_len = SIZ;
	struct CtdlMessage *msg = NULL;
	struct CtdlMessage *imsg;
	struct ser_ret sermsg;
	FILE *fp;
	char filename[SIZ];
	char buf[SIZ];
	int bang = 0;
	int send = 1;
	int delete_after_send = 0;	/* Set to 1 to delete after spooling */

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
			 * Determine if this message is set to be deleted
			 * after sending out on the network
			 */
			if (msg->cm_fields['S'] != NULL) {
				if (!strcasecmp(msg->cm_fields['S'],
				   "CANCEL")) {
					delete_after_send = 1;
				}
			}

			/* 
			 * Now serialize it for transmission
			 */
			serialize_message(&sermsg, msg);

			/* Now send it to every node */
			for (nptr = sc->ignet_push_shares; nptr != NULL;
			    nptr = nptr->next) {

				send = 1;

				/* Check for valid node name */
				if (is_valid_node(NULL,NULL,nptr->name) != 0) {
					lprintf(3, "Invalid node <%s>\n",
						nptr->name);
					send = 0;
				}

				/* Check for split horizon */
				lprintf(9, "Path is %s\n", msg->cm_fields['P']);
				bang = num_tokens(msg->cm_fields['P'], '!');
				if (bang > 1) for (i=0; i<(bang-1); ++i) {
					extract_token(buf, msg->cm_fields['P'],
						i, '!');
					if (!strcasecmp(buf, nptr->name)) {
						send = 0;
					}
				}

				/* Send the message */
				if (send == 1) {
					sprintf(filename,
						"./network/spoolout/%s",
						nptr->name);
					fp = fopen(filename, "ab");
					if (fp != NULL) {
						fwrite(sermsg.ser,
							sermsg.len, 1, fp);
						fclose(fp);
					}
				}
			}
			phree(sermsg.ser);
			CtdlFreeMessage(msg);
		}
	}

	/* update lastsent */
	sc->lastsent = msgnum;

	/* Delete this message if delete-after-send is set */
	if (delete_after_send) {
		CtdlDeleteMessages(CC->quickroom.QRname, msgnum, "");
	}

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
 * Learn topology from path fields
 */
void network_learn_topology(char *node, char *path) {
	char nexthop[SIZ];
	struct NetMap *nmptr;

	strcpy(nexthop, "");

	if (num_tokens(path, '!') < 3) return;
	for (nmptr = the_netmap; nmptr != NULL; nmptr = nmptr->next) {
		if (!strcasecmp(nmptr->nodename, node)) {
			extract_token(nmptr->nexthop, path, 0, '!');
			nmptr->lastcontact = time(NULL);
			return;
		}
	}

	/* If we got here then it's not in the map, so add it. */
	nmptr = (struct NetMap *) mallok(sizeof (struct NetMap));
	strcpy(nmptr->nodename, node);
	nmptr->lastcontact = time(NULL);
	extract_token(nmptr->nexthop, path, 0, '!');
	nmptr->next = the_netmap;
	the_netmap = nmptr;
}



/*
 * Process a buffer containing a single message from a single file
 * from the inbound queue 
 */
void network_process_buffer(char *buffer, long size) {
	struct CtdlMessage *msg;
	long pos;
	int field;
	int a;
	int e = MES_LOCAL;
	struct usersupp tempUS;
	char recp[SIZ];
	char target_room[ROOMNAMELEN];
	struct ser_ret sermsg;
	char *oldpath = NULL;
	char filename[SIZ];
	FILE *fp;

	/* Set default target room to trash */
	strcpy(target_room, TWITROOM);

	/* Load the message into memory */
        msg = (struct CtdlMessage *) mallok(sizeof(struct CtdlMessage));
        memset(msg, 0, sizeof(struct CtdlMessage));
        msg->cm_magic = CTDLMESSAGE_MAGIC;
        msg->cm_anon_type = buffer[1];
        msg->cm_format_type = buffer[2];

	for (pos = 3; pos < size; ++pos) {
		field = buffer[pos];
		msg->cm_fields[field] = strdoop(&buffer[pos+1]);
		pos = pos + strlen(&buffer[(int)pos]);
	}

	/* Check for message routing */
	if (msg->cm_fields['D'] != NULL) {
		if (strcasecmp(msg->cm_fields['D'], config.c_nodename)) {

			/* route the message */
			if (is_valid_node(NULL, NULL,
			   msg->cm_fields['D']) == 0) {

				/* prepend our node to the path */
				if (msg->cm_fields['P'] != NULL) {
					oldpath = msg->cm_fields['P'];
					msg->cm_fields['P'] = NULL;
				}
				else {
					oldpath = strdoop("unknown_user");
				}
				msg->cm_fields['P'] =
					mallok(strlen(oldpath) + SIZ);
				sprintf(msg->cm_fields['P'], "%s!%s",
					config.c_nodename, oldpath);
				phree(oldpath);

				/* serialize the message */
				serialize_message(&sermsg, msg);

				/* now send it */
				sprintf(filename,
					"./network/spoolout/%s",
					msg->cm_fields['D']);
				fp = fopen(filename, "ab");
				if (fp != NULL) {
					fwrite(sermsg.ser,
						sermsg.len, 1, fp);
					fclose(fp);
				}
				phree(sermsg.ser);
				CtdlFreeMessage(msg);
				return;
			}
			
			else {	/* invalid destination node name */

				/* FIXME bounce the msg */

			}
		}
	}

	/* FIXME check to see if we already have this message */

	/* Learn network topology from the path */
	if ((msg->cm_fields['N'] != NULL) && (msg->cm_fields['P'] != NULL)) {
		network_learn_topology(msg->cm_fields['N'], 
					msg->cm_fields['P']);
	}

	/* Does it have a recipient?  If so, validate it... */
	if (msg->cm_fields['R'] != NULL) {

		safestrncpy(recp, msg->cm_fields['R'], sizeof(recp));

                e = alias(recp);        /* alias and mail type */
                if ((recp[0] == 0) || (e == MES_ERROR)) {

			/* FIXME bounce the msg */

                }
                else if (e == MES_LOCAL) {
                        a = getuser(&tempUS, recp);
                        if (a != 0) {
				/* FIXME bounce the msg */
                        }
			else {
				MailboxName(target_room, &tempUS, MAILROOM);
			}
                }
        }

	else if (msg->cm_fields['C'] != NULL) {
		safestrncpy(target_room,
			msg->cm_fields['C'],
			sizeof target_room);
	}

	else if (msg->cm_fields['O'] != NULL) {
		safestrncpy(target_room,
			msg->cm_fields['O'],
			sizeof target_room);
	}

	/* save the message into a room */
	msg->cm_flags = CM_SKIP_HOOKS;
        CtdlSaveMsg(msg, "", target_room, 0);
	CtdlFreeMessage(msg);
}


/*
 * Process a single message from a single file from the inbound queue 
 */
void network_process_message(FILE *fp, long msgstart, long msgend) {
	long hold_pos;
	long size;
	char *buffer;

	hold_pos = ftell(fp);
	size = msgend - msgstart + 1;
	buffer = mallok(size);
	if (buffer != NULL) {
		fseek(fp, msgstart, SEEK_SET);
		fread(buffer, size, 1, fp);
		network_process_buffer(buffer, size);
		phree(buffer);
	}

	fseek(fp, hold_pos, SEEK_SET);
}


/*
 * Process a single file from the inbound queue 
 */
void network_process_file(char *filename) {
	FILE *fp;
	long msgstart = (-1L);
	long msgend = (-1L);
	long msgcur = 0L;
	int ch;

	lprintf(7, "network: processing <%s>\n", filename);

	fp = fopen(filename, "rb");
	if (fp == NULL) {
		lprintf(5, "Error opening %s: %s\n",
			filename, strerror(errno));
		return;
	}

	/* Look for messages in the data stream and break them out */
	while (ch = getc(fp), ch >= 0) {
	
		if (ch == 255) {
			if (msgstart >= 0L) {
				msgend = msgcur - 1;
				network_process_message(fp, msgstart, msgend);
			}
			msgstart = msgcur;
		}

		++msgcur;
	}

	msgend = msgcur - 1;
	if (msgstart >= 0L) {
		network_process_message(fp, msgstart, msgend);
	}

	fclose(fp);
	unlink(filename);
}


/*
 * Process anything in the inbound queue
 */
void network_do_spoolin(void) {
	DIR *dp;
	struct dirent *d;
	char filename[SIZ];

	dp = opendir("./network/spoolin");
	if (dp == NULL) return;

	while (d = readdir(dp), d != NULL) {
		sprintf(filename, "./network/spoolin/%s", d->d_name);
		network_process_file(filename);
	}


	closedir(dp);
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

	read_network_map();

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

	lprintf(7, "network: processing inbound queue\n");
	network_do_spoolin();

	write_network_map();

	lprintf(7, "network: queue run completed\n");
	doing_queue = 0;
}



/*
 * cmd_netp() - authenticate to the server as another Citadel node polling
 *              for network traffic
 */
void cmd_netp(char *cmdbuf)
{
	char node[SIZ];
	char pass[SIZ];

	char secret[SIZ];
	char nexthop[SIZ];

	extract(node, cmdbuf, 0);
	extract(pass, cmdbuf, 1);

	if (is_valid_node(nexthop, secret, node) != 0) {
		cprintf("%d authentication failed\n", ERROR);
		return;
	}

	if (strcasecmp(pass, secret)) {
		cprintf("%d authentication failed\n", ERROR);
		return;
	}

	safestrncpy(CC->net_node, node, sizeof CC->net_node);
	cprintf("%d authenticated as network node '%s'\n", OK,
		CC->net_node);
}



/*
 * Module entry point
 */
char *Dynamic_Module_Init(void)
{
	CtdlRegisterProtoHook(cmd_gnet, "GNET", "Get network config");
	CtdlRegisterProtoHook(cmd_snet, "SNET", "Get network config");
	CtdlRegisterProtoHook(cmd_netp, "NETP", "Identify as network poller");
	CtdlRegisterSessionHook(network_do_queue, EVT_TIMER);
	return "$Id$";
}
