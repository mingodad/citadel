/*
 * $Id$ 
 *
 * This module handles shared rooms, inter-Citadel mail, and outbound
 * mailing list processing.
 *
 * Copyright (C) 2000-2002 by Art Cancro and others.
 * This code is released under the terms of the GNU General Public License.
 *
 */

/*
 * FIXME
 * Don't allow polls during network processing
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
#include "clientsocket.h"
#include "file_ops.h"

#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif


/*
 * When we do network processing, it's accomplished in two passes; one to
 * gather a list of rooms and one to actually do them.  It's ok that rplist
 * is global; this process *only* runs as part of the housekeeping loop and
 * therefore only one will run at a time.
 */
struct RoomProcList *rplist = NULL;

/*
 * We build a map of network nodes during processing.
 */
struct NetMap *the_netmap = NULL;

/*
 * Keep track of what messages to reject
 */
struct FilterList *load_filter_list(void) {
	char *serialized_list = NULL;
	int i;
	char buf[SIZ];
	struct FilterList *newlist = NULL;
	struct FilterList *nptr;

	serialized_list = CtdlGetSysConfig(FILTERLIST);
	if (serialized_list == NULL) return(NULL); /* if null, no entries */

	/* Use the string tokenizer to grab one line at a time */
	for (i=0; i<num_tokens(serialized_list, '\n'); ++i) {
		extract_token(buf, serialized_list, i, '\n');
		nptr = (struct FilterList *) mallok(sizeof(struct FilterList));
		extract(nptr->fl_user, buf, 0);
		striplt(nptr->fl_user);
		extract(nptr->fl_room, buf, 1);
		striplt(nptr->fl_room);
		extract(nptr->fl_node, buf, 2);
		striplt(nptr->fl_node);

		/* Cowardly refuse to add an any/any/any entry that would
		 * end up filtering every single message.
		 */
		if (strlen(nptr->fl_user) + strlen(nptr->fl_room)
		   + strlen(nptr->fl_node) == 0) {
			phree(nptr);
		}
		else {
			nptr->next = newlist;
			newlist = nptr;
		}
	}

	phree(serialized_list);
	return newlist;
}


