/*
 * $Id$
 * 
 * A server-side module for Citadel which supports address book information
 * using the standard vCard format.
 * 
 * Copyright (c) 1999-2001 / released under the GNU General Public License
 */

/*
 * Where we keep messages containing the vCards that source our directory.  It
 * makes no sense to change this, because you'd have to change it on every
 * system on the network.  That would be stupid.
 */
#define ADDRESS_BOOK_ROOM	"Global Address Book"

/*
 * Format of the "Extended ID" field of the message containing a user's
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
#include "dynloader.h"
#include "room_ops.h"
#include "user_ops.h"
#include "policy.h"
#include "database.h"
#include "msgbase.h"
#include "internet_addressing.h"
#include "tools.h"
#include "vcard.h"

struct vcard_internal_info {
	long msgnum;
};

/* Message number symbol used internally by these functions */
unsigned long SYM_VCARD;
#define VC ((struct vcard_internal_info *)CtdlGetUserData(SYM_VCARD))


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
	sprintf(citadel_address, "%s @ %s",
		msg->cm_fields['A'], msg->cm_fields['N']);

	v = vcard_load(msg->cm_fields['M']);
	if (v == NULL) return;

	/* Go through the vCard searching for *all* instances of
	 * the "email;internet" key
	 */
	do {
		s = vcard_get_prop(v, "email;internet", 0, instance++);
		if (s != NULL) {
			addr = strdoop(s);
			striplt(addr);
			if (strlen(addr) > 0) {
				if (callback != NULL) {
					callback(addr, citadel_address);
				}
			}
			phree(addr);
			found_something = 1;
		}
		else {
			found_something = 0;
		}
	} while(found_something);

	vcard_free(v);
}

/*
 * Back end function for cmd_igab()
 */
void vcard_add_to_directory(long msgnum, void *data) {
	struct CtdlMessage *msg;

	msg = CtdlFetchMessage(msgnum);
	if (msg != NULL) {
		vcard_extract_internet_addresses(msg, CtdlDirectoryAddUser);
	}

	CtdlFreeMessage(msg);
}


/*
 * Initialize Global Adress Book
 */
void cmd_igab(char *argbuf) {
        char hold_rm[ROOMNAMELEN];

	if (CtdlAccessCheck(ac_aide)) return;

        strcpy(hold_rm, CC->quickroom.QRname);	/* save current room */

        if (getroom(&CC->quickroom, ADDRESS_BOOK_ROOM) != 0) {
                getroom(&CC->quickroom, hold_rm);
		cprintf("%d cannot get address book room\n", ERROR);
		return;
        }

	/* Empty the existing database first.
	 */
	CtdlDirectoryInit();

        /* We want the last (and probably only) vcard in this room */
        CtdlForEachMessage(MSGS_ALL, 0, (-127), "text/x-vcard",
		NULL, vcard_add_to_directory, NULL);

        getroom(&CC->quickroom, hold_rm);	/* return to saved room */
	cprintf("%d Directory has been rebuilt.\n", OK);
}



/*
 * This handler detects whether the user is attempting to save a new
 * vCard as part of his/her personal configuration, and handles the replace
 * function accordingly (delete the user's existing vCard in the config room
 * and in the global address book).
 */
