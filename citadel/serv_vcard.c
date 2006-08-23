/*
 * $Id$
 * 
 * A server-side module for Citadel which supports address book information
 * using the standard vCard format.
 * 
 * Copyright (c) 1999-2002 / released under the GNU General Public License
 */

/*
 * Format of the "Exclusive ID" field of the message containing a user's
 * vCard.  Doesn't matter what it really looks like as long as it's both
 * unique and consistent (because we use it for replication checking to
 * delete the old vCard network-wide when the user enters a new one).
 */
#define VCARD_EXT_FORMAT	"Citadel vCard: personal card for %s at %s"


#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <pwd.h>
#include <errno.h>
#include <ctype.h>
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
#include "room_ops.h"
#include "user_ops.h"
#include "policy.h"
#include "database.h"
#include "msgbase.h"
#include "internet_addressing.h"
#include "tools.h"
#include "vcard.h"
#include "serv_ldap.h"
#include "serv_vcard.h"

/*
 * set global flag calling for an aide to validate new users
 */
void set_mm_valid(void) {
	begin_critical_section(S_CONTROL);
	get_control();
	CitControl.MMflags = CitControl.MMflags | MM_VALID ;
	put_control();
	end_critical_section(S_CONTROL);
}



/*
 * Extract Internet e-mail addresses from a message containing a vCard, and
 * perform a callback for any found.
 */
void vcard_extract_internet_addresses(struct CtdlMessage *msg,
				void (*callback)(char *, char *) ) {
	struct vCard *v;
	char *s;
	char *addr;
	char citadel_address[SIZ];
	int instance = 0;
	int found_something = 0;

	if (msg->cm_fields['A'] == NULL) return;
	if (msg->cm_fields['N'] == NULL) return;
	snprintf(citadel_address, sizeof citadel_address, "%s @ %s",
		msg->cm_fields['A'], msg->cm_fields['N']);

	v = vcard_load(msg->cm_fields['M']);
	if (v == NULL) return;

	/* Go through the vCard searching for *all* instances of
	 * the "email;internet" key
	 */
	do {
		s = vcard_get_prop(v, "email;internet", 0, instance++, 0);
		if (s != NULL) {
			addr = strdup(s);
			striplt(addr);
			if (strlen(addr) > 0) {
				if (callback != NULL) {
					callback(addr, citadel_address);
				}
			}
			free(addr);
			found_something = 1;
		}
		else {
			found_something = 0;
		}
	} while(found_something);

	vcard_free(v);
}



/*
 * Callback for vcard_add_to_directory()
 * (Lotsa ugly nested callbacks.  Oh well.)
 */
void vcard_directory_add_user(char *internet_addr, char *citadel_addr) {
	char buf[SIZ];

	/* We have to validate that we're not stepping on someone else's
	 * email address ... but only if we're logged in.  Otherwise it's
	 * probably just the networker or something.
	 */
	if (CC->logged_in) {
		lprintf(CTDL_DEBUG, "Checking for <%s>...\n", internet_addr);
		if (CtdlDirectoryLookup(buf, internet_addr, sizeof buf) == 0) {
			if (strcasecmp(buf, citadel_addr)) {
				/* This address belongs to someone else.
				 * Bail out silently without saving.
				 */
				lprintf(CTDL_DEBUG, "DOOP!\n");
				return;
			}
		}
	}
	lprintf(CTDL_INFO, "Adding %s (%s) to directory\n",
			citadel_addr, internet_addr);
	CtdlDirectoryAddUser(internet_addr, citadel_addr);
}


/*
 * Back end function for cmd_igab()
 */
void vcard_add_to_directory(long msgnum, void *data) {
	struct CtdlMessage *msg;

	msg = CtdlFetchMessage(msgnum, 1);
	if (msg != NULL) {
		vcard_extract_internet_addresses(msg, vcard_directory_add_user);
	}

#ifdef HAVE_LDAP
	ctdl_vcard_to_ldap(msg, V2L_WRITE);
#endif

	CtdlFreeMessage(msg);
}


/*
 * Initialize Global Adress Book
 */
void cmd_igab(char *argbuf) {
	char hold_rm[ROOMNAMELEN];

	if (CtdlAccessCheck(ac_aide)) return;

	strcpy(hold_rm, CC->room.QRname);	/* save current room */

	if (getroom(&CC->room, ADDRESS_BOOK_ROOM) != 0) {
		getroom(&CC->room, hold_rm);
		cprintf("%d cannot get address book room\n", ERROR + ROOM_NOT_FOUND);
		return;
	}

	/* Empty the existing database first.
	 */
	CtdlDirectoryInit();

	/* We want *all* vCards in this room */
	CtdlForEachMessage(MSGS_ALL, 0, "text/x-vcard",
		NULL, vcard_add_to_directory, NULL);

	getroom(&CC->room, hold_rm);	/* return to saved room */
	cprintf("%d Directory has been rebuilt.\n", CIT_OK);
}