void free_filter_list(struct FilterList *fl) {
	if (fl == NULL) return;
	free_filter_list(fl->next);
	phree(fl);
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
	if (strlen(msg->cm_fields['I']) == 0) {
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
		return(1);
	}

	/* If we got to this point, it's unique: add it. */
	strcpy(ut.ut_msgid, msgid);
	ut.ut_timestamp = time(NULL);
	cdb_store(CDB_USETABLE, msgid, strlen(msgid),
		&ut, sizeof(struct UseTable) );
	return(0);
}


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
				snprintf(&serialized_map[strlen(serialized_map)],
					SIZ,
					"%s|%ld|%s\n",
					nmptr->nodename,
					(long)nmptr->lastcontact,
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
	assoc_file_name(filename, sizeof filename, &CC->quickroom, "netconfigs");
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
	assoc_file_name(filename, sizeof filename, &CC->quickroom, "netconfigs");

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
			lprintf(1, "Cannot allocate %ld bytes for instr...\n",
				(long)instr_len);
			abort();
		}
		snprintf(instr, instr_len,
			"Content-type: %s\n\nmsgid|%ld\nsubmitted|%ld\n"
			"bounceto|postmaster@%s\n" ,
			SPOOLMIME, msgnum, (long)time(NULL), config.c_fqdn );
	
		/* Generate delivery instructions for each recipient */
		for (nptr = sc->listrecps; nptr != NULL; nptr = nptr->next) {
			size_t tmp = strlen(instr);
			snprintf(&instr[tmp], instr_len - tmp,
				 "remote|%s|0||\n", nptr->name);
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
		CtdlSubmitMsg(imsg, NULL, SMTP_SPOOLOUT_ROOM);
		CtdlFreeMessage(imsg);
	}
	
	/*
	 * Process IGnet push shares
	 */
	if (sc->ignet_push_shares != NULL) {
	
		msg = CtdlFetchMessage(msgnum);
		if (msg != NULL) {
			size_t newpath_len;

			/* Prepend our node name to the Path field whenever
			 * sending a message to another IGnet node
			 */
			if (msg->cm_fields['P'] == NULL) {
				msg->cm_fields['P'] = strdoop("username");
			}
			newpath_len = strlen(msg->cm_fields['P']) +
				 strlen(config.c_nodename) + 2;
			newpath = mallok(newpath_len);
			snprintf(newpath, newpath_len, "%s!%s",
				 config.c_nodename, msg->cm_fields['P']);
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
					snprintf(filename, sizeof filename,
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
	assoc_file_name(filename, sizeof filename, &CC->quickroom, "netconfigs");

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

	lprintf(9, "entering network_bounce()\n");

	if (msg == NULL) return;

	snprintf(bouncesource, sizeof bouncesource, "%s@%s", BOUNCESOURCE, config.c_nodename);

	/* 
	 * Give it a fresh message ID
	 */
	if (msg->cm_fields['I'] != NULL) {
		phree(msg->cm_fields['I']);
	}
	snprintf(buf, sizeof buf, "%ld.%04lx.%04x@%s",
		(long)time(NULL), (long)getpid(), ++serialnum, config.c_fqdn);
	msg->cm_fields['I'] = strdoop(buf);

	/*
	 * FIXME ... right now we're just sending a bounce; we really want to
	 * include the text of the bounced message.
	 */
	if (msg->cm_fields['M'] != NULL) {
		phree(msg->cm_fields['M']);
	}
	msg->cm_fields['M'] = strdoop(reason);
	msg->cm_format_type = 0;

	/*
	 * Turn the message around
	 */
	if (msg->cm_fields['R'] == NULL) {
		phree(msg->cm_fields['R']);
	}

	if (msg->cm_fields['D'] == NULL) {
		phree(msg->cm_fields['D']);
	}

	snprintf(recipient, sizeof recipient, "%s@%s",
		msg->cm_fields['A'], msg->cm_fields['N']);

	if (msg->cm_fields['A'] == NULL) {
		phree(msg->cm_fields['A']);
	}

	if (msg->cm_fields['N'] == NULL) {
		phree(msg->cm_fields['N']);
	}

	msg->cm_fields['A'] = strdoop(BOUNCESOURCE);
	msg->cm_fields['N'] = strdoop(config.c_nodename);
	

	/* prepend our node to the path */
	if (msg->cm_fields['P'] != NULL) {
		oldpath = msg->cm_fields['P'];
		msg->cm_fields['P'] = NULL;
	}
	else {
		oldpath = strdoop("unknown_user");
	}
	size = strlen(oldpath) + SIZ;
	msg->cm_fields['P'] = mallok(size);
	snprintf(msg->cm_fields['P'], size, "%s!%s", config.c_nodename, oldpath);
	phree(oldpath);

	/* Now submit the message */
	valid = validate_recipients(recipient);
	if (valid != NULL) if (valid->num_error > 0) {
		phree(valid);
		valid = NULL;
	}
	if ( (valid == NULL) || (!strcasecmp(recipient, bouncesource)) ) {
		strcpy(force_room, AIDEROOM);
	}
	else {
		strcpy(force_room, "");
	}
	if ( (valid == NULL) && (strlen(force_room) == 0) ) {
		strcpy(force_room, AIDEROOM);
	}
	CtdlSubmitMsg(msg, valid, force_room);

	/* Clean up */
	if (valid != NULL) phree(valid);
	CtdlFreeMessage(msg);
	lprintf(9, "leaving network_bounce()\n");
}




/*
 * Process a buffer containing a single message from a single file
 * from the inbound queue 
 */
void network_process_buffer(char *buffer, long size) {
	struct CtdlMessage *msg;
	long pos;
	int field;
	struct recptypes *recp = NULL;
	char target_room[ROOMNAMELEN];
	struct ser_ret sermsg;
	char *oldpath = NULL;
	char filename[SIZ];
	FILE *fp;
	char buf[SIZ];

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
				size = strlen(oldpath) + SIZ;
				msg->cm_fields['P'] = mallok(size);
				snprintf(msg->cm_fields['P'], size, "%s!%s",
					config.c_nodename, oldpath);
				phree(oldpath);

				/* serialize the message */
				serialize_message(&sermsg, msg);

				/* now send it */
				snprintf(filename, sizeof filename,
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

				network_bounce(msg,
"A message you sent could not be delivered due to an invalid destination node"
" name.  Please check the address and try sending the message again.\n");
				msg = NULL;
				return;

			}
		}
	}

	/*
	 * Check to see if we already have a copy of this message
	 */
	if (network_usetable(msg) != 0) {
		snprintf(buf, sizeof buf,
			"Loopzapper rejected message <%s> "
			"from <%s> in <%s> @ <%s>\n",
			((msg->cm_fields['I']!=NULL)?(msg->cm_fields['I']):""),
			((msg->cm_fields['A']!=NULL)?(msg->cm_fields['A']):""),
			((msg->cm_fields['O']!=NULL)?(msg->cm_fields['O']):""),
			((msg->cm_fields['N']!=NULL)?(msg->cm_fields['N']):"")
		);
		aide_message(buf);
		CtdlFreeMessage(msg);
		msg = NULL;
		return;
	}

	/* Learn network topology from the path */
	if ((msg->cm_fields['N'] != NULL) && (msg->cm_fields['P'] != NULL)) {
		network_learn_topology(msg->cm_fields['N'], 
					msg->cm_fields['P']);
	}

	/* Does it have a recipient?  If so, validate it... */
	if (msg->cm_fields['R'] != NULL) {
		recp = validate_recipients(msg->cm_fields['R']);
		if (recp != NULL) if (recp->num_error > 0) {
			network_bounce(msg,
"A message you sent could not be delivered due to an invalid address.\n"
"Please check the address and try sending the message again.\n");
			msg = NULL;
			phree(recp);
			return;
                }
		strcpy(target_room, "");	/* no target room if mail */
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

	/* Strip out fields that are only relevant during transit */
	if (msg->cm_fields['D'] != NULL) {
		phree(msg->cm_fields['D']);
		msg->cm_fields['D'] = NULL;
	}
	if (msg->cm_fields['C'] != NULL) {
		phree(msg->cm_fields['C']);
		msg->cm_fields['C'] = NULL;
	}

	/* save the message into a room */
	if (PerformNetprocHooks(msg, target_room) == 0) {
		msg->cm_flags = CM_SKIP_HOOKS;
        	CtdlSubmitMsg(msg, recp, target_room);
	}
	CtdlFreeMessage(msg);
	phree(recp);
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
		snprintf(filename, sizeof filename, "./network/spoolin/%s", d->d_name);
		network_process_file(filename);
	}


	closedir(dp);
}





