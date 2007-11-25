/*
 * $Id$ 
 *
 * This module handles shared rooms, inter-Citadel mail, and outbound
 * mailing list processing.
 *
 * Copyright (C) 2000-2005 by Art Cancro and others.
 * This code is released under the terms of the GNU General Public License.
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

#include <sys/wait.h>
#include <string.h>
#include <limits.h>
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "room_ops.h"
#include "user_ops.h"
#include "policy.h"
#include "database.h"
#include "msgbase.h"
#include "internet_addressing.h"
#include "serv_network.h"
#include "clientsocket.h"
#include "file_ops.h"
#include "citadel_dirs.h"

#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif


#include "ctdl_module.h"



/* Nonzero while we are doing network processing */
static int doing_queue = 0;

/*
 * When we do network processing, it's accomplished in two passes; one to
 * gather a list of rooms and one to actually do them.  It's ok that rplist
 * is global; we have a mutex that keeps it safe.
 */
struct RoomProcList *rplist = NULL;

/*
 * We build a map of network nodes during processing.
 */
struct NetMap *the_netmap = NULL;
int netmap_changed = 0;
char *working_ignetcfg = NULL;

/*
 * Load or refresh the Citadel network (IGnet) configuration for this node.
 */
void load_working_ignetcfg(void) {
	char *cfg;
	char *oldcfg;

	cfg = CtdlGetSysConfig(IGNETCFG);
	if (cfg == NULL) {
		cfg = strdup("");
	}

	oldcfg = working_ignetcfg;
	working_ignetcfg = cfg;
	if (oldcfg != NULL) {
		free(oldcfg);
	}
}





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
		extract_token(buf, serialized_list, i, '\n', sizeof buf);
		nptr = (struct FilterList *) malloc(sizeof(struct FilterList));
		extract_token(nptr->fl_user, buf, 0, '|', sizeof nptr->fl_user);
		striplt(nptr->fl_user);
		extract_token(nptr->fl_room, buf, 1, '|', sizeof nptr->fl_room);
		striplt(nptr->fl_room);
		extract_token(nptr->fl_node, buf, 2, '|', sizeof nptr->fl_node);
		striplt(nptr->fl_node);

		/* Cowardly refuse to add an any/any/any entry that would
		 * end up filtering every single message.
		 */
		if (IsEmptyStr(nptr->fl_user) && 
		    IsEmptyStr(nptr->fl_room) &&
		    IsEmptyStr(nptr->fl_node)) {
			free(nptr);
		}
		else {
			nptr->next = newlist;
			newlist = nptr;
		}
	}

	free(serialized_list);
	return newlist;
}


void free_filter_list(struct FilterList *fl) {
	if (fl == NULL) return;
	free_filter_list(fl->next);
	free(fl);
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
		extract_token(buf, serialized_map, i, '\n', sizeof buf);
		nmptr = (struct NetMap *) malloc(sizeof(struct NetMap));
		extract_token(nmptr->nodename, buf, 0, '|', sizeof nmptr->nodename);
		nmptr->lastcontact = extract_long(buf, 1);
		extract_token(nmptr->nexthop, buf, 2, '|', sizeof nmptr->nexthop);
		nmptr->next = the_netmap;
		the_netmap = nmptr;
	}

	free(serialized_map);
	netmap_changed = 0;
}


/*
 * Write the network map from memory back to the configuration file.
 */