/*
 * See if there is a valid Internet address in a vCard to use for outbound
 * Internet messages.  If there is, stick it in the buffer.
 */
void extract_primary_inet_email(char *emailaddrbuf, size_t emailaddrbuf_len, struct vCard *v) {
	char *s, *addr;
	int continue_searching = 1;
	int instance = 0;

	/* Go through the vCard searching for *all* instances of
	 * the "email;internet" key
	 */
	do {
		s = vcard_get_prop(v, "email;internet", 0, instance++, 0);
		if (s != NULL) {
			continue_searching = 1;
			addr = strdup(s);
			striplt(addr);
			if (strlen(addr) > 0) {
				if (IsDirectory(addr)) {
					continue_searching = 0;
					safestrncpy(emailaddrbuf, addr,
						emailaddrbuf_len);
				}
			}
			free(addr);
		}
		else {
			continue_searching = 0;
		}
	} while(continue_searching);
}



/*
 * This handler detects whether the user is attempting to save a new
 * vCard as part of his/her personal configuration, and handles the replace
 * function accordingly (delete the user's existing vCard in the config room
 * and in the global address book).
 */
int vcard_upload_beforesave(struct CtdlMessage *msg) {
	char *ptr;
	char *s;
	int linelen;
	char buf[SIZ];
	struct ctdluser usbuf;
	long what_user;
	struct vCard *v = NULL;
	char *ser = NULL;
	int i = 0;
	int yes_my_citadel_config = 0;
	int yes_any_vcard_room = 0;

	if (!CC->logged_in) return(0);	/* Only do this if logged in. */

	/* Is this some user's "My Citadel Config" room? */
	if ( (CC->room.QRflags && QR_MAILBOX)
	   && (!strcasecmp(&CC->room.QRname[11], USERCONFIGROOM)) ) {
		/* Yes, we want to do this */
		yes_my_citadel_config = 1;

#ifdef VCARD_SAVES_BY_AIDES_ONLY
		/* Prevent non-aides from performing registration changes */
		if (CC->user.axlevel < 6) {
			return(1);
		}
#endif

	}

	/* Is this a room with an address book in it? */
	if (CC->room.QRdefaultview == VIEW_ADDRESSBOOK) {
		yes_any_vcard_room = 1;
	}

	/* If neither condition exists, don't run this hook. */
	if ( (!yes_my_citadel_config) && (!yes_any_vcard_room) ) {
		return(0);
	}

	/* If this isn't a MIME message, don't bother. */
	if (msg->cm_format_type != 4) return(0);

	/* Ok, if we got this far, look into the situation further... */

	ptr = msg->cm_fields['M'];
	if (ptr == NULL) return(0);
	while (ptr != NULL) {
	
		linelen = strcspn(ptr, "\n");
		if (linelen == 0) return(0);	/* end of headers */	
		
		if (!strncasecmp(ptr, "Content-type: text/x-vcard", 26)) {


			if (yes_my_citadel_config) {
				/* Bingo!  The user is uploading a new vCard, so
				 * delete the old one.  First, figure out which user
				 * is being re-registered...
				 */
				what_user = atol(CC->room.QRname);
	
				if (what_user == CC->user.usernum) {
					/* It's the logged in user.  That was easy. */
					memcpy(&usbuf, &CC->user,
						sizeof(struct ctdluser) );
				}
				
				else if (getuserbynumber(&usbuf, what_user) == 0) {
					/* We fetched a valid user record */
				}
			
				else {
					/* No user with that number! */
					return(0);
				}
	
				/* Delete the user's old vCard.  This would probably
				 * get taken care of by the replication check, but we
				 * want to make sure there is absolutely only one
				 * vCard in the user's config room at all times.
				 */
				CtdlDeleteMessages(CC->room.QRname,
						NULL, 0, "text/x-vcard", 1);

				/* Make the author of the message the name of the user.
				 */
				if (msg->cm_fields['A'] != NULL) {
					free(msg->cm_fields['A']);
				}
				msg->cm_fields['A'] = strdup(usbuf.fullname);
			}

			/* Manipulate the vCard data structure */
			v = vcard_load(msg->cm_fields['M']);
			if (v != NULL) {

				/* Insert or replace RFC2739-compliant free/busy URL */
				if (yes_my_citadel_config) {
					sprintf(buf, "http://%s/%s.vfb",
						config.c_fqdn,
						usbuf.fullname);
					for (i=0; i<strlen(buf); ++i) {
						if (buf[i] == ' ') buf[i] = '_';
					}
					vcard_set_prop(v, "FBURL;PREF", buf, 0);
				}

				/* If this is an address book room, and the vCard has
				 * no UID, then give it one.
				 */
				if (yes_any_vcard_room) {
					s = vcard_get_prop(v, "UID", 0, 0, 0);
					if (s == NULL) {
						generate_uuid(buf);
						vcard_set_prop(v, "UID", buf, 0);
					}
				}

				/* Enforce local UID policy if applicable */
				if (yes_my_citadel_config) {
					snprintf(buf, sizeof buf, VCARD_EXT_FORMAT,
						msg->cm_fields['A'], NODENAME);
					vcard_set_prop(v, "UID", buf, 0);
				}

				/* 
				 * Set the EUID of the message to the UID of the vCard.
				 */
				if (msg->cm_fields['E'] != NULL) free(msg->cm_fields['E']);
				s = vcard_get_prop(v, "UID", 0, 0, 0);
				if (s != NULL) {
					msg->cm_fields['E'] = strdup(s);
					if (msg->cm_fields['U'] == NULL) {
						msg->cm_fields['U'] = strdup(s);
					}
				}

				/*
				 * Set the Subject to the name in the vCard.
				 */
				s = vcard_get_prop(v, "FN", 0, 0, 0);
				if (s == NULL) {
					s = vcard_get_prop(v, "N", 0, 0, 0);
				}
				if (s != NULL) {
					if (msg->cm_fields['U'] != NULL) {
						free(msg->cm_fields['U']);
					}
					msg->cm_fields['U'] = strdup(s);
				}

				/* Re-serialize it back into the msg body */
				ser = vcard_serialize(v);
				if (ser != NULL) {
					msg->cm_fields['M'] = realloc(
						msg->cm_fields['M'],
						strlen(ser) + 1024
					);
					sprintf(msg->cm_fields['M'],
						"Content-type: text/x-vcard"
						"\r\n\r\n%s\r\n", ser);
					free(ser);
				}
				vcard_free(v);
			}

			/* Now allow the save to complete. */
			return(0);
		}

		ptr = strchr((char *)ptr, '\n');
		if (ptr != NULL) ++ptr;
	}

	return(0);
}