/*
 * receive network spool from the remote system
 */
void receive_spool(int sock, char *remote_nodename) {
	long download_len;
	long bytes_received;
	char buf[SIZ];
	static char pbuf[IGNET_PACKET_SIZE];
	char tempfilename[PATH_MAX];
	long plen;
	FILE *fp;

	strcpy(tempfilename, tmpnam(NULL));
	if (sock_puts(sock, "NDOP") < 0) return;
	if (sock_gets(sock, buf) < 0) return;
	lprintf(9, "<%s\n", buf);
	if (buf[0] != '2') {
		return;
	}
	download_len = extract_long(&buf[4], 0);

	bytes_received = 0L;
	fp = fopen(tempfilename, "w");
	if (fp == NULL) {
		lprintf(9, "cannot open download file locally: %s\n",
			strerror(errno));
		return;
	}

	while (bytes_received < download_len) {
		snprintf(buf, sizeof buf, "READ %ld|%ld",
			bytes_received,
		     ((download_len - bytes_received > IGNET_PACKET_SIZE)
		 ? IGNET_PACKET_SIZE : (download_len - bytes_received)));
		if (sock_puts(sock, buf) < 0) {
			fclose(fp);
			unlink(tempfilename);
			return;
		}
		if (sock_gets(sock, buf) < 0) {
			fclose(fp);
			unlink(tempfilename);
			return;
		}
		if (buf[0] == '6') {
			plen = extract_long(&buf[4], 0);
			if (sock_read(sock, pbuf, plen) < 0) {
				fclose(fp);
				unlink(tempfilename);
				return;
			}
			fwrite((char *) pbuf, plen, 1, fp);
			bytes_received = bytes_received + plen;
		}
	}

	fclose(fp);
	if (sock_puts(sock, "CLOS") < 0) {
		unlink(tempfilename);
		return;
	}
	if (sock_gets(sock, buf) < 0) {
		unlink(tempfilename);
		return;
	}
	lprintf(9, "%s\n", buf);
	snprintf(buf, sizeof buf, "mv %s ./network/spoolin/%s.%ld",
		tempfilename, remote_nodename, (long) getpid());
	system(buf);
}



/*
 * transmit network spool to the remote system
 */
void transmit_spool(int sock, char *remote_nodename)
{
	char buf[SIZ];
	char pbuf[4096];
	long plen;
	long bytes_to_write, thisblock;
	int fd;
	char sfname[128];

	if (sock_puts(sock, "NUOP") < 0) return;
	if (sock_gets(sock, buf) < 0) return;
	lprintf(9, "<%s\n", buf);
	if (buf[0] != '2') {
		return;
	}

	snprintf(sfname, sizeof sfname, "./network/spoolout/%s", remote_nodename);
	fd = open(sfname, O_RDONLY);
	if (fd < 0) {
		if (errno == ENOENT) {
			lprintf(9, "Nothing to send.\n");
		} else {
			lprintf(5, "cannot open upload file locally: %s\n",
				strerror(errno));
		}
		return;
	}
	while (plen = (long) read(fd, pbuf, IGNET_PACKET_SIZE), plen > 0L) {
		bytes_to_write = plen;
		while (bytes_to_write > 0L) {
			snprintf(buf, sizeof buf, "WRIT %ld", bytes_to_write);
			if (sock_puts(sock, buf) < 0) {
				close(fd);
				return;
			}
			if (sock_gets(sock, buf) < 0) {
				close(fd);
				return;
			}
			thisblock = atol(&buf[4]);
			if (buf[0] == '7') {
				if (sock_write(sock, pbuf,
				   (int) thisblock) < 0) {
					close(fd);
					return;
				}
				bytes_to_write = bytes_to_write - thisblock;
			} else {
				goto ABORTUPL;
			}
		}
	}

ABORTUPL:
	close(fd);
	if (sock_puts(sock, "UCLS 1") < 0) return;
	if (sock_gets(sock, buf) < 0) return;
	lprintf(9, "<%s\n", buf);
	if (buf[0] == '2') {
		unlink(sfname);
	}
}