void write_network_map(void) {
	char *serialized_map = NULL;
	struct NetMap *nmptr;


	if (netmap_changed) {
		serialized_map = strdup("");
	
		if (the_netmap != NULL) {
			for (nmptr = the_netmap; nmptr != NULL; nmptr = nmptr->next) {
				serialized_map = realloc(serialized_map,
							(strlen(serialized_map)+SIZ) );
				if (!IsEmptyStr(nmptr->nodename)) {
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
		free(serialized_map);
	}

	/* Now free the list */
	while (the_netmap != NULL) {
		nmptr = the_netmap->next;
		free(the_netmap);
		the_netmap = nmptr;
	}
	netmap_changed = 0;
}



/* 
 * Check the network map and determine whether the supplied node name is
 * valid.  If it is not a neighbor node, supply the name of a neighbor node
 * which is the next hop.  If it *is* a neighbor node, we also fill in the
 * shared secret.
 */
int is_valid_node(char *nexthop, char *secret, char *node) {
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
	if (working_ignetcfg == NULL) {
		lprintf(CTDL_ERR, "working_ignetcfg is NULL!\n");
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
	for (i=0; i<num_tokens(working_ignetcfg, '\n'); ++i) {
		extract_token(linebuf, working_ignetcfg, i, '\n', sizeof linebuf);
		extract_token(buf, linebuf, 0, '|', sizeof buf);
		if (!strcasecmp(buf, node)) {
			if (nexthop != NULL) {
				strcpy(nexthop, "");
			}
			if (secret != NULL) {
				extract_token(secret, linebuf, 1, '|', 256);
			}
			retval = 0;
		}
	}

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
	lprintf(CTDL_ERR, "Invalid node name <%s>\n", node);
	return(-1);
}





void cmd_gnet(char *argbuf) {
	char filename[SIZ];
	char buf[SIZ];
	FILE *fp;

	if ( (CC->room.QRflags & QR_MAILBOX) && (CC->user.usernum == atol(CC->room.QRname)) ) {
		/* users can edit the netconfigs for their own mailbox rooms */
	}
	else if (CtdlAccessCheck(ac_room_aide)) return;

	assoc_file_name(filename, sizeof filename, &CC->room, ctdl_netcfg_dir);
	cprintf("%d Network settings for room #%ld <%s>\n",
		LISTING_FOLLOWS,
		CC->room.QRnumber, CC->room.QRname);

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
	FILE *fp, *newfp;

	unbuffer_output();

	if ( (CC->room.QRflags & QR_MAILBOX) && (CC->user.usernum == atol(CC->room.QRname)) ) {
		/* users can edit the netconfigs for their own mailbox rooms */
	}
	else if (CtdlAccessCheck(ac_room_aide)) return;

	CtdlMakeTempFileName(tempfilename, sizeof tempfilename);
	assoc_file_name(filename, sizeof filename, &CC->room, ctdl_netcfg_dir);

	fp = fopen(tempfilename, "w");
	if (fp == NULL) {
		cprintf("%d Cannot open %s: %s\n",
			ERROR + INTERNAL_ERROR,
			tempfilename,
			strerror(errno));
	}

	cprintf("%d %s\n", SEND_LISTING, tempfilename);
	while (client_getln(buf, sizeof buf), strcmp(buf, "000")) {
		fprintf(fp, "%s\n", buf);
	}
	fclose(fp);

	/* Now copy the temp file to its permanent location.
	 * (We copy instead of link because they may be on different filesystems)
	 */
	begin_critical_section(S_NETCONFIGS);
	fp = fopen(tempfilename, "r");
	if (fp != NULL) {
		newfp = fopen(filename, "w");
		if (newfp != NULL) {
			while (fgets(buf, sizeof buf, fp) != NULL) {
				fprintf(newfp, "%s", buf);
			}
			fclose(newfp);
		}
		fclose(fp);
	}
	end_critical_section(S_NETCONFIGS);
	unlink(tempfilename);
}


/*
 * Deliver digest messages
 */
void network_deliver_digest(struct SpoolControl *sc) {
	char buf[SIZ];
	int i;
	struct CtdlMessage *msg = NULL;
	long msglen;
	char *recps = NULL;
	size_t recps_len = SIZ;
	struct recptypes *valid;
	struct namelist *nptr;

	if (sc->num_msgs_spooled < 1) {
		fclose(sc->digestfp);
		sc->digestfp = NULL;
		return;
	}

	msg = malloc(sizeof(struct CtdlMessage));
	memset(msg, 0, sizeof(struct CtdlMessage));
	msg->cm_magic = CTDLMESSAGE_MAGIC;
	msg->cm_format_type = FMT_RFC822;
	msg->cm_anon_type = MES_NORMAL;

	sprintf(buf, "%ld", time(NULL));
	msg->cm_fields['T'] = strdup(buf);
	msg->cm_fields['A'] = strdup(CC->room.QRname);
	snprintf(buf, sizeof buf, "[%s]", CC->room.QRname);
	msg->cm_fields['U'] = strdup(buf);
	sprintf(buf, "room_%s@%s", CC->room.QRname, config.c_fqdn);
	for (i=0; buf[i]; ++i) {
		if (isspace(buf[i])) buf[i]='_';
		buf[i] = tolower(buf[i]);
	}
	msg->cm_fields['F'] = strdup(buf);
	msg->cm_fields['R'] = strdup(buf);

	/*
	 * Go fetch the contents of the digest
	 */
	fseek(sc->digestfp, 0L, SEEK_END);
	msglen = ftell(sc->digestfp);

	msg->cm_fields['M'] = malloc(msglen + 1);
	fseek(sc->digestfp, 0L, SEEK_SET);
	fread(msg->cm_fields['M'], (size_t)msglen, 1, sc->digestfp);
	msg->cm_fields['M'][msglen] = 0;

	fclose(sc->digestfp);
	sc->digestfp = NULL;

	/* Now generate the delivery instructions */

	/* 
	 * Figure out how big a buffer we need to allocate
	 */
	for (nptr = sc->digestrecps; nptr != NULL; nptr = nptr->next) {
		recps_len = recps_len + strlen(nptr->name) + 2;
	}
	
	recps = malloc(recps_len);

	if (recps == NULL) {
		lprintf(CTDL_EMERG, "Cannot allocate %ld bytes for recps...\n", (long)recps_len);
		abort();
	}

	strcpy(recps, "");

	/* Each recipient */
	for (nptr = sc->digestrecps; nptr != NULL; nptr = nptr->next) {
		if (nptr != sc->digestrecps) {
			strcat(recps, ",");
		}
		strcat(recps, nptr->name);
	}

	/* Now submit the message */
	valid = validate_recipients(recps);
	free(recps);
	CtdlSubmitMsg(msg, valid, NULL);
	CtdlFreeMessage(msg);
	free_recipients(valid);
}


/*
 * Deliver list messages to everyone on the list ... efficiently
 */
void network_deliver_list(struct CtdlMessage *msg, struct SpoolControl *sc) {
	char *recps = NULL;
	size_t recps_len = SIZ;
	struct recptypes *valid;
	struct namelist *nptr;

	/* Don't do this if there were no recipients! */
	if (sc->listrecps == NULL) return;

	/* Now generate the delivery instructions */

	/* 
	 * Figure out how big a buffer we need to allocate
	 */
	for (nptr = sc->listrecps; nptr != NULL; nptr = nptr->next) {
		recps_len = recps_len + strlen(nptr->name) + 2;
	}
	
	recps = malloc(recps_len);

	if (recps == NULL) {
		lprintf(CTDL_EMERG, "Cannot allocate %ld bytes for recps...\n", (long)recps_len);
		abort();
	}

	strcpy(recps, "");

	/* Each recipient */
	for (nptr = sc->listrecps; nptr != NULL; nptr = nptr->next) {
		if (nptr != sc->listrecps) {
			strcat(recps, ",");
		}
		strcat(recps, nptr->name);
	}

	/* Now submit the message */
	valid = validate_recipients(recps);
	free(recps);
	CtdlSubmitMsg(msg, valid, NULL);
	free_recipients(valid);
	/* Do not call CtdlFreeMessage(msg) here; the caller will free it. */
}




/*
 * Spools out one message from the list.
 */
void network_spool_msg(long msgnum, void *userdata) {
	struct SpoolControl *sc;
	int i;
	char *newpath = NULL;
	size_t instr_len = SIZ;
	struct CtdlMessage *msg = NULL;
	struct namelist *nptr;
	struct maplist *mptr;
	struct ser_ret sermsg;
	FILE *fp;
	char filename[SIZ];
	char buf[SIZ];
	int bang = 0;
	int send = 1;
	int delete_after_send = 0;	/* Set to 1 to delete after spooling */
	int ok_to_participate = 0;
	struct recptypes *valid;

	sc = (struct SpoolControl *)userdata;

	/*
	 * Process mailing list recipients
	 */
	instr_len = SIZ;
	if (sc->listrecps != NULL) {
		/* Fetch the message.  We're going to need to modify it
		 * in order to insert the [list name] in it, etc.
		 */
		msg = CtdlFetchMessage(msgnum, 1);
		if (msg != NULL) {

			/* Prepend "[List name]" to the subject */
			if (msg->cm_fields['U'] == NULL) {
				msg->cm_fields['U'] = strdup("(no subject)");
			}
			snprintf(buf, sizeof buf, "[%s] %s", CC->room.QRname, msg->cm_fields['U']);
			free(msg->cm_fields['U']);
			msg->cm_fields['U'] = strdup(buf);

			/* Set the recipient of the list message to the
			 * email address of the room itself.
			 * FIXME ... I want to be able to pick any address
			 */
			if (msg->cm_fields['R'] != NULL) {
				free(msg->cm_fields['R']);
			}
			msg->cm_fields['R'] = malloc(256);
			snprintf(msg->cm_fields['R'], 256,
				"room_%s@%s", CC->room.QRname,
				config.c_fqdn);
			for (i=0; msg->cm_fields['R'][i]; ++i) {
				if (isspace(msg->cm_fields['R'][i])) {
					msg->cm_fields['R'][i] = '_';
				}
			}

			/* Handle delivery */
			network_deliver_list(msg, sc);
			CtdlFreeMessage(msg);
		}
	}

	/*
	 * Process digest recipients
	 */
	if ((sc->digestrecps != NULL) && (sc->digestfp != NULL)) {
		msg = CtdlFetchMessage(msgnum, 1);
		if (msg != NULL) {
			fprintf(sc->digestfp,	" -----------------------------------"
						"------------------------------------"
						"-------\n");
			fprintf(sc->digestfp, "From: ");
			if (msg->cm_fields['A'] != NULL) {
				fprintf(sc->digestfp, "%s ", msg->cm_fields['A']);
			}
			if (msg->cm_fields['F'] != NULL) {
				fprintf(sc->digestfp, "<%s> ", msg->cm_fields['F']);
			}
			else if (msg->cm_fields['N'] != NULL) {
				fprintf(sc->digestfp, "@%s ", msg->cm_fields['N']);
			}
			fprintf(sc->digestfp, "\n");
			if (msg->cm_fields['U'] != NULL) {
				fprintf(sc->digestfp, "Subject: %s\n", msg->cm_fields['U']);
			}

			CC->redirect_buffer = malloc(SIZ);
			CC->redirect_len = 0;
			CC->redirect_alloc = SIZ;

			safestrncpy(CC->preferred_formats, "text/plain", sizeof CC->preferred_formats);
			CtdlOutputPreLoadedMsg(msg, MT_CITADEL, HEADERS_NONE, 0, 0);

			striplt(CC->redirect_buffer);
			fprintf(sc->digestfp, "\n%s\n", CC->redirect_buffer);

			free(CC->redirect_buffer);
			CC->redirect_buffer = NULL;
			CC->redirect_len = 0;
			CC->redirect_alloc = 0;

			sc->num_msgs_spooled += 1;
			free(msg);
		}
	}

	/*
	 * Process client-side list participations for this room
	 */
	instr_len = SIZ;
	if (sc->participates != NULL) {
		msg = CtdlFetchMessage(msgnum, 1);
		if (msg != NULL) {

			/* Only send messages which originated on our own Citadel
			 * network, otherwise we'll end up sending the remote
			 * mailing list's messages back to it, which is rude...
			 */
			ok_to_participate = 0;
			if (msg->cm_fields['N'] != NULL) {
				if (!strcasecmp(msg->cm_fields['N'], config.c_nodename)) {
					ok_to_participate = 1;
				}
				if (is_valid_node(NULL, NULL, msg->cm_fields['N']) == 0) {
					ok_to_participate = 1;
				}
			}
			if (ok_to_participate) {
				if (msg->cm_fields['F'] != NULL) {
					free(msg->cm_fields['F']);
				}
				msg->cm_fields['F'] = malloc(SIZ);
				/* Replace the Internet email address of the actual
			 	* author with the email address of the room itself,
			 	* so the remote listserv doesn't reject us.
			 	* FIXME ... I want to be able to pick any address
			 	*/
				snprintf(msg->cm_fields['F'], SIZ,
					"room_%s@%s", CC->room.QRname,
					config.c_fqdn);
				for (i=0; msg->cm_fields['F'][i]; ++i) {
					if (isspace(msg->cm_fields['F'][i])) {
						msg->cm_fields['F'][i] = '_';
					}
				}

				/* 
				 * Figure out how big a buffer we need to allocate
			 	 */
				for (nptr = sc->participates; nptr != NULL; nptr = nptr->next) {

					if (msg->cm_fields['R'] == NULL) {
						free(msg->cm_fields['R']);
					}
					msg->cm_fields['R'] = strdup(nptr->name);
	
					valid = validate_recipients(nptr->name);
					CtdlSubmitMsg(msg, valid, "");
					free_recipients(valid);
				}
			
			}
			CtdlFreeMessage(msg);
		}
	}
	
	/*
	 * Process IGnet push shares
	 */
	msg = CtdlFetchMessage(msgnum, 1);
	if (msg != NULL) {
		size_t newpath_len;

		/* Prepend our node name to the Path field whenever
		 * sending a message to another IGnet node
		 */
		if (msg->cm_fields['P'] == NULL) {
			msg->cm_fields['P'] = strdup("username");
		}
		newpath_len = strlen(msg->cm_fields['P']) +
			 strlen(config.c_nodename) + 2;
		newpath = malloc(newpath_len);
		snprintf(newpath, newpath_len, "%s!%s",
			 config.c_nodename, msg->cm_fields['P']);
		free(msg->cm_fields['P']);
		msg->cm_fields['P'] = newpath;

		/*
		 * Determine if this message is set to be deleted
		 * after sending out on the network
		 */
		if (msg->cm_fields['S'] != NULL) {
			if (!strcasecmp(msg->cm_fields['S'], "CANCEL")) {
				delete_after_send = 1;
			}
		}

		/* Now send it to every node */
		if (sc->ignet_push_shares != NULL)
		  for (mptr = sc->ignet_push_shares; mptr != NULL;
		    mptr = mptr->next) {

			send = 1;

			/* Check for valid node name */
			if (is_valid_node(NULL, NULL, mptr->remote_nodename) != 0) {
				lprintf(CTDL_ERR, "Invalid node <%s>\n",
					mptr->remote_nodename);
				send = 0;
			}

			/* Check for split horizon */
			lprintf(CTDL_DEBUG, "Path is %s\n", msg->cm_fields['P']);
			bang = num_tokens(msg->cm_fields['P'], '!');
			if (bang > 1) for (i=0; i<(bang-1); ++i) {
				extract_token(buf, msg->cm_fields['P'],
					i, '!', sizeof buf);
				if (!strcasecmp(buf, mptr->remote_nodename)) {
					send = 0;
				}
			}

			/* Send the message */
			if (send == 1) {

				/*
				 * Force the message to appear in the correct room
				 * on the far end by setting the C field correctly
				 */
				if (msg->cm_fields['C'] != NULL) {
					free(msg->cm_fields['C']);
				}
				if (!IsEmptyStr(mptr->remote_roomname)) {
					msg->cm_fields['C'] = strdup(mptr->remote_roomname);
				}
				else {
					msg->cm_fields['C'] = strdup(CC->room.QRname);
				}

				/* serialize it for transmission */
				serialize_message(&sermsg, msg);
				if (sermsg.len > 0) {

					/* write it to the spool file */
					snprintf(filename, sizeof filename,"%s/%s",
						 	ctdl_netout_dir,
						 	mptr->remote_nodename);
					lprintf(CTDL_DEBUG, "Appending to %s\n", filename);
					fp = fopen(filename, "ab");
					if (fp != NULL) {
						fwrite(sermsg.ser,
							sermsg.len, 1, fp);
						fclose(fp);
					}
					else {
						lprintf(CTDL_ERR, "%s: %s\n", filename, strerror(errno));
					}
	
					/* free the serialized version */
					free(sermsg.ser);
				}

			}
		}
		CtdlFreeMessage(msg);
	}

	/* update lastsent */
	sc->lastsent = msgnum;

	/* Delete this message if delete-after-send is set */
	if (delete_after_send) {
		CtdlDeleteMessages(CC->room.QRname, &msgnum, 1, "");
	}

}
	

/*
 * Batch up and send all outbound traffic from the current room
 */
void network_spoolout_room(char *room_to_spool) {
	char filename[SIZ];
	char buf[SIZ];
	char instr[SIZ];
	char nodename[256];
	char roomname[ROOMNAMELEN];
	char nexthop[256];
	FILE *fp;
	struct SpoolControl sc;
	struct namelist *nptr = NULL;
	struct maplist *mptr = NULL;
	size_t miscsize = 0;
	size_t linesize = 0;
	int skipthisline = 0;
	int i;

	/*
	 * If the room doesn't exist, don't try to perform its networking tasks.
	 * Normally this should never happen, but once in a while maybe a room gets
	 * queued for networking and then deleted before it can happen.
	 */
	if (getroom(&CC->room, room_to_spool) != 0) {
		lprintf(CTDL_CRIT, "ERROR: cannot load <%s>\n", room_to_spool);
		return;
	}

	memset(&sc, 0, sizeof(struct SpoolControl));
	assoc_file_name(filename, sizeof filename, &CC->room, ctdl_netcfg_dir);

	begin_critical_section(S_NETCONFIGS);

	/* Only do net processing for rooms that have netconfigs */
	fp = fopen(filename, "r");
	if (fp == NULL) {
		end_critical_section(S_NETCONFIGS);
		return;
	}

	lprintf(CTDL_INFO, "Networking started for <%s>\n", CC->room.QRname);

	while (fgets(buf, sizeof buf, fp) != NULL) {
		buf[strlen(buf)-1] = 0;

		extract_token(instr, buf, 0, '|', sizeof instr);
		if (!strcasecmp(instr, "lastsent")) {
			sc.lastsent = extract_long(buf, 1);
		}
		else if (!strcasecmp(instr, "listrecp")) {
			nptr = (struct namelist *)
				malloc(sizeof(struct namelist));
			nptr->next = sc.listrecps;
			extract_token(nptr->name, buf, 1, '|', sizeof nptr->name);
			sc.listrecps = nptr;
		}
		else if (!strcasecmp(instr, "participate")) {
			nptr = (struct namelist *)
				malloc(sizeof(struct namelist));
			nptr->next = sc.participates;
			extract_token(nptr->name, buf, 1, '|', sizeof nptr->name);
			sc.participates = nptr;
		}
		else if (!strcasecmp(instr, "digestrecp")) {
			nptr = (struct namelist *)
				malloc(sizeof(struct namelist));
			nptr->next = sc.digestrecps;
			extract_token(nptr->name, buf, 1, '|', sizeof nptr->name);
			sc.digestrecps = nptr;
		}
		else if (!strcasecmp(instr, "ignet_push_share")) {
			/* by checking each node's validity, we automatically
			 * purge nodes which do not exist from room network
			 * configurations at this time.
			 */
			extract_token(nodename, buf, 1, '|', sizeof nodename);
			extract_token(roomname, buf, 2, '|', sizeof roomname);
			strcpy(nexthop, "xxx");
			if (is_valid_node(nexthop, NULL, nodename) == 0) {
				if (IsEmptyStr(nexthop)) {
					mptr = (struct maplist *)
						malloc(sizeof(struct maplist));
					mptr->next = sc.ignet_push_shares;
					strcpy(mptr->remote_nodename, nodename);
					strcpy(mptr->remote_roomname, roomname);
					sc.ignet_push_shares = mptr;
				}
			}
		}
		else {
			/* Preserve 'other' lines ... *unless* they happen to
			 * be subscribe/unsubscribe pendings with expired
			 * timestamps.
			 */
			skipthisline = 0;
			if (!strncasecmp(buf, "subpending|", 11)) {
				if (time(NULL) - extract_long(buf, 4) > EXP) {
					skipthisline = 1;
				}
			}
			if (!strncasecmp(buf, "unsubpending|", 13)) {
				if (time(NULL) - extract_long(buf, 3) > EXP) {
					skipthisline = 1;
				}
			}

			if (skipthisline == 0) {
				linesize = strlen(buf);
				sc.misc = realloc(sc.misc,
					(miscsize + linesize + 2) );
				sprintf(&sc.misc[miscsize], "%s\n", buf);
				miscsize = miscsize + linesize + 1;
			}
		}


	}
	fclose(fp);

	/* If there are digest recipients, we have to build a digest */
	if (sc.digestrecps != NULL) {
		sc.digestfp = tmpfile();
		fprintf(sc.digestfp, "Content-type: text/plain\n\n");
	}

	/* Do something useful */
	CtdlForEachMessage(MSGS_GT, sc.lastsent, NULL, NULL, NULL,
		network_spool_msg, &sc);

	/* If we wrote a digest, deliver it and then close it */
	snprintf(buf, sizeof buf, "room_%s@%s",
		CC->room.QRname, config.c_fqdn);
	for (i=0; buf[i]; ++i) {
		buf[i] = tolower(buf[i]);
		if (isspace(buf[i])) buf[i] = '_';
	}
	if (sc.digestfp != NULL) {
		fprintf(sc.digestfp,	" -----------------------------------"
					"------------------------------------"
					"-------\n"
					"You are subscribed to the '%s' "
					"list.\n"
					"To post to the list: %s\n",
					CC->room.QRname, buf
		);
		network_deliver_digest(&sc);	/* deliver and close */
	}

	/* Now rewrite the config file */
	fp = fopen(filename, "w");
	if (fp == NULL) {
		lprintf(CTDL_CRIT, "ERROR: cannot open %s: %s\n",
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
			free(sc.listrecps);
			sc.listrecps = nptr;
		}
		/* Do the same for digestrecps */
		while (sc.digestrecps != NULL) {
			fprintf(fp, "digestrecp|%s\n", sc.digestrecps->name);
			nptr = sc.digestrecps->next;
			free(sc.digestrecps);
			sc.digestrecps = nptr;
		}
		/* Do the same for participates */
		while (sc.participates != NULL) {
			fprintf(fp, "participate|%s\n", sc.participates->name);
			nptr = sc.participates->next;
			free(sc.participates);
			sc.participates = nptr;
		}
		while (sc.ignet_push_shares != NULL) {
			/* by checking each node's validity, we automatically
			 * purge nodes which do not exist from room network
			 * configurations at this time.
			 */
			if (is_valid_node(NULL, NULL, sc.ignet_push_shares->remote_nodename) == 0) {
			}
			fprintf(fp, "ignet_push_share|%s",
				sc.ignet_push_shares->remote_nodename);
			if (!IsEmptyStr(sc.ignet_push_shares->remote_roomname)) {
				fprintf(fp, "|%s", sc.ignet_push_shares->remote_roomname);
			}
			fprintf(fp, "\n");
			mptr = sc.ignet_push_shares->next;
			free(sc.ignet_push_shares);
			sc.ignet_push_shares = mptr;
		}
		if (sc.misc != NULL) {
			fwrite(sc.misc, strlen(sc.misc), 1, fp);
		}
		free(sc.misc);

		fclose(fp);
	}
	end_critical_section(S_NETCONFIGS);
}



/*
 * Send the *entire* contents of the current room to one specific network node,
 * ignoring anything we know about which messages have already undergone
 * network processing.  This can be used to bring a new node into sync.
 */
int network_sync_to(char *target_node) {
	struct SpoolControl sc;
	int num_spooled = 0;
	int found_node = 0;
	char buf[256];
	char sc_type[256];
	char sc_node[256];
	char sc_room[256];
	char filename[256];
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
			memset(&sc, 0, sizeof(struct SpoolControl));
			sc.ignet_push_shares = (struct maplist *)
				malloc(sizeof(struct maplist));
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

	lprintf(CTDL_NOTICE, "Synchronized %d messages to <%s>\n",
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
void network_queue_room(struct ctdlroom *qrbuf, void *data) {
	struct RoomProcList *ptr;

	ptr = (struct RoomProcList *) malloc(sizeof (struct RoomProcList));
	if (ptr == NULL) return;

	safestrncpy(ptr->name, qrbuf->QRname, sizeof ptr->name);
	begin_critical_section(S_RPLIST);
	ptr->next = rplist;
	rplist = ptr;
	end_critical_section(S_RPLIST);
}

void destroy_network_queue_room(void)
{
	struct RoomProcList *cur, *p;
	struct NetMap *nmcur, *nmp;

	cur = rplist;
	begin_critical_section(S_RPLIST);
	while (cur != NULL)
	{
		p = cur->next;
		free (cur);
		cur = p;		
	}
	rplist = NULL;
	end_critical_section(S_RPLIST);

	nmcur = the_netmap;
	while (nmcur != NULL)
	{
		nmp = nmcur->next;
		free (nmcur);
		nmcur = nmp;		
	}
	the_netmap = NULL;
	if (working_ignetcfg != NULL)
		free (working_ignetcfg);
	working_ignetcfg = NULL;
}


/*
 * Learn topology from path fields
 */
void network_learn_topology(char *node, char *path) {
	char nexthop[256];
	struct NetMap *nmptr;

	strcpy(nexthop, "");

	if (num_tokens(path, '!') < 3) return;
	for (nmptr = the_netmap; nmptr != NULL; nmptr = nmptr->next) {
		if (!strcasecmp(nmptr->nodename, node)) {
			extract_token(nmptr->nexthop, path, 0, '!', sizeof nmptr->nexthop);
			nmptr->lastcontact = time(NULL);
			++netmap_changed;
			return;
		}
	}

	/* If we got here then it's not in the map, so add it. */
	nmptr = (struct NetMap *) malloc(sizeof (struct NetMap));
	strcpy(nmptr->nodename, node);
	nmptr->lastcontact = time(NULL);
	extract_token(nmptr->nexthop, path, 0, '!', sizeof nmptr->nexthop);
	nmptr->next = the_netmap;
	the_netmap = nmptr;
	++netmap_changed;
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

	lprintf(CTDL_DEBUG, "entering network_bounce()\n");

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
	valid = validate_recipients(recipient);
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
	CtdlSubmitMsg(msg, valid, force_room);

	/* Clean up */
	if (valid != NULL) free_recipients(valid);
	CtdlFreeMessage(msg);
	lprintf(CTDL_DEBUG, "leaving network_bounce()\n");
}




/*
 * Process a buffer containing a single message from a single file
 * from the inbound queue 
 */
void network_process_buffer(char *buffer, long size) {
	struct CtdlMessage *msg = NULL;
	long pos;
	int field;
	struct recptypes *recp = NULL;
	char target_room[ROOMNAMELEN];
	struct ser_ret sermsg;
	char *oldpath = NULL;
	char filename[SIZ];
	FILE *fp;
	char nexthop[SIZ];
	unsigned char firstbyte;
	unsigned char lastbyte;

	/* Validate just a little bit.  First byte should be FF and * last byte should be 00. */
	firstbyte = buffer[0];
	lastbyte = buffer[size-1];
	if ( (firstbyte != 255) || (lastbyte != 0) ) {
		lprintf(CTDL_ERR, "Corrupt message ignored.  Length=%ld, firstbyte = %d, lastbyte = %d\n",
			size, firstbyte, lastbyte);
		return;
	}

	/* Set default target room to trash */
	strcpy(target_room, TWITROOM);

	/* Load the message into memory */
	msg = (struct CtdlMessage *) malloc(sizeof(struct CtdlMessage));
	memset(msg, 0, sizeof(struct CtdlMessage));
	msg->cm_magic = CTDLMESSAGE_MAGIC;
	msg->cm_anon_type = buffer[1];
	msg->cm_format_type = buffer[2];

	for (pos = 3; pos < size; ++pos) {
		field = buffer[pos];
		msg->cm_fields[field] = strdup(&buffer[pos+1]);
		pos = pos + strlen(&buffer[(int)pos]);
	}

	/* Check for message routing */
	if (msg->cm_fields['D'] != NULL) {
		if (strcasecmp(msg->cm_fields['D'], config.c_nodename)) {

			/* route the message */
			strcpy(nexthop, "");
			if (is_valid_node(nexthop, NULL,
			   msg->cm_fields['D']) == 0) {

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
				snprintf(msg->cm_fields['P'], size, "%s!%s",
					config.c_nodename, oldpath);
				free(oldpath);

				/* serialize the message */
				serialize_message(&sermsg, msg);

				/* now send it */
				if (IsEmptyStr(nexthop)) {
					strcpy(nexthop, msg->cm_fields['D']);
				}
				snprintf(filename, 
						 sizeof filename,
						 "%s/%s",
						 ctdl_netout_dir,
						 nexthop);
				lprintf(CTDL_DEBUG, "Appending to %s\n", filename);
				fp = fopen(filename, "ab");
				if (fp != NULL) {
					fwrite(sermsg.ser,
						sermsg.len, 1, fp);
					fclose(fp);
				}
				else {
					lprintf(CTDL_ERR, "%s: %s\n", filename, strerror(errno));
				}
				free(sermsg.ser);
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
	 * Check to see if we already have a copy of this message, and
	 * abort its processing if so.  (We used to post a warning to Aide>
	 * every time this happened, but the network is now so densely
	 * connected that it's inevitable.)
	 */
	if (network_usetable(msg) != 0) {
		CtdlFreeMessage(msg);
		return;
	}

	/* Learn network topology from the path */
	if ((msg->cm_fields['N'] != NULL) && (msg->cm_fields['P'] != NULL)) {
		network_learn_topology(msg->cm_fields['N'], 
					msg->cm_fields['P']);
	}

	/* Is the sending node giving us a very persuasive suggestion about
	 * which room this message should be saved in?  If so, go with that.
	 */
	if (msg->cm_fields['C'] != NULL) {
		safestrncpy(target_room,
			msg->cm_fields['C'],
			sizeof target_room);
	}

	/* Otherwise, does it have a recipient?  If so, validate it... */
	else if (msg->cm_fields['R'] != NULL) {
		recp = validate_recipients(msg->cm_fields['R']);
		if (recp != NULL) if (recp->num_error != 0) {
			network_bounce(msg,
				"A message you sent could not be delivered due to an invalid address.\n"
				"Please check the address and try sending the message again.\n");
			msg = NULL;
			free_recipients(recp);
			return;
		}
		strcpy(target_room, "");	/* no target room if mail */
	}

	/* Our last shot at finding a home for this message is to see if
	 * it has the O field (Originating room) set.
	 */
	else if (msg->cm_fields['O'] != NULL) {
		safestrncpy(target_room,
			msg->cm_fields['O'],
			sizeof target_room);
	}

	/* Strip out fields that are only relevant during transit */
	if (msg->cm_fields['D'] != NULL) {
		free(msg->cm_fields['D']);
		msg->cm_fields['D'] = NULL;
	}
	if (msg->cm_fields['C'] != NULL) {
		free(msg->cm_fields['C']);
		msg->cm_fields['C'] = NULL;
	}

	/* save the message into a room */
	if (PerformNetprocHooks(msg, target_room) == 0) {
		msg->cm_flags = CM_SKIP_HOOKS;
		CtdlSubmitMsg(msg, recp, target_room);
	}
	CtdlFreeMessage(msg);
	free_recipients(recp);
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
	buffer = malloc(size);
	if (buffer != NULL) {
		fseek(fp, msgstart, SEEK_SET);
		fread(buffer, size, 1, fp);
		network_process_buffer(buffer, size);
		free(buffer);
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


	fp = fopen(filename, "rb");
	if (fp == NULL) {
		lprintf(CTDL_CRIT, "Error opening %s: %s\n", filename, strerror(errno));
		return;
	}

	fseek(fp, 0L, SEEK_END);
	lprintf(CTDL_INFO, "network: processing %ld bytes from %s\n", ftell(fp), filename);
	rewind(fp);

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
	struct stat statbuf;
	char filename[256];
	static time_t last_spoolin_mtime = 0L;

	/*
	 * Check the spoolin directory's modification time.  If it hasn't
	 * been touched, we don't need to scan it.
	 */
	if (stat(ctdl_netin_dir, &statbuf)) return;
	if (statbuf.st_mtime == last_spoolin_mtime) {
		lprintf(CTDL_DEBUG, "network: nothing in inbound queue\n");
		return;
	}
	last_spoolin_mtime = statbuf.st_mtime;
	lprintf(CTDL_DEBUG, "network: processing inbound queue\n");

	/*
	 * Ok, there's something interesting in there, so scan it.
	 */
	dp = opendir(ctdl_netin_dir);
	if (dp == NULL) return;

	while (d = readdir(dp), d != NULL) {
		if ((strcmp(d->d_name, ".")) && (strcmp(d->d_name, ".."))) {
			snprintf(filename, 
					 sizeof filename,
					 "%s/%s",
					 ctdl_netin_dir,
					 d->d_name);
			network_process_file(filename);
		}
	}

	closedir(dp);
}

/*
 * Delete any files in the outbound queue that were intended
 * to be sent to nodes which no longer exist.
 */
void network_purge_spoolout(void) {
	DIR *dp;
	struct dirent *d;
	char filename[256];
	char nexthop[256];
	int i;

	dp = opendir(ctdl_netout_dir);
	if (dp == NULL) return;

	while (d = readdir(dp), d != NULL) {
		if (!strcmp(d->d_name, ".") || !strcmp(d->d_name, ".."))
			continue;
		snprintf(filename, 
				 sizeof filename,
				 "%s/%s",
				 ctdl_netout_dir,
				 d->d_name);

		strcpy(nexthop, "");
		i = is_valid_node(nexthop, NULL, d->d_name);
	
		if ( (i != 0) || !IsEmptyStr(nexthop) ) {
			unlink(filename);
		}
	}


	closedir(dp);
}


/*
 * receive network spool from the remote system
 */
void receive_spool(int sock, char *remote_nodename) {
	long download_len = 0L;
	long bytes_received = 0L;
	long bytes_copied = 0L;
	char buf[SIZ];
	static char pbuf[IGNET_PACKET_SIZE];
	char tempfilename[PATH_MAX];
	char filename[PATH_MAX];
	long plen;
	FILE *fp, *newfp;

	CtdlMakeTempFileName(tempfilename, sizeof tempfilename);
	if (sock_puts(sock, "NDOP") < 0) return;
	if (sock_getln(sock, buf, sizeof buf) < 0) return;
	lprintf(CTDL_DEBUG, "<%s\n", buf);
	if (buf[0] != '2') {
		return;
	}
	download_len = extract_long(&buf[4], 0);

	bytes_received = 0L;
	fp = fopen(tempfilename, "w");
	if (fp == NULL) {
		lprintf(CTDL_CRIT, "cannot open download file locally: %s\n",
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
		if (sock_getln(sock, buf, sizeof buf) < 0) {
			fclose(fp);
			unlink(tempfilename);
			return;
		}
		if (buf[0] == '6') {
			plen = extract_long(&buf[4], 0);
			if (sock_read(sock, pbuf, plen, 1) < 0) {
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
	if (sock_getln(sock, buf, sizeof buf) < 0) {
		unlink(tempfilename);
		return;
	}
	if (download_len > 0) {
		lprintf(CTDL_NOTICE, "Received %ld octets from <%s>\n", download_len, remote_nodename);
	}
	lprintf(CTDL_DEBUG, "%s\n", buf);
	
	/* Now copy the temp file to its permanent location.
	 * (We copy instead of link because they may be on different filesystems)
	 */
	begin_critical_section(S_NETSPOOL);
	snprintf(filename, 
			 sizeof filename, 
			 "%s/%s.%ld",
			 ctdl_netin_dir,
			 remote_nodename, 
			 (long) getpid()
	);
	fp = fopen(tempfilename, "r");
	if (fp != NULL) {
		newfp = fopen(filename, "w");
		if (newfp != NULL) {
			bytes_copied = 0L;
			while (bytes_copied < download_len) {
				plen = download_len - bytes_copied;
				if (plen > sizeof buf) {
					plen = sizeof buf;
				}
				fread(buf, plen, 1, fp);
				fwrite(buf, plen, 1, newfp);
				bytes_copied += plen;
			}
			fclose(newfp);
		}
		fclose(fp);
	}
	end_critical_section(S_NETSPOOL);
	unlink(tempfilename);
}



/*
 * transmit network spool to the remote system
 */
void transmit_spool(int sock, char *remote_nodename)
{
	char buf[SIZ];
	char pbuf[4096];
	long plen;
	long bytes_to_write, thisblock, bytes_written;
	int fd;
	char sfname[128];

	if (sock_puts(sock, "NUOP") < 0) return;
	if (sock_getln(sock, buf, sizeof buf) < 0) return;
	lprintf(CTDL_DEBUG, "<%s\n", buf);
	if (buf[0] != '2') {
		return;
	}

	snprintf(sfname, sizeof sfname, 
			 "%s/%s",
			 ctdl_netout_dir,
			 remote_nodename);
	fd = open(sfname, O_RDONLY);
	if (fd < 0) {
		if (errno != ENOENT) {
			lprintf(CTDL_CRIT, "cannot open upload file locally: %s\n",
				strerror(errno));
		}
		return;
	}
	bytes_written = 0;
	while (plen = (long) read(fd, pbuf, IGNET_PACKET_SIZE), plen > 0L) {
		bytes_to_write = plen;
		while (bytes_to_write > 0L) {
			snprintf(buf, sizeof buf, "WRIT %ld", bytes_to_write);
			if (sock_puts(sock, buf) < 0) {
				close(fd);
				return;
			}
			if (sock_getln(sock, buf, sizeof buf) < 0) {
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
				bytes_to_write -= thisblock;
				bytes_written += thisblock;
			} else {
				goto ABORTUPL;
			}
		}
	}

ABORTUPL:
	close(fd);
	if (sock_puts(sock, "UCLS 1") < 0) return;
	if (sock_getln(sock, buf, sizeof buf) < 0) return;
	lprintf(CTDL_NOTICE, "Sent %ld octets to <%s>\n",
			bytes_written, remote_nodename);
	lprintf(CTDL_DEBUG, "<%s\n", buf);
	if (buf[0] == '2') {
		lprintf(CTDL_DEBUG, "Removing <%s>\n", sfname);
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
	lprintf(CTDL_NOTICE, "Connecting to <%s> at %s:%s\n", node, host, port);

	sock = sock_connect(host, port, "tcp");
	if (sock < 0) {
		lprintf(CTDL_ERR, "Could not connect: %s\n", strerror(errno));
		network_talking_to(node, NTT_REMOVE);
		return;
	}
	
	lprintf(CTDL_DEBUG, "Connected!\n");

	/* Read the server greeting */
	if (sock_getln(sock, buf, sizeof buf) < 0) goto bail;
	lprintf(CTDL_DEBUG, ">%s\n", buf);

	/* Identify ourselves */
	snprintf(buf, sizeof buf, "NETP %s|%s", config.c_nodename, secret);
	lprintf(CTDL_DEBUG, "<%s\n", buf);
	if (sock_puts(sock, buf) <0) goto bail;
	if (sock_getln(sock, buf, sizeof buf) < 0) goto bail;
	lprintf(CTDL_DEBUG, ">%s\n", buf);
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
 * Set "full" to nonzero to force a poll of every node, or to zero to poll
 * only nodes to which we have data to send.
 */
void network_poll_other_citadel_nodes(int full_poll) {
	int i;
	char linebuf[256];
	char node[SIZ];
	char host[256];
	char port[256];
	char secret[256];
	int poll = 0;
	char spoolfile[256];

	if (working_ignetcfg == NULL) {
		lprintf(CTDL_DEBUG, "No nodes defined - not polling\n");
		return;
	}

	/* Use the string tokenizer to grab one line at a time */
	for (i=0; i<num_tokens(working_ignetcfg, '\n'); ++i) {
		extract_token(linebuf, working_ignetcfg, i, '\n', sizeof linebuf);
		extract_token(node, linebuf, 0, '|', sizeof node);
		extract_token(secret, linebuf, 1, '|', sizeof secret);
		extract_token(host, linebuf, 2, '|', sizeof host);
		extract_token(port, linebuf, 3, '|', sizeof port);
		if ( !IsEmptyStr(node) && !IsEmptyStr(secret) 
		   && !IsEmptyStr(host) && !IsEmptyStr(port)) {
			poll = full_poll;
			if (poll == 0) {
				snprintf(spoolfile, 
						 sizeof spoolfile,
						 "%s/%s",
						 ctdl_netout_dir, 
						 node);
				if (access(spoolfile, R_OK) == 0) {
					poll = 1;
				}
			}
			if (poll) {
				network_poll_node(node, secret, host, port);
			}
		}
	}

}




/*
 * It's ok if these directories already exist.  Just fail silently.
 */
void create_spool_dirs(void) {
	mkdir(ctdl_spool_dir, 0700);
	chown(ctdl_spool_dir, CTDLUID, (-1));
	mkdir(ctdl_netin_dir, 0700);
	chown(ctdl_netin_dir, CTDLUID, (-1));
	mkdir(ctdl_netout_dir, 0700);
	chown(ctdl_netout_dir, CTDLUID, (-1));
}





/*
 * network_do_queue()
 * 
 * Run through the rooms doing various types of network stuff.
 */
void network_do_queue(void) {
	static time_t last_run = 0L;
	struct RoomProcList *ptr;
	int full_processing = 1;

	/*
	 * Run the full set of processing tasks no more frequently
	 * than once every n seconds
	 */
	if ( (time(NULL) - last_run) < config.c_net_freq ) {
		full_processing = 0;
	}

	/*
	 * This is a simple concurrency check to make sure only one queue run
	 * is done at a time.  We could do this with a mutex, but since we
	 * don't really require extremely fine granularity here, we'll do it
	 * with a static variable instead.
	 */
	if (doing_queue) return;
	doing_queue = 1;

	/* Load the IGnet Configuration into memory */
	load_working_ignetcfg();

	/*
	 * Poll other Citadel nodes.  Maybe.  If "full_processing" is set
	 * then we poll everyone.  Otherwise we only poll nodes we have stuff
	 * to send to.
	 */
	network_poll_other_citadel_nodes(full_processing);

	/*
	 * Load the network map and filter list into memory.
	 */
	read_network_map();
	filterlist = load_filter_list();

	/* 
	 * Go ahead and run the queue
	 */
	if (full_processing) {
		lprintf(CTDL_DEBUG, "network: loading outbound queue\n");
		ForEachRoom(network_queue_room, NULL);
	}

	if (rplist != NULL) {
		lprintf(CTDL_DEBUG, "network: running outbound queue\n");
		while (rplist != NULL) {
			char spoolroomname[ROOMNAMELEN];
			safestrncpy(spoolroomname, rplist->name, sizeof spoolroomname);
			begin_critical_section(S_RPLIST);

			/* pop this record off the list */
			ptr = rplist;
			rplist = rplist->next;
			free(ptr);

			/* invalidate any duplicate entries to prevent double processing */
			for (ptr=rplist; ptr!=NULL; ptr=ptr->next) {
				if (!strcasecmp(ptr->name, spoolroomname)) {
					ptr->name[0] = 0;
				}
			}

			end_critical_section(S_RPLIST);
			if (spoolroomname[0] != 0) {
				network_spoolout_room(spoolroomname);
			}
		}
	}

	/* If there is anything in the inbound queue, process it */
	network_do_spoolin();

	/* Save the network map back to disk */
	write_network_map();

	/* Free the filter list in memory */
	free_filter_list(filterlist);
	filterlist = NULL;

	network_purge_spoolout();

	lprintf(CTDL_DEBUG, "network: queue run completed\n");

	if (full_processing) {
		last_run = time(NULL);
	}

	doing_queue = 0;
}


/*
 * cmd_netp() - authenticate to the server as another Citadel node polling
 *	      for network traffic
 */
void cmd_netp(char *cmdbuf)
{
	char node[256];
	char pass[256];
	int v;

	char secret[256];
	char nexthop[256];

	/* Authenticate */
	extract_token(node, cmdbuf, 0, '|', sizeof node);
	extract_token(pass, cmdbuf, 1, '|', sizeof pass);

	if (doing_queue) {
		lprintf(CTDL_WARNING, "Network node <%s> refused - spooling", node);
		cprintf("%d spooling - try again in a few minutes\n",
			ERROR + RESOURCE_BUSY);
		return;
	}

	/* load the IGnet Configuration to check node validity */
	load_working_ignetcfg();
	v = is_valid_node(nexthop, secret, node);

	if (v != 0) {
		lprintf(CTDL_WARNING, "Unknown node <%s>\n", node);
		cprintf("%d authentication failed\n",
			ERROR + PASSWORD_REQUIRED);
		return;
	}

	if (strcasecmp(pass, secret)) {
		lprintf(CTDL_WARNING, "Bad password for network node <%s>", node);
		cprintf("%d authentication failed\n", ERROR + PASSWORD_REQUIRED);
		return;
	}

	if (network_talking_to(node, NTT_CHECK)) {
		lprintf(CTDL_WARNING, "Duplicate session for network node <%s>", node);
		cprintf("%d Already talking to %s right now\n", ERROR + RESOURCE_BUSY, node);
		return;
	}

	safestrncpy(CC->net_node, node, sizeof CC->net_node);
	network_talking_to(node, NTT_ADD);
	lprintf(CTDL_NOTICE, "Network node <%s> logged in\n", CC->net_node);
	cprintf("%d authenticated as network node '%s'\n", CIT_OK,
		CC->net_node);
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
		create_spool_dirs();
		CtdlRegisterProtoHook(cmd_gnet, "GNET", "Get network config");
		CtdlRegisterProtoHook(cmd_snet, "SNET", "Set network config");
		CtdlRegisterProtoHook(cmd_netp, "NETP", "Identify as network poller");
		CtdlRegisterProtoHook(cmd_nsyn, "NSYN", "Synchronize room to node");
		CtdlRegisterSessionHook(network_do_queue, EVT_TIMER);
	        CtdlRegisterRoomHook(network_room_handler);
		CtdlRegisterCleanupHook(destroy_network_queue_room);
	}

	/* return our Subversion id for the Log */
	return "$Id$";
}