/*
 * This handler detects whether the user is attempting to save a new
 * vCard as part of his/her personal configuration, and handles the replace
 * function accordingly (copy the vCard from the config room to the global
 * address book).
 */
int vcard_upload_aftersave(struct CtdlMessage *msg) {
	char *ptr;
	int linelen;
	long I;
	struct vCard *v;

	if (!CC->logged_in) return(0);	/* Only do this if logged in. */

	/* If this isn't the configuration room, or if this isn't a MIME
	 * message, don't bother.
	 */
	if (msg->cm_fields['O'] == NULL) return(0);
	if (strcasecmp(msg->cm_fields['O'], USERCONFIGROOM)) return(0);
	if (msg->cm_format_type != 4) return(0);

	ptr = msg->cm_fields['M'];
	if (ptr == NULL) return(0);
	while (ptr != NULL) {
	
		linelen = strcspn(ptr, "\n");
		if (linelen == 0) return(0);	/* end of headers */	
		
		if (!strncasecmp(ptr, "Content-type: text/x-vcard", 26)) {
			/* Bingo!  The user is uploading a new vCard, so
			 * copy it to the Global Address Book room.
			 */

			I = atol(msg->cm_fields['I']);
			if (I < 0L) return(0);

			/* Store our Internet return address in memory */
			v = vcard_load(msg->cm_fields['M']);
			extract_primary_inet_email(CC->cs_inet_email,
						sizeof CC->cs_inet_email, v);
			vcard_free(v);

			/* Put it in the Global Address Book room... */
			CtdlSaveMsgPointerInRoom(ADDRESS_BOOK_ROOM, I, 1, msg);

			/* ...and also in the directory database. */
			vcard_add_to_directory(I, NULL);

			/* Some sites want an Aide to be notified when a
			 * user registers or re-registers...
			 */
			set_mm_valid();

			/* ...which also means we need to flag the user */
			lgetuser(&CC->user, CC->curr_user);
			CC->user.flags |= (US_REGIS|US_NEEDVALID);
			lputuser(&CC->user);

			return(0);
		}

		ptr = strchr((char *)ptr, '\n');
		if (ptr != NULL) ++ptr;
	}

	return(0);
}



/*
 * back end function used for callbacks
 */
void vcard_gu_backend(long supplied_msgnum, void *userdata) {
	long *msgnum;

	msgnum = (long *) userdata;
	*msgnum = supplied_msgnum;
}


/*
 * If this user has a vcard on disk, read it into memory, otherwise allocate
 * and return an empty vCard.
 */