/*
 * Poll one Citadel node (called by network_poll_other_citadel_nodes() below)
 */
void network_poll_node(char *node, char *secret, char *host, char *port) {
	int sock;
	char buf[SIZ];

	if (network_talking_to(node, NTT_CHECK)) return;
	network_talking_to(node, NTT_ADD);
	lprintf(5, "Polling node <%s> at %s:%s\n", node, host, port);

	sock = sock_connect(host, port, "tcp");
	if (sock < 0) {
		lprintf(7, "Could not connect: %s\n", strerror(errno));
		network_talking_to(node, NTT_REMOVE);
		return;
	}
	
	lprintf(9, "Connected!\n");

	/* Read the server greeting */
	if (sock_gets(sock, buf) < 0) goto bail;
	lprintf(9, ">%s\n", buf);

	/* Identify ourselves */
	snprintf(buf, sizeof buf, "NETP %s|%s", config.c_nodename, secret);
	lprintf(9, "<%s\n", buf);
	if (sock_puts(sock, buf) <0) goto bail;
	if (sock_gets(sock, buf) < 0) goto bail;
	lprintf(9, ">%s\n", buf);
	if (buf[0] != '2') goto bail;

	/* At this point we are authenticated. */
	receive_spool(sock, node);
	transmit_spool(sock, node);

	sock_puts(sock, "QUIT");
bail:	sock_close(sock);
	network_talking_to(node, NTT_REMOVE);
}



/*
 * Poll other Citadel nodes and transfer inbound/outbound network data.
 */
void network_poll_other_citadel_nodes(void) {
	char *ignetcfg = NULL;
	int i;
	char linebuf[SIZ];
	char node[SIZ];
	char host[SIZ];
	char port[SIZ];
	char secret[SIZ];

	ignetcfg = CtdlGetSysConfig(IGNETCFG);
	if (ignetcfg == NULL) return; 	/* no nodes defined */

	/* Use the string tokenizer to grab one line at a time */
	for (i=0; i<num_tokens(ignetcfg, '\n'); ++i) {
		extract_token(linebuf, ignetcfg, i, '\n');
		extract(node, linebuf, 0);
		extract(secret, linebuf, 1);
		extract(host, linebuf, 2);
		extract(port, linebuf, 3);
		if ( (strlen(node) > 0) && (strlen(secret) > 0) 
		   && (strlen(host) > 0) && strlen(port) > 0) {
			network_poll_node(node, secret, host, port);
		}
	}

	phree(ignetcfg);
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
	if ( (time(NULL) - last_run) < config.c_net_freq ) return;

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
	 * Poll other Citadel nodes.
	 */
	network_poll_other_citadel_nodes();

	/*
	 * Load the network map and filter list into memory.
	 */
	read_network_map();
	filterlist = load_filter_list();

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

	/* Save the network map back to disk */
	write_network_map();

	/* Free the filter list in memory */
	free_filter_list(filterlist);
	filterlist = NULL;

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

	if (network_talking_to(node, NTT_CHECK)) {
		cprintf("%d Already talking to %s right now\n", ERROR, node);
		return;
	}

	safestrncpy(CC->net_node, node, sizeof CC->net_node);
	network_talking_to(node, NTT_ADD);
	cprintf("%d authenticated as network node '%s'\n", OK,
		CC->net_node);
}





/*
 * Module entry point
 */
char *Dynamic_Module_Init(void)
{
	CtdlRegisterProtoHook(cmd_gnet, "GNET", "Get network config");
	CtdlRegisterProtoHook(cmd_snet, "SNET", "Set network config");
	CtdlRegisterProtoHook(cmd_netp, "NETP", "Identify as network poller");
	CtdlRegisterSessionHook(network_do_queue, EVT_TIMER);
	return "$Id$";
}