int vcard_upload_beforesave(struct CtdlMessage *msg) {
	char *ptr;
	int linelen;
        char config_rm[ROOMNAMELEN];
	char buf[SIZ];

	if (!CC->logged_in) return(0);	/* Only do this if logged in. */

	/* If this isn't the configuration room, or if this isn't a MIME
	 * message, don't bother.  (Check for NULL room first, otherwise
	 * some messages will cause it to crash!!)
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
			 * delete the old one.
			 */

			/* Delete the user's old vCard.  This would probably
			 * get taken care of by the replication check, but we
			 * want to make sure there is absolutely only one
			 * vCard in the user's config room at all times.
			 * 
			 * FIXME ... this needs to be tweaked to allow an admin
			 * to make changes to another user's vCard instead of
			 * assuming that it's always the user saving his own.
			 */
        		MailboxName(config_rm, &CC->usersupp, USERCONFIGROOM);
			CtdlDeleteMessages(config_rm, 0L, "text/x-vcard");

			/* Set the Extended-ID to a standardized one so the
			 * replication always works correctly
			 */
                        if (msg->cm_fields['E'] != NULL)
                                phree(msg->cm_fields['E']);

                        sprintf(buf, VCARD_EXT_FORMAT,
                                msg->cm_fields['A'], NODENAME);
                        msg->cm_fields['E'] = strdoop(buf);

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

			/* Put it in the Global Address Book room... */
			CtdlSaveMsgPointerInRoom(ADDRESS_BOOK_ROOM, I,
				(SM_VERIFY_GOODNESS | SM_DO_REPL_CHECK) );

			/* ...and also in the directory database. */
			vcard_add_to_directory(I, NULL);

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
void vcard_gu_backend(long msgnum, void *userdata) {
	VC->msgnum = msgnum;
}


/*
 * If this user has a vcard on disk, read it into memory, otherwise allocate
 * and return an empty vCard.
 */
struct vCard *vcard_get_user(struct usersupp *u) {
        char hold_rm[ROOMNAMELEN];
        char config_rm[ROOMNAMELEN];
	struct CtdlMessage *msg;
	struct vCard *v;

        strcpy(hold_rm, CC->quickroom.QRname);	/* save current room */
        MailboxName(config_rm, u, USERCONFIGROOM);

        if (getroom(&CC->quickroom, config_rm) != 0) {
                getroom(&CC->quickroom, hold_rm);
                return vcard_new();
        }

        /* We want the last (and probably only) vcard in this room */
	VC->msgnum = (-1);
        CtdlForEachMessage(MSGS_LAST, 1, (-127), "text/x-vcard",
		NULL, vcard_gu_backend, NULL);
        getroom(&CC->quickroom, hold_rm);	/* return to saved room */

	if (VC->msgnum < 0L) return vcard_new();

	msg = CtdlFetchMessage(VC->msgnum);
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
void vcard_write_user(struct usersupp *u, struct vCard *v) {
        char temp[PATH_MAX];
        FILE *fp;
	char *ser;

        strcpy(temp, tmpnam(NULL));
	ser = vcard_serialize(v);

        fp = fopen(temp, "w");
        if (fp == NULL) return;
	if (ser == NULL) {
		fprintf(fp, "begin:vcard\r\nend:vcard\r\n");
	} else {
		fwrite(ser, strlen(ser), 1, fp);
		phree(ser);
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

	if (!(CC->logged_in)) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
	}

	my_vcard = vcard_get_user(&CC->usersupp);
	strcpy(tmpaddr, "");
	strcpy(tmpcity, "");
	strcpy(tmpstate, "");
	strcpy(tmpzip, "");
	strcpy(tmpcountry, "USA");

	cprintf("%d Send registration...\n", SEND_LISTING);
	a=0;
	while (client_gets(buf), strcmp(buf,"000")) {
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

	sprintf(tmpaddress, ";;%s;%s;%s;%s;%s",
		tmpaddr, tmpcity, tmpstate, tmpzip, tmpcountry);
	vcard_set_prop(my_vcard, "adr", tmpaddress, 0);
	vcard_write_user(&CC->usersupp, my_vcard);
	vcard_free(my_vcard);

	lgetuser(&CC->usersupp, CC->curr_user);
	CC->usersupp.flags=(CC->usersupp.flags|US_REGIS|US_NEEDVALID);
	lputuser(&CC->usersupp);

	/* set global flag calling for validation */
	begin_critical_section(S_CONTROL);
	get_control();
	CitControl.MMflags = CitControl.MMflags | MM_VALID ;
	put_control();
	end_critical_section(S_CONTROL);
}



/*
 * Protocol command to fetch registration info for a user
 */
void cmd_greg(char *argbuf)
{
	struct usersupp usbuf;
	struct vCard *v;
	char *s;
	char who[SIZ];
	char adr[SIZ];
	char buf[SIZ];

	extract(who, argbuf, 0);

	if (!(CC->logged_in)) {
		cprintf("%d Not logged in.\n", ERROR+NOT_LOGGED_IN);
		return;
	}

	if (!strcasecmp(who,"_SELF_")) strcpy(who,CC->curr_user);

	if ((CC->usersupp.axlevel < 6) && (strcasecmp(who,CC->curr_user))) {
		cprintf("%d Higher access required.\n",
			ERROR+HIGHER_ACCESS_REQUIRED);
		return;
	}

	if (getuser(&usbuf, who) != 0) {
		cprintf("%d '%s' not found.\n", ERROR+NO_SUCH_USER, who);
		return;
	}

	v = vcard_get_user(&usbuf);

	cprintf("%d %s\n", LISTING_FOLLOWS, usbuf.fullname);
	cprintf("%ld\n", usbuf.usernum);
	cprintf("%s\n", usbuf.password);
	s = vcard_get_prop(v, "n", 0, 0);
	cprintf("%s\n", s ? s : " ");	/* name */

	s = vcard_get_prop(v, "adr", 0, 0);
	sprintf(adr, "%s", s ? s : " ");/* address... */

	extract_token(buf, adr, 2, ';');
	cprintf("%s\n", buf);				/* street */
	extract_token(buf, adr, 3, ';');
	cprintf("%s\n", buf);				/* city */
	extract_token(buf, adr, 4, ';');
	cprintf("%s\n", buf);				/* state */
	extract_token(buf, adr, 5, ';');
	cprintf("%s\n", buf);				/* zip */

	s = vcard_get_prop(v, "tel;home", 0, 0);
	if (s == NULL) s = vcard_get_prop(v, "tel", 1, 0);
	if (s != NULL) {
		cprintf("%s\n", s);
	}
	else {
		cprintf(" \n");
	}

	cprintf("%d\n", usbuf.axlevel);

	s = vcard_get_prop(v, "email;internet", 0, 0);
	cprintf("%s\n", s ? s : " ");
	s = vcard_get_prop(v, "adr", 0, 0);
	sprintf(adr, "%s", s ? s : " ");/* address... */

	extract_token(buf, adr, 6, ';');
	cprintf("%s\n", buf);				/* country */
	cprintf("000\n");
}


/*
 * When a user is being deleted, we have to remove his/her vCard.
 * This is accomplished by issuing a message with 'CANCEL' in the S (special)
 * field, and the same Extended ID as the existing card.
 */
void vcard_purge(char *username, long usernum) {
	struct CtdlMessage *msg;
	char buf[SIZ];

	msg = (struct CtdlMessage *) mallok(sizeof(struct CtdlMessage));
	if (msg == NULL) return;
	memset(msg, 0, sizeof(struct CtdlMessage));

        msg->cm_magic = CTDLMESSAGE_MAGIC;
        msg->cm_anon_type = MES_NORMAL;
        msg->cm_format_type = 0;
        msg->cm_fields['A'] = strdoop(username);
        msg->cm_fields['O'] = strdoop(ADDRESS_BOOK_ROOM);
        msg->cm_fields['N'] = strdoop(NODENAME);
        msg->cm_fields['M'] = strdoop("Purge this vCard\n");

        sprintf(buf, VCARD_EXT_FORMAT, msg->cm_fields['A'], NODENAME);
        msg->cm_fields['E'] = strdoop(buf);

	msg->cm_fields['S'] = strdoop("CANCEL");

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
 * Session startup, allocate some per-session data
 */
void vcard_session_startup_hook(void) {
	CtdlAllocUserData(SYM_VCARD, sizeof(struct vcard_internal_info));
}


char *Dynamic_Module_Init(void)
{
	SYM_VCARD = CtdlGetDynamicSymbol();
	CtdlRegisterSessionHook(vcard_session_startup_hook, EVT_START);
	CtdlRegisterMessageHook(vcard_upload_beforesave, EVT_BEFORESAVE);
	CtdlRegisterMessageHook(vcard_upload_aftersave, EVT_AFTERSAVE);
	CtdlRegisterProtoHook(cmd_regi, "REGI", "Enter registration info");
	CtdlRegisterProtoHook(cmd_greg, "GREG", "Get registration info");
	CtdlRegisterProtoHook(cmd_igab, "IGAB",
					"Initialize Global Address Book");
	CtdlRegisterUserHook(vcard_purge, EVT_PURGEUSER);
	create_room(ADDRESS_BOOK_ROOM, 3, "", 0, 1);
	CtdlRegisterNetprocHook(vcard_extract_from_network);
	return "$Id$";
}