struct vCard *vcard_get_user(struct ctdluser *u) {
	char hold_rm[ROOMNAMELEN];
	char config_rm[ROOMNAMELEN];
	struct CtdlMessage *msg;
	struct vCard *v;
	long VCmsgnum;

	strcpy(hold_rm, CC->room.QRname);	/* save current room */
	MailboxName(config_rm, sizeof config_rm, u, USERCONFIGROOM);

	if (getroom(&CC->room, config_rm) != 0) {
		getroom(&CC->room, hold_rm);
		return vcard_new();
	}

	/* We want the last (and probably only) vcard in this room */
	VCmsgnum = (-1);
	CtdlForEachMessage(MSGS_LAST, 1, "text/x-vcard",
		NULL, vcard_gu_backend, (void *)&VCmsgnum );
	getroom(&CC->room, hold_rm);	/* return to saved room */

	if (VCmsgnum < 0L) return vcard_new();

	msg = CtdlFetchMessage(VCmsgnum, 1);
	if (msg == NULL) return vcard_new();

	v = vcard_load(msg->cm_fields['M']);
	CtdlFreeMessage(msg);
	return v;
}


/*
 * Store this user's vCard in the appropriate place
 */
/*
 * Write our config to disk
 */
void vcard_write_user(struct ctdluser *u, struct vCard *v) {
	char temp[PATH_MAX];
	FILE *fp;
	char *ser;

	CtdlMakeTempFileName(temp, sizeof temp);
	ser = vcard_serialize(v);

	fp = fopen(temp, "w");
	if (fp == NULL) return;
	if (ser == NULL) {
		fprintf(fp, "begin:vcard\r\nend:vcard\r\n");
	} else {
		fwrite(ser, strlen(ser), 1, fp);
		free(ser);
	}
	fclose(fp);

	/* This handy API function does all the work for us.
	 * NOTE: normally we would want to set that last argument to 1, to
	 * force the system to delete the user's old vCard.  But it doesn't
	 * have to, because the vcard_upload_beforesave() hook above
	 * is going to notice what we're trying to do, and delete the old vCard.
	 */
	CtdlWriteObject(USERCONFIGROOM,	/* which room */
			"text/x-vcard",	/* MIME type */
			temp,		/* temp file */
			u,		/* which user */
			0,		/* not binary */
			0,		/* don't delete others of this type */
			0);		/* no flags */

	unlink(temp);
}



/*
 * Old style "enter registration info" command.  This function simply honors
 * the REGI protocol command, translates the entered parameters into a vCard,
 * and enters the vCard into the user's configuration.
 */
void cmd_regi(char *argbuf) {
	int a,b,c;
	char buf[SIZ];
	struct vCard *my_vcard;

	char tmpaddr[SIZ];
	char tmpcity[SIZ];
	char tmpstate[SIZ];
	char tmpzip[SIZ];
	char tmpaddress[SIZ];
	char tmpcountry[SIZ];

	unbuffer_output();

	if (!(CC->logged_in)) {
		cprintf("%d Not logged in.\n",ERROR + NOT_LOGGED_IN);
		return;
	}

	my_vcard = vcard_get_user(&CC->user);
	strcpy(tmpaddr, "");
	strcpy(tmpcity, "");
	strcpy(tmpstate, "");
	strcpy(tmpzip, "");
	strcpy(tmpcountry, "USA");

	cprintf("%d Send registration...\n", SEND_LISTING);
	a=0;
	while (client_getln(buf, sizeof buf), strcmp(buf,"000")) {
		if (a==0) vcard_set_prop(my_vcard, "n", buf, 0);
		if (a==1) strcpy(tmpaddr, buf);
		if (a==2) strcpy(tmpcity, buf);
		if (a==3) strcpy(tmpstate, buf);
		if (a==4) {
			for (c=0; c<strlen(buf); ++c) {
				if ((buf[c]>='0') && (buf[c]<='9')) {
					b = strlen(tmpzip);
					tmpzip[b] = buf[c];
					tmpzip[b+1] = 0;
				}
			}
		}
		if (a==5) vcard_set_prop(my_vcard, "tel;home", buf, 0);
		if (a==6) vcard_set_prop(my_vcard, "email;internet", buf, 0);
		if (a==7) strcpy(tmpcountry, buf);
		++a;
	}

	snprintf(tmpaddress, sizeof tmpaddress, ";;%s;%s;%s;%s;%s",
		tmpaddr, tmpcity, tmpstate, tmpzip, tmpcountry);
	vcard_set_prop(my_vcard, "adr", tmpaddress, 0);
	vcard_write_user(&CC->user, my_vcard);
	vcard_free(my_vcard);
}


/*
 * Protocol command to fetch registration info for a user
 */
void cmd_greg(char *argbuf)
{
	struct ctdluser usbuf;
	struct vCard *v;
	char *s;
	char who[USERNAME_SIZE];
	char adr[256];
	char buf[256];

	extract_token(who, argbuf, 0, '|', sizeof who);

	if (!(CC->logged_in)) {
		cprintf("%d Not logged in.\n", ERROR + NOT_LOGGED_IN);
		return;
	}

	if (!strcasecmp(who,"_SELF_")) strcpy(who,CC->curr_user);

	if ((CC->user.axlevel < 6) && (strcasecmp(who,CC->curr_user))) {
		cprintf("%d Higher access required.\n",
			ERROR + HIGHER_ACCESS_REQUIRED);
		return;
	}

	if (getuser(&usbuf, who) != 0) {
		cprintf("%d '%s' not found.\n", ERROR + NO_SUCH_USER, who);
		return;
	}

	v = vcard_get_user(&usbuf);

	cprintf("%d %s\n", LISTING_FOLLOWS, usbuf.fullname);
	cprintf("%ld\n", usbuf.usernum);
	cprintf("%s\n", usbuf.password);
	s = vcard_get_prop(v, "n", 0, 0, 0);
	cprintf("%s\n", s ? s : " ");	/* name */

	s = vcard_get_prop(v, "adr", 0, 0, 0);
	snprintf(adr, sizeof adr, "%s", s ? s : " ");/* address... */

	extract_token(buf, adr, 2, ';', sizeof buf);
	cprintf("%s\n", buf);				/* street */
	extract_token(buf, adr, 3, ';', sizeof buf);
	cprintf("%s\n", buf);				/* city */
	extract_token(buf, adr, 4, ';', sizeof buf);
	cprintf("%s\n", buf);				/* state */
	extract_token(buf, adr, 5, ';', sizeof buf);
	cprintf("%s\n", buf);				/* zip */

	s = vcard_get_prop(v, "tel;home", 0, 0, 0);
	if (s == NULL) s = vcard_get_prop(v, "tel", 1, 0, 0);
	if (s != NULL) {
		cprintf("%s\n", s);
	}
	else {
		cprintf(" \n");
	}

	cprintf("%d\n", usbuf.axlevel);

	s = vcard_get_prop(v, "email;internet", 0, 0, 0);
	cprintf("%s\n", s ? s : " ");
	s = vcard_get_prop(v, "adr", 0, 0, 0);
	snprintf(adr, sizeof adr, "%s", s ? s : " ");/* address... */

	extract_token(buf, adr, 6, ';', sizeof buf);
	cprintf("%s\n", buf);				/* country */
	cprintf("000\n");
}



/*
 * When a user is being created, create his/her vCard.
 */
void vcard_newuser(struct ctdluser *usbuf) {
	char vname[256];
	char buf[256];
	int i;
	struct vCard *v;

	vcard_fn_to_n(vname, usbuf->fullname, sizeof vname);
	lprintf(CTDL_DEBUG, "Converted <%s> to <%s>\n", usbuf->fullname, vname);

	/* Create and save the vCard */
	v = vcard_new();
	if (v == NULL) return;
	sprintf(buf, "%s@%s", usbuf->fullname, config.c_fqdn);
	for (i=0; i<strlen(buf); ++i) {
		if (buf[i] == ' ') buf[i] = '_';
	}
	vcard_add_prop(v, "fn", usbuf->fullname);
	vcard_add_prop(v, "n", vname);
	vcard_add_prop(v, "adr", "adr:;;_;_;_;00000;__");
	vcard_add_prop(v, "email;internet", buf);
	vcard_write_user(usbuf, v);
	vcard_free(v);
}


/*
 * When a user is being deleted, we have to remove his/her vCard.
 * This is accomplished by issuing a message with 'CANCEL' in the S (special)
 * field, and the same Exclusive ID as the existing card.
 */
void vcard_purge(struct ctdluser *usbuf) {
	struct CtdlMessage *msg;
	char buf[SIZ];

	msg = (struct CtdlMessage *) malloc(sizeof(struct CtdlMessage));
	if (msg == NULL) return;
	memset(msg, 0, sizeof(struct CtdlMessage));

	msg->cm_magic = CTDLMESSAGE_MAGIC;
	msg->cm_anon_type = MES_NORMAL;
	msg->cm_format_type = 0;
	msg->cm_fields['A'] = strdup(usbuf->fullname);
	msg->cm_fields['O'] = strdup(ADDRESS_BOOK_ROOM);
	msg->cm_fields['N'] = strdup(NODENAME);
	msg->cm_fields['M'] = strdup("Purge this vCard\n");

	snprintf(buf, sizeof buf, VCARD_EXT_FORMAT,
			msg->cm_fields['A'], NODENAME);
	msg->cm_fields['E'] = strdup(buf);

	msg->cm_fields['S'] = strdup("CANCEL");

	CtdlSubmitMsg(msg, NULL, ADDRESS_BOOK_ROOM);
	CtdlFreeMessage(msg);
}


/*
 * Grab vCard directory stuff out of incoming network messages
 */
int vcard_extract_from_network(struct CtdlMessage *msg, char *target_room) {
	char *ptr;
	int linelen;

	if (msg == NULL) return(0);

	if (strcasecmp(target_room, ADDRESS_BOOK_ROOM)) {
		return(0);
	}

	if (msg->cm_format_type != 4) return(0);

	ptr = msg->cm_fields['M'];
	if (ptr == NULL) return(0);
	while (ptr != NULL) {
	
		linelen = strcspn(ptr, "\n");
		if (linelen == 0) return(0);	/* end of headers */	
		
		if (!strncasecmp(ptr, "Content-type: text/x-vcard", 26)) {
			 /* It's a vCard.  Add it to the directory. */
			vcard_extract_internet_addresses(msg,
							CtdlDirectoryAddUser);
			return(0);
		}

		ptr = strchr((char *)ptr, '\n');
		if (ptr != NULL) ++ptr;
	}

	return(0);
}



/* 
 * When a vCard is being removed from the Global Address Book room, remove it
 * from the directory as well.
 */
void vcard_delete_remove(char *room, long msgnum) {
	struct CtdlMessage *msg;
	char *ptr;
	int linelen;

	if (msgnum <= 0L) return;

	if (strcasecmp(room, ADDRESS_BOOK_ROOM)) {
		return;
	}

	msg = CtdlFetchMessage(msgnum, 1);
	if (msg == NULL) return;

	ptr = msg->cm_fields['M'];
	if (ptr == NULL) goto EOH;
	while (ptr != NULL) {
		linelen = strcspn(ptr, "\n");
		if (linelen == 0) goto EOH;
		
		if (!strncasecmp(ptr, "Content-type: text/x-vcard", 26)) {
			/* Bingo!  A vCard is being deleted.
		 	*/
			vcard_extract_internet_addresses(msg,
							CtdlDirectoryDelUser);
#ifdef HAVE_LDAP
			ctdl_vcard_to_ldap(msg, V2L_DELETE);
#endif
		}
		ptr = strchr((char *)ptr, '\n');
		if (ptr != NULL) ++ptr;
	}

EOH:	CtdlFreeMessage(msg);
}



/*
 * Query Directory
 */
void cmd_qdir(char *argbuf) {
	char citadel_addr[256];
	char internet_addr[256];

	if (CtdlAccessCheck(ac_logged_in)) return;

	extract_token(internet_addr, argbuf, 0, '|', sizeof internet_addr);

	if (CtdlDirectoryLookup(citadel_addr, internet_addr, sizeof citadel_addr) != 0) {
		cprintf("%d %s was not found.\n",
			ERROR + NO_SUCH_USER, internet_addr);
		return;
	}

	cprintf("%d %s\n", CIT_OK, citadel_addr);
}

/*
 * Query Directory, in fact an alias to match postfix tcp auth.
 */
void check_get(void) {
	char citadel_addr[256];
	char internet_addr[256];

	char cmdbuf[SIZ];

	time(&CC->lastcmd);
	memset(cmdbuf, 0, sizeof cmdbuf); /* Clear it, just in case */
	if (client_getln(cmdbuf, sizeof cmdbuf) < 1) {
		lprintf(CTDL_CRIT, "Client disconnected: ending session.\n");
		CC->kill_me = 1;
		return;
	}
	lprintf(CTDL_INFO, ": %s\n", cmdbuf);
	while (strlen(cmdbuf) < 3) strcat(cmdbuf, " ");

	if (strcasecmp(cmdbuf, "GET "));
	{

		char *argbuf = &cmdbuf[4];
		//// if (CtdlAccessCheck(ac_logged_in)) return;
		
		extract_token(internet_addr, argbuf, 0, '|', sizeof internet_addr);
		
		if (CtdlDirectoryLookup(citadel_addr, internet_addr, sizeof citadel_addr) != 0) {
			cprintf("500 %s was not found.\r\n",
				internet_addr);
			
		}
		
		else cprintf("200 OK %s\r\n", internet_addr);//,citadel_addr);
	}
	CC->kill_me = 1;
}

void check_get_greeting(void) {
/* dummy function, we have no greeting in this verry simple protocol. */
}


/*
 * We don't know if the Contacts room exists so we just create it at login
 */
void vcard_create_room(void)
{
	struct ctdlroom qr;
	struct visit vbuf;

	/* Create the calendar room if it doesn't already exist */
	create_room(USERCONTACTSROOM, 4, "", 0, 1, 0, VIEW_ADDRESSBOOK);

	/* Set expiration policy to manual; otherwise objects will be lost! */
	if (lgetroom(&qr, USERCONTACTSROOM)) {
		lprintf(CTDL_ERR, "Couldn't get the user CONTACTS room!\n");
		return;
	}
	qr.QRep.expire_mode = EXPIRE_MANUAL;
	qr.QRdefaultview = VIEW_ADDRESSBOOK;	/* 2 = address book view */
	lputroom(&qr);

	/* Set the view to a calendar view */
	CtdlGetRelationship(&vbuf, &CC->user, &qr);
	vbuf.v_view = 2;	/* 2 = address book view */
	CtdlSetRelationship(&vbuf, &CC->user, &qr);

	return;
}




/*
 * When a user logs in...
 */
void vcard_session_login_hook(void) {
	struct vCard *v;

	v = vcard_get_user(&CC->user);
	extract_primary_inet_email(CC->cs_inet_email,
				sizeof CC->cs_inet_email, v);
	vcard_free(v);

	vcard_create_room();
}


/* 
 * Turn an arbitrary RFC822 address into a struct vCard for possible
 * inclusion into an address book.
 */
struct vCard *vcard_new_from_rfc822_addr(char *addr) {
	struct vCard *v;
	char user[256], node[256], name[256], email[256], n[256], uid[256];
	int i;

	v = vcard_new();
	if (v == NULL) return(NULL);

	process_rfc822_addr(addr, user, node, name);
	vcard_set_prop(v, "fn", name, 0);

	vcard_fn_to_n(n, name, sizeof n);
	vcard_set_prop(v, "n", n, 0);

	snprintf(email, sizeof email, "%s@%s", user, node);
	vcard_set_prop(v, "email;internet", email, 0);

	snprintf(uid, sizeof uid, "collected: %s %s@%s", name, user, node);
	for (i=0; i<strlen(uid); ++i) {
		if (isspace(uid[i])) uid[i] = '_';
		uid[i] = tolower(uid[i]);
	}
	vcard_set_prop(v, "UID", uid, 0);

	return(v);
}



/*
 * This is called by store_harvested_addresses() to remove from the
 * list any addresses we already have in our address book.
 */
void strip_addresses_already_have(long msgnum, void *userdata) {
	char *collected_addresses;
	struct CtdlMessage *msg;
	struct vCard *v;
	char *value = NULL;
	int i, j;
	char addr[256], user[256], node[256], name[256];

	collected_addresses = (char *)userdata;

	msg = CtdlFetchMessage(msgnum, 1);
	if (msg == NULL) return;
	v = vcard_load(msg->cm_fields['M']);
	CtdlFreeMessage(msg);

	i = 0;
	while (value = vcard_get_prop(v, "email", 1, i++, 0), value != NULL) {

		for (j=0; j<num_tokens(collected_addresses, ','); ++j) {
			extract_token(addr, collected_addresses, j, ',', sizeof addr);

			/* Remove the address if we already have it! */
			process_rfc822_addr(addr, user, node, name);
			snprintf(addr, sizeof addr, "%s@%s", user, node);
			if (!strcasecmp(value, addr)) {
				remove_token(collected_addresses, j, ',');
			}
		}

	}

	vcard_free(v);
}



/*
 * Back end function for store_harvested_addresses()
 */
void store_this_ha(struct addresses_to_be_filed *aptr) {
	struct CtdlMessage *vmsg = NULL;
	long vmsgnum = (-1L);
	char *ser = NULL;
	struct vCard *v = NULL;
	char recipient[256];
	int i;

	/* First remove any addresses we already have in the address book */
	usergoto(aptr->roomname, 0, 0, NULL, NULL);
	CtdlForEachMessage(MSGS_ALL, 0, "text/x-vcard", NULL,
		strip_addresses_already_have, aptr->collected_addresses);

	if (strlen(aptr->collected_addresses) > 0)
	   for (i=0; i<num_tokens(aptr->collected_addresses, ','); ++i) {

		/* Make a vCard out of each address */
		extract_token(recipient, aptr->collected_addresses, i, ',', sizeof recipient);
		striplt(recipient);
		v = vcard_new_from_rfc822_addr(recipient);
		if (v != NULL) {
			vmsg = malloc(sizeof(struct CtdlMessage));
			memset(vmsg, 0, sizeof(struct CtdlMessage));
			vmsg->cm_magic = CTDLMESSAGE_MAGIC;
			vmsg->cm_anon_type = MES_NORMAL;
			vmsg->cm_format_type = FMT_RFC822;
			vmsg->cm_fields['A'] = strdup("Citadel");
			vmsg->cm_fields['E'] =  strdup(vcard_get_prop(v, "UID", 0, 0, 0));
			ser = vcard_serialize(v);
			if (ser != NULL) {
				vmsg->cm_fields['M'] = malloc(strlen(ser) + 1024);
				sprintf(vmsg->cm_fields['M'],
					"Content-type: text/x-vcard"
					"\r\n\r\n%s\r\n", ser);
				free(ser);
			}
			vcard_free(v);

			lprintf(CTDL_DEBUG, "Adding contact: %s\n", recipient);
			vmsgnum = CtdlSubmitMsg(vmsg, NULL, aptr->roomname);
			CtdlFreeMessage(vmsg);
		}
	}

	free(aptr->roomname);
	free(aptr->collected_addresses);
	free(aptr);
}


/*
 * When a user sends a message, we may harvest one or more email addresses
 * from the recipient list to be added to the user's address book.  But we
 * want to do this asynchronously so it doesn't keep the user waiting.
 */
void store_harvested_addresses(void) {

	struct addresses_to_be_filed *aptr = NULL;

	if (atbf == NULL) return;

	begin_critical_section(S_ATBF);
	while (atbf != NULL) {
		aptr = atbf;
		atbf = atbf->next;
		end_critical_section(S_ATBF);
		store_this_ha(aptr);
		begin_critical_section(S_ATBF);
	}
	end_critical_section(S_ATBF);
}


/* 
 * Function to output vCard data as plain text.  Nobody uses MSG0 anymore, so
 * really this is just so we expose the vCard data to the full text indexer.
 */
void vcard_fixed_output(char *ptr, int len) {
	char *serialized_vcard;
	struct vCard *v;
	char *key, *value;
	int i = 0;

	serialized_vcard = malloc(len + 1);
	safestrncpy(serialized_vcard, ptr, len+1);
	v = vcard_load(serialized_vcard);
	free(serialized_vcard);

	i = 0;
	while (key = vcard_get_prop(v, "", 0, i, 1), key != NULL) {
		value = vcard_get_prop(v, "", 0, i++, 0);
		cprintf("%s\n", value);
	}

	vcard_free(v);
}



char *serv_vcard_init(void)
{
	struct ctdlroom qr;
	char filename[256];
	FILE *fp;

	CtdlRegisterSessionHook(vcard_session_login_hook, EVT_LOGIN);
	CtdlRegisterMessageHook(vcard_upload_beforesave, EVT_BEFORESAVE);
	CtdlRegisterMessageHook(vcard_upload_aftersave, EVT_AFTERSAVE);
	CtdlRegisterDeleteHook(vcard_delete_remove);
	CtdlRegisterProtoHook(cmd_regi, "REGI", "Enter registration info");
	CtdlRegisterProtoHook(cmd_greg, "GREG", "Get registration info");
	CtdlRegisterProtoHook(cmd_igab, "IGAB",
					"Initialize Global Address Book");
	CtdlRegisterProtoHook(cmd_qdir, "QDIR", "Query Directory");
	CtdlRegisterUserHook(vcard_newuser, EVT_NEWUSER);
	CtdlRegisterUserHook(vcard_purge, EVT_PURGEUSER);
	CtdlRegisterNetprocHook(vcard_extract_from_network);
	CtdlRegisterSessionHook(store_harvested_addresses, EVT_TIMER);
	CtdlRegisterFixedOutputHook("text/x-vcard", vcard_fixed_output);

	/* Create the Global ADdress Book room if necessary */
	create_room(ADDRESS_BOOK_ROOM, 3, "", 0, 1, 0, VIEW_ADDRESSBOOK);

	/* Set expiration policy to manual; otherwise objects will be lost! */
	if (!lgetroom(&qr, ADDRESS_BOOK_ROOM)) {
		qr.QRep.expire_mode = EXPIRE_MANUAL;
		qr.QRdefaultview = VIEW_ADDRESSBOOK;	/* 2 = address book view */
		lputroom(&qr);

		/*
		 * Also make sure it has a netconfig file, so the networker runs
		 * on this room even if we don't share it with any other nodes.
		 * This allows the CANCEL messages (i.e. "Purge this vCard") to be
		 * purged.
		 */
		assoc_file_name(filename, sizeof filename, &qr, ctdl_netcfg_dir);
		fp = fopen(filename, "a");
		if (fp != NULL) fclose(fp);
		chown(filename, CTDLUID, (-1));
	}

	return "$Id$";
}


char *serv_postfix_tcpdict(void)
{
	CtdlRegisterServiceHook(config.c_pftcpdict_port,	/* Postfix */
				NULL,
				check_get_greeting,
				check_get,
				NULL);
	return "$Id$";
}
