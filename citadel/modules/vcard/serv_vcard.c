/*
 * A server-side module for Citadel which supports address book information
 * using the standard vCard format.
 * 
 * Copyright (c) 1999-2016 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*
 * Format of the "Exclusive ID" field of the message containing a user's
 * vCard.  Doesn't matter what it really looks like as long as it's both
 * unique and consistent (because we use it for replication checking to
 * delete the old vCard network-wide when the user enters a new one).
 */
#define VCARD_EXT_FORMAT	"Citadel vCard: personal card for %s at %s"

/*
 * Citadel will accept either text/vcard or text/x-vcard as the MIME type
 * for a vCard.  The following definition determines which one it *generates*
 * when serializing.
 */
#define VCARD_MIME_TYPE		"text/x-vcard"

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
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "control.h"
#include "user_ops.h"
#include "database.h"
#include "msgbase.h"
#include "room_ops.h"
#include "internet_addressing.h"
#include "serv_vcard.h"
#include "citadel_ldap.h"
#include "ctdl_module.h"

/*
 * set global flag calling for an aide to validate new users
 */
void set_mm_valid(void) {
	int flags = 0;

	begin_critical_section(S_CONTROL);
	flags = CtdlGetConfigInt("MMflags");
	flags = flags | MM_VALID ;
	CtdlSetConfigInt("MMflags", flags);
	end_critical_section(S_CONTROL);
}



/*
 * Extract Internet e-mail addresses from a message containing a vCard, and
 * perform a callback for any found.
 */
void vcard_extract_internet_addresses(struct CtdlMessage *msg, int (*callback)(char *, char *) ) {
	struct vCard *v;
	char *s;
	char *k;
	char *addr;
	char citadel_address[SIZ];
	int instance = 0;
	int found_something = 0;

	if (CM_IsEmpty(msg, eAuthor)) return;
	if (CM_IsEmpty(msg, eNodeName)) return;
	snprintf(citadel_address, sizeof citadel_address, "%s @ %s",
		msg->cm_fields[eAuthor], msg->cm_fields[eNodeName]);

	v = vcard_load(msg->cm_fields[eMesageText]);
	if (v == NULL) return;

	/* Go through the vCard searching for *all* instances of
	 * the "email;internet" key
	 */
	do {
		s = vcard_get_prop(v, "email", 1, instance, 0);		/* get any 'email' field */
		k = vcard_get_prop(v, "email", 1, instance++, 1);	/* but also learn it with attrs */
		if ( (s != NULL) && (k != NULL) && (bmstrcasestr(k, "internet")) ) {
			addr = strdup(s);
			striplt(addr);
			if (!IsEmptyStr(addr)) {
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
///TODO: gettext!
#define _(a) a
/*
 * Callback for vcard_add_to_directory()
 * (Lotsa ugly nested callbacks.  Oh well.)
 */
int vcard_directory_add_user(char *internet_addr, char *citadel_addr) {
	struct CitContext *CCC = CC;
	char buf[SIZ];

	/* We have to validate that we're not stepping on someone else's
	 * email address ... but only if we're logged in.  Otherwise it's
	 * probably just the networker or something.
	 */
	if (CCC->logged_in) {
		syslog(LOG_DEBUG, "Checking for <%s>...", internet_addr);
		if (CtdlDirectoryLookup(buf, internet_addr, sizeof buf) == 0) {
			if (strcasecmp(buf, citadel_addr)) {
				/* This address belongs to someone else.
				 * Bail out silently without saving.
				 */
				syslog(LOG_DEBUG, "DOOP!");
				
				StrBufAppendPrintf(CCC->StatusMessage, "\n%d|", ERROR+ALREADY_EXISTS);
				StrBufAppendBufPlain(CCC->StatusMessage, internet_addr, -1, 0);
				StrBufAppendBufPlain(CCC->StatusMessage, HKEY("|"), 0);
				StrBufAppendBufPlain(CCC->StatusMessage, _("Unable to add this email address again."), -1, 0);
				StrBufAppendBufPlain(CCC->StatusMessage, HKEY("\n"), 0);
				return 0;
			}
		}
	}
	syslog(LOG_INFO, "Adding %s (%s) to directory", citadel_addr, internet_addr);
	if (CtdlDirectoryAddUser(internet_addr, citadel_addr))
	{
		StrBufAppendPrintf(CCC->StatusMessage, "\n%d|", CIT_OK);
		StrBufAppendBufPlain(CCC->StatusMessage, internet_addr, -1, 0);
		StrBufAppendBufPlain(CCC->StatusMessage, HKEY("|"), 0);
		StrBufAppendBufPlain(CCC->StatusMessage, _("Successfully added email address."), -1, 0);
		return 1;
	}
	else
	{
		StrBufAppendPrintf(CCC->StatusMessage, "\n%d|", ERROR+ ILLEGAL_VALUE);
		StrBufAppendBufPlain(CCC->StatusMessage, internet_addr, -1, 0);
		StrBufAppendBufPlain(CCC->StatusMessage, HKEY("|"), 0);
		StrBufAppendBufPlain(CCC->StatusMessage, _("Unable to add this email address. It does not match any local domain."), -1, 0);
		return 0;
	}
}


/*
 * Back end function for cmd_igab()
 */
void vcard_add_to_directory(long msgnum, void *data) {
	struct CtdlMessage *msg;

	msg = CtdlFetchMessage(msgnum, 1, 1);
	if (msg != NULL) {
		vcard_extract_internet_addresses(msg, vcard_directory_add_user);
	}

	CM_Free(msg);
}


/*
 * Initialize Global Adress Book
 */
void cmd_igab(char *argbuf) {
	char hold_rm[ROOMNAMELEN];

	if (CtdlAccessCheck(ac_aide)) return;

	strcpy(hold_rm, CC->room.QRname);	/* save current room */

	if (CtdlGetRoom(&CC->room, ADDRESS_BOOK_ROOM) != 0) {
		CtdlGetRoom(&CC->room, hold_rm);
		cprintf("%d cannot get address book room\n", ERROR + ROOM_NOT_FOUND);
		return;
	}

	/* Empty the existing database first.
	 */
	CtdlDirectoryInit();

	/* We want *all* vCards in this room */
	NewStrBufDupAppendFlush(&CC->StatusMessage, NULL, NULL, 0);
	CtdlForEachMessage(MSGS_ALL, 0, NULL, "[Tt][Ee][Xx][Tt]/.*[Vv][Cc][Aa][Rr][Dd]$",
		NULL, vcard_add_to_directory, NULL);

	CtdlGetRoom(&CC->room, hold_rm);	/* return to saved room */
	cprintf("%d Directory has been rebuilt.\n", CIT_OK);
}




/*
 * See if there is a valid Internet address in a vCard to use for outbound
 * Internet messages.  If there is, stick it in the buffer.
 */
void extract_inet_email_addrs(char *emailaddrbuf, size_t emailaddrbuf_len,
			      char *secemailaddrbuf, size_t secemailaddrbuf_len,
			      struct vCard *v,
			      int local_addrs_only)
{
	struct CitContext *CCC = CC;		/* put this on the stack, just for speed */
	char *s, *k, *addr;
	int instance = 0;
	int IsDirectoryAddress;
	int saved_instance = 0;

	/* Go through the vCard searching for *all* Internet email addresses
	 */
	while (s = vcard_get_prop(v, "email", 1, instance, 0),  s != NULL) {
		k = vcard_get_prop(v, "email", 1, instance, 1);
		if ( (s != NULL) && (k != NULL) && (bmstrcasestr(k, "internet")) ) {
			addr = strdup(s);
			striplt(addr);
			if (!IsEmptyStr(addr)) {
				IsDirectoryAddress = IsDirectory(addr, 1);
				if ( IsDirectoryAddress || !local_addrs_only)
				{
					++saved_instance;
					if ((saved_instance == 1) && (emailaddrbuf != NULL)) {
						safestrncpy(emailaddrbuf, addr, emailaddrbuf_len);
					}
					else if ((saved_instance == 2) && (secemailaddrbuf != NULL)) {
						safestrncpy(secemailaddrbuf, addr, secemailaddrbuf_len);
					}
					else if ((saved_instance > 2) && (secemailaddrbuf != NULL)) {
						if ( (strlen(addr) + strlen(secemailaddrbuf) + 2) 
					   	< secemailaddrbuf_len ) {
							strcat(secemailaddrbuf, "|");
							strcat(secemailaddrbuf, addr);
						}
					}
				}
				if (!IsDirectoryAddress && local_addrs_only)
				{
					StrBufAppendPrintf(CCC->StatusMessage, "\n%d|", ERROR+ ILLEGAL_VALUE);
					StrBufAppendBufPlain(CCC->StatusMessage, addr, -1, 0);
					StrBufAppendBufPlain(CCC->StatusMessage, HKEY("|"), 0);
					StrBufAppendBufPlain(CCC->StatusMessage, _("unable to add this emailaddress; its not matching our domain."), -1, 0);
				}
			}
			free(addr);
		}
		++instance;
	}
}



/*
 * See if there is a name / screen name / friendly name  in a vCard to use for outbound
 * Internet messages.  If there is, stick it in the buffer.
 */
void extract_friendly_name(char *namebuf, size_t namebuf_len, struct vCard *v)
{
	char *s;

	s = vcard_get_prop(v, "fn", 1, 0, 0);
	if (s == NULL) {
		s = vcard_get_prop(v, "n", 1, 0, 0);
	}

	if (s != NULL) {
		safestrncpy(namebuf, s, namebuf_len);
	}
}


/*
 * Callback function for vcard_upload_beforesave() hunts for the real vcard in the MIME structure
 */
void vcard_extract_vcard(char *name, char *filename, char *partnum, char *disp,
		   void *content, char *cbtype, char *cbcharset, size_t length,
		   char *encoding, char *cbid, void *cbuserdata)
{
	struct vCard **v = (struct vCard **) cbuserdata;

	if (  (!strcasecmp(cbtype, "text/x-vcard"))
	   || (!strcasecmp(cbtype, "text/vcard")) ) {

		syslog(LOG_DEBUG, "Part %s contains a vCard!  Loading...", partnum);
		if (*v != NULL) {
			vcard_free(*v);
		}
		*v = vcard_load(content);
	}
}


/*
 * This handler detects whether the user is attempting to save a new
 * vCard as part of his/her personal configuration, and handles the replace
 * function accordingly (delete the user's existing vCard in the config room
 * and in the global address book).
 */
int vcard_upload_beforesave(struct CtdlMessage *msg, recptypes *recp) {
	struct CitContext *CCC = CC;
	char *s;
	char buf[SIZ];
	struct ctdluser usbuf;
	long what_user;
	struct vCard *v = NULL;
	char *ser = NULL;
	int i = 0;
	int yes_my_citadel_config = 0;
	int yes_any_vcard_room = 0;

	if ((!CCC->logged_in) && (CCC->vcard_updated_by_ldap==0)) return(0);	/* Only do this if logged in, or if ldap changed the vcard. */

	/* Is this some user's "My Citadel Config" room? */
	if (((CCC->room.QRflags & QR_MAILBOX) != 0) &&
	      (!strcasecmp(&CCC->room.QRname[11], USERCONFIGROOM)) ) {
		/* Yes, we want to do this */
		yes_my_citadel_config = 1;
#ifdef VCARD_SAVES_BY_AIDES_ONLY
		/* Prevent non-aides from performing registration changes, but ldap is ok. */
		if ((CCC->user.axlevel < AxAideU) && (CCC->vcard_updated_by_ldap==0)) {
			return(1);
		}
#endif

	}

	/* Is this a room with an address book in it? */
	if (CCC->room.QRdefaultview == VIEW_ADDRESSBOOK) {
		yes_any_vcard_room = 1;
	}

	/* If neither condition exists, don't run this hook. */
	if ( (!yes_my_citadel_config) && (!yes_any_vcard_room) ) {
		return(0);
	}

	/* If this isn't a MIME message, don't bother. */
	if (msg->cm_format_type != 4) return(0);

	/* Ok, if we got this far, look into the situation further... */

	if (CM_IsEmpty(msg, eMesageText)) return(0);

	mime_parser(CM_RANGE(msg, eMesageText),
		    *vcard_extract_vcard,
		    NULL, NULL,
		    &v,		/* user data ptr - put the vcard here */
		    0
	);

	if (v == NULL) return(0);	/* no vCards were found in this message */

	/* If users cannot create their own accounts, they cannot re-register either. */
	if ( (yes_my_citadel_config) &&
	     (CtdlGetConfigInt("c_disable_newu")) &&
	     (CCC->user.axlevel < AxAideU) &&
	     (CCC->vcard_updated_by_ldap==0) )
	{
		return(1);
	}

	vcard_get_prop(v, "fn", 1, 0, 0);


	if (yes_my_citadel_config) {
		/* Bingo!  The user is uploading a new vCard, so
		 * delete the old one.  First, figure out which user
		 * is being re-registered...
		 */
		what_user = atol(CCC->room.QRname);

		if (what_user == CCC->user.usernum) {
			/* It's the logged in user.  That was easy. */
			memcpy(&usbuf, &CCC->user, sizeof(struct ctdluser));
		}
		
		else if (CtdlGetUserByNumber(&usbuf, what_user) == 0) {
			/* We fetched a valid user record */
		}

		else {
			/* somebody set up us the bomb! */
			yes_my_citadel_config = 0;
		}
	}
	
	if (yes_my_citadel_config) {
		/* Delete the user's old vCard.  This would probably
		 * get taken care of by the replication check, but we
		 * want to make sure there is absolutely only one
		 * vCard in the user's config room at all times.
		 *
		 */
		CtdlDeleteMessages(CCC->room.QRname, NULL, 0, "[Tt][Ee][Xx][Tt]/.*[Vv][Cc][Aa][Rr][Dd]$");

		/* Make the author of the message the name of the user. */
		if (!IsEmptyStr(usbuf.fullname)) {
			CM_SetField(msg, eAuthor, usbuf.fullname, strlen(usbuf.fullname));
		}
	}

	/* Insert or replace RFC2739-compliant free/busy URL */
	if (yes_my_citadel_config) {
		sprintf(buf, "http://%s/%s.vfb",
			CtdlGetConfigStr("c_fqdn"),
			usbuf.fullname);
		for (i=0; buf[i]; ++i) {
			if (buf[i] == ' ') buf[i] = '_';
		}
		vcard_set_prop(v, "FBURL;PREF", buf, 0);
	}


	s = vcard_get_prop(v, "UID", 1, 0, 0);
	if (s == NULL) { /* Note LDAP auth sets UID from the LDAP UUID, use that if it exists. */
	  /* Enforce local UID policy if applicable */
	  if (yes_my_citadel_config) {
		snprintf(buf, sizeof buf, VCARD_EXT_FORMAT, msg->cm_fields[eAuthor], NODENAME);
	  } else {
		/* If the vCard has no UID, then give it one. */
		generate_uuid(buf);
	  }
 	  vcard_set_prop(v, "UID", buf, 0);
    }


	/* 
	 * Set the EUID of the message to the UID of the vCard.
	 */
	CM_FlushField(msg, eExclusiveID);

	s = vcard_get_prop(v, "UID", 1, 0, 0);
	if (!IsEmptyStr(s)) {
		CM_SetField(msg, eExclusiveID, s, strlen(s));
		if (CM_IsEmpty(msg, eMsgSubject)) {
			CM_CopyField(msg, eMsgSubject, eExclusiveID);
		}
	}

	/*
	 * Set the Subject to the name in the vCard.
	 */
	s = vcard_get_prop(v, "FN", 1, 0, 0);
	if (s == NULL) {
		s = vcard_get_prop(v, "N", 1, 0, 0);
	}
	if (!IsEmptyStr(s)) {
		CM_SetField(msg, eMsgSubject, s, strlen(s));
	}

	/* Re-serialize it back into the msg body */
	ser = vcard_serialize(v);
	if (!IsEmptyStr(ser)) {
		StrBuf *buf;
		long serlen;

		serlen = strlen(ser);
		buf = NewStrBufPlain(NULL, serlen + 1024);

		StrBufAppendBufPlain(buf, HKEY("Content-type: " VCARD_MIME_TYPE "\r\n\r\n"), 0);
		StrBufAppendBufPlain(buf, ser, serlen, 0);
		StrBufAppendBufPlain(buf, HKEY("\r\n"), 0);
		CM_SetAsFieldSB(msg, eMesageText, &buf);
		free(ser);
	}

	/* Now allow the save to complete. */
	vcard_free(v);
	return(0);
}



/*
 * This handler detects whether the user is attempting to save a new
 * vCard as part of his/her personal configuration, and handles the replace
 * function accordingly (copy the vCard from the config room to the global
 * address book).
 */
int vcard_upload_aftersave(struct CtdlMessage *msg, recptypes *recp) {
	struct CitContext *CCC = CC;
	char *ptr;
	int linelen;
	long I;
	struct vCard *v;
	int is_UserConf=0;
	int is_MY_UserConf=0;
	int is_GAB=0;
	char roomname[ROOMNAMELEN];

	if (msg->cm_format_type != 4) return(0);
	if ((!CCC->logged_in) && (CCC->vcard_updated_by_ldap==0)) return(0);	/* Only do this if logged in, or if ldap changed the vcard. */

	/* We're interested in user config rooms only. */

	if ( !IsEmptyStr(CCC->room.QRname) &&
             (strlen(CCC->room.QRname) >= 12) &&
             (!strcasecmp(&CCC->room.QRname[11], USERCONFIGROOM)) ) {
		is_UserConf = 1;	/* It's someone's config room */
	}
	CtdlMailboxName(roomname, sizeof roomname, &CCC->user, USERCONFIGROOM);
	if (!strcasecmp(CCC->room.QRname, roomname)) {
		is_UserConf = 1;
		is_MY_UserConf = 1;	/* It's MY config room */
	}
	if (!strcasecmp(CCC->room.QRname, ADDRESS_BOOK_ROOM)) {
		is_GAB = 1;		/* It's the Global Address Book */
	}

	if (!is_UserConf && !is_GAB) return(0);

	if (CM_IsEmpty(msg, eMesageText))
		return 0;

	ptr = msg->cm_fields[eMesageText];

	CCC->vcard_updated_by_ldap=0;  /* As this will write LDAP's previous changes, disallow LDAP change auth until next LDAP change. */ 

	NewStrBufDupAppendFlush(&CCC->StatusMessage, NULL, NULL, 0);

	StrBufPrintf(CCC->StatusMessage, "%d\n", LISTING_FOLLOWS);

	while (ptr != NULL) {
	
		linelen = strcspn(ptr, "\n");
		if (linelen == 0) return(0);	/* end of headers */	
		
		if (  (!strncasecmp(ptr, "Content-type: text/x-vcard", 26))
		   || (!strncasecmp(ptr, "Content-type: text/vcard", 24)) ) {
			/*
			 * Bingo!  The user is uploading a new vCard, so
			 * copy it to the Global Address Book room.
			 */

			I = atol(msg->cm_fields[eVltMsgNum]);
			if (I <= 0L) return(0);

			/* Store our Internet return address in memory */
			if (is_MY_UserConf) {
				v = vcard_load(msg->cm_fields[eMesageText]);
				extract_inet_email_addrs(CCC->cs_inet_email, sizeof CCC->cs_inet_email,
						CCC->cs_inet_other_emails, sizeof CCC->cs_inet_other_emails,
						v, 1);
				extract_friendly_name(CCC->cs_inet_fn, sizeof CCC->cs_inet_fn, v);
				vcard_free(v);
			}

			if (!is_GAB)
			{	// This is not the GAB
				/* Put it in the Global Address Book room... */
				CtdlSaveMsgPointerInRoom(ADDRESS_BOOK_ROOM, I, 1, msg);
			}

			/* ...and also in the directory database. */
			vcard_add_to_directory(I, NULL);

			/* Some sites want an Aide to be notified when a
			 * user registers or re-registers
			 * But if the user was an Aide or was edited by an Aide then we can
			 * Assume they don't need validating.
			 */
			if (CCC->user.axlevel >= AxAideU) {
				CtdlLockGetCurrentUser();
				CCC->user.flags |= US_REGIS;
				CtdlPutCurrentUserLock();
				return (0);
			}
			
			set_mm_valid();

			/* ...which also means we need to flag the user */
			CtdlLockGetCurrentUser();
			CCC->user.flags |= (US_REGIS|US_NEEDVALID);
			CtdlPutCurrentUserLock();

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
	struct CitContext *CCC = CC;
	char hold_rm[ROOMNAMELEN];
	char config_rm[ROOMNAMELEN];
	struct CtdlMessage *msg = NULL;
	struct vCard *v;
	long VCmsgnum;

	strcpy(hold_rm, CCC->room.QRname);	/* save current room */
	CtdlMailboxName(config_rm, sizeof config_rm, u, USERCONFIGROOM);

	if (CtdlGetRoom(&CCC->room, config_rm) != 0) {
		CtdlGetRoom(&CCC->room, hold_rm);
		return vcard_new();
	}

	/* We want the last (and probably only) vcard in this room */
	VCmsgnum = (-1);
	CtdlForEachMessage(MSGS_LAST, 1, NULL, "[Tt][Ee][Xx][Tt]/.*[Vv][Cc][Aa][Rr][Dd]$",
		NULL, vcard_gu_backend, (void *)&VCmsgnum );
	CtdlGetRoom(&CCC->room, hold_rm);	/* return to saved room */

	if (VCmsgnum < 0L) return vcard_new();

	msg = CtdlFetchMessage(VCmsgnum, 1, 1);
	if (msg == NULL) return vcard_new();

	v = vcard_load(msg->cm_fields[eMesageText]);
	CM_Free(msg);
	return v;
}


/*
 * Store this user's vCard in the appropriate place
 */
/*
 * Write our config to disk
 */
void vcard_write_user(struct ctdluser *u, struct vCard *v) {
	char *ser;

	ser = vcard_serialize(v);
	if (ser == NULL) {
		ser = strdup("begin:vcard\r\nend:vcard\r\n");
	}
	if (ser == NULL) return;

	/* This handy API function does all the work for us.
	 * NOTE: normally we would want to set that last argument to 1, to
	 * force the system to delete the user's old vCard.  But it doesn't
	 * have to, because the vcard_upload_beforesave() hook above
	 * is going to notice what we're trying to do, and delete the old vCard.
	 */
	CtdlWriteObject(USERCONFIGROOM,		/* which room */
			VCARD_MIME_TYPE,	/* MIME type */
			ser,			/* data */
			strlen(ser)+1,		/* length */
			u,			/* which user */
			0,			/* not binary */
			0,			/* don't delete others of this type */
			0);			/* no flags */

	free(ser);
}



/*
 * Old style "enter registration info" command.  This function simply honors
 * the REGI protocol command, translates the entered parameters into a vCard,
 * and enters the vCard into the user's configuration.
 */
void cmd_regi(char *argbuf) {
	struct CitContext *CCC = CC;
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

	if (!(CCC->logged_in)) {
		cprintf("%d Not logged in.\n",ERROR + NOT_LOGGED_IN);
		return;
	}

	/* If users cannot create their own accounts, they cannot re-register either. */
	if ( (CtdlGetConfigInt("c_disable_newu")) && (CCC->user.axlevel < AxAideU) ) {
		cprintf("%d Self-service registration is not allowed here.\n",
			ERROR + HIGHER_ACCESS_REQUIRED);
	}

	my_vcard = vcard_get_user(&CCC->user);
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
			for (c=0; buf[c]; ++c) {
				if ((buf[c]>='0') && (buf[c]<='9')) {
					b = strlen(tmpzip);
					tmpzip[b] = buf[c];
					tmpzip[b+1] = 0;
				}
			}
		}
		if (a==5) vcard_set_prop(my_vcard, "tel", buf, 0);
		if (a==6) vcard_set_prop(my_vcard, "email;internet", buf, 0);
		if (a==7) strcpy(tmpcountry, buf);
		++a;
	}

	snprintf(tmpaddress, sizeof tmpaddress, ";;%s;%s;%s;%s;%s",
		tmpaddr, tmpcity, tmpstate, tmpzip, tmpcountry);
	vcard_set_prop(my_vcard, "adr", tmpaddress, 0);
	vcard_write_user(&CCC->user, my_vcard);
	vcard_free(my_vcard);
}


/*
 * Protocol command to fetch registration info for a user
 */
void cmd_greg(char *argbuf)
{
	struct CitContext *CCC = CC;
	struct ctdluser usbuf;
	struct vCard *v;
	char *s;
	char who[USERNAME_SIZE];
	char adr[256];
	char buf[256];

	extract_token(who, argbuf, 0, '|', sizeof who);

	if (!(CCC->logged_in)) {
		cprintf("%d Not logged in.\n", ERROR + NOT_LOGGED_IN);
		return;
	}

	if (!strcasecmp(who,"_SELF_")) strcpy(who,CCC->curr_user);

	if ((CCC->user.axlevel < AxAideU) && (strcasecmp(who,CCC->curr_user))) {
		cprintf("%d Higher access required.\n",
			ERROR + HIGHER_ACCESS_REQUIRED);
		return;
	}

	if (CtdlGetUser(&usbuf, who) != 0) {
		cprintf("%d '%s' not found.\n", ERROR + NO_SUCH_USER, who);
		return;
	}

	v = vcard_get_user(&usbuf);

	cprintf("%d %s\n", LISTING_FOLLOWS, usbuf.fullname);
	cprintf("%ld\n", usbuf.usernum);
	cprintf("%s\n", usbuf.password);
	s = vcard_get_prop(v, "n", 1, 0, 0);
	cprintf("%s\n", s ? s : " ");	/* name */

	s = vcard_get_prop(v, "adr", 1, 0, 0);
	snprintf(adr, sizeof adr, "%s", s ? s : " ");/* address... */

	extract_token(buf, adr, 2, ';', sizeof buf);
	cprintf("%s\n", buf);				/* street */
	extract_token(buf, adr, 3, ';', sizeof buf);
	cprintf("%s\n", buf);				/* city */
	extract_token(buf, adr, 4, ';', sizeof buf);
	cprintf("%s\n", buf);				/* state */
	extract_token(buf, adr, 5, ';', sizeof buf);
	cprintf("%s\n", buf);				/* zip */

	s = vcard_get_prop(v, "tel", 1, 0, 0);
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
	vcard_free(v);
}



/*
 * When a user is being created, create his/her vCard.
 */
void vcard_newuser(struct ctdluser *usbuf) {
	char vname[256];
	char buf[256];
	int i;
	struct vCard *v;
	int need_default_vcard;

	need_default_vcard =1;
	vcard_fn_to_n(vname, usbuf->fullname, sizeof vname);
	syslog(LOG_DEBUG, "Converted <%s> to <%s>", usbuf->fullname, vname);

	/* Create and save the vCard */
	v = vcard_new();
	if (v == NULL) return;
	vcard_add_prop(v, "fn", usbuf->fullname);
	vcard_add_prop(v, "n", vname);
	vcard_add_prop(v, "adr", "adr:;;_;_;_;00000;__");

#ifdef HAVE_GETPWUID_R
	/* If using host auth mode, we add an email address based on the login */
	if (CtdlGetConfigInt("c_auth_mode") == AUTHMODE_HOST) {
		struct passwd pwd;
		char pwd_buffer[SIZ];
		
#ifdef SOLARIS_GETPWUID
		if (getpwuid_r(usbuf->uid, &pwd, pwd_buffer, sizeof pwd_buffer) != NULL) {
#else // SOLARIS_GETPWUID
		struct passwd *result = NULL;
		syslog(LOG_DEBUG, "Searching for uid %d", usbuf->uid);
		if (getpwuid_r(usbuf->uid, &pwd, pwd_buffer, sizeof pwd_buffer, &result) == 0) {
#endif // HAVE_GETPWUID_R
			snprintf(buf, sizeof buf, "%s@%s", pwd.pw_name, CtdlGetConfigStr("c_fqdn"));
			vcard_add_prop(v, "email;internet", buf);
			need_default_vcard = 0;
		}
	}
#endif


#ifdef HAVE_LDAP
	/*
	 * Is this an LDAP session?  If so, copy various LDAP attributes from the directory entry
	 * into the user's vCard.
	 */
	if ((CtdlGetConfigInt("c_auth_mode") == AUTHMODE_LDAP) || (CtdlGetConfigInt("c_auth_mode") == AUTHMODE_LDAP_AD)) {
            //uid_t ldap_uid;
	    int found_user;
            char ldap_cn[512];
            char ldap_dn[512];
	    found_user = CtdlTryUserLDAP(usbuf->fullname, ldap_dn, sizeof ldap_dn, ldap_cn, sizeof ldap_cn, &usbuf->uid,1);
            if (found_user == 0) {
		if (Ctdl_LDAP_to_vCard(ldap_dn, v)) {
			/* Allow global address book and internet directory update without login long enough to write this. */
			CC->vcard_updated_by_ldap++;  /* Otherwise we'll only update the user config. */
			need_default_vcard = 0;
			syslog(LOG_DEBUG, "LDAP Created Initial Vcard for %s\n",usbuf->fullname);
		}
	    }
	}
#endif
	if (need_default_vcard!=0) {
	  /* Everyone gets an email address based on their display name */
	  snprintf(buf, sizeof buf, "%s@%s", usbuf->fullname, CtdlGetConfigStr("c_fqdn"));
	  for (i=0; buf[i]; ++i) {
		if (buf[i] == ' ') buf[i] = '_';
	  }
	  vcard_add_prop(v, "email;internet", buf);
    }

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
	long len;

	msg = (struct CtdlMessage *) malloc(sizeof(struct CtdlMessage));
	if (msg == NULL) return;
	memset(msg, 0, sizeof(struct CtdlMessage));

	msg->cm_magic = CTDLMESSAGE_MAGIC;
	msg->cm_anon_type = MES_NORMAL;
	msg->cm_format_type = 0;
	if (!IsEmptyStr(usbuf->fullname)) {
		CM_SetField(msg, eAuthor, usbuf->fullname, strlen(usbuf->fullname));
	}
	CM_SetField(msg, eOriginalRoom, HKEY(ADDRESS_BOOK_ROOM));
	CM_SetField(msg, eNodeName, CtdlGetConfigStr("c_nodename"), strlen(CtdlGetConfigStr("c_nodename")));
	CM_SetField(msg, eMesageText, HKEY("Purge this vCard\n"));

	len = snprintf(buf, sizeof buf, VCARD_EXT_FORMAT,
		       msg->cm_fields[eAuthor], NODENAME);
	CM_SetField(msg, eExclusiveID, buf, len);

	CM_SetField(msg, eSpecialField, HKEY("CANCEL"));

	CtdlSubmitMsg(msg, NULL, ADDRESS_BOOK_ROOM, QP_EADDR);
	CM_Free(msg);
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

	if (CM_IsEmpty(msg, eMesageText))
		return 0;

	ptr = msg->cm_fields[eMesageText];

	while (ptr != NULL) {
	
		linelen = strcspn(ptr, "\n");
		if (linelen == 0) return(0);	/* end of headers */	
		
		if (  (!strncasecmp(ptr, "Content-type: text/x-vcard", 26))
		   || (!strncasecmp(ptr, "Content-type: text/vcard", 24)) ) {
			/* It's a vCard.  Add it to the directory. */
			vcard_extract_internet_addresses(msg, CtdlDirectoryAddUser);
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
	
	if (room == NULL) return;

	if (strcasecmp(room, ADDRESS_BOOK_ROOM)) {
		return;
	}

	msg = CtdlFetchMessage(msgnum, 1, 1);
	if (msg == NULL) return;

	if (CM_IsEmpty(msg, eMesageText))
		goto EOH;

	ptr = msg->cm_fields[eMesageText];

	while (ptr != NULL) {
		linelen = strcspn(ptr, "\n");
		if (linelen == 0) goto EOH;
		
		if (  (!strncasecmp(ptr, "Content-type: text/x-vcard", 26))
		   || (!strncasecmp(ptr, "Content-type: text/vcard", 24)) ) {
			/* Bingo!  A vCard is being deleted. */
			vcard_extract_internet_addresses(msg, CtdlDirectoryDelUser);
		}
		ptr = strchr((char *)ptr, '\n');
		if (ptr != NULL) ++ptr;
	}

EOH:	CM_Free(msg);
}



/*
 * Get Valid Screen Names
 */
void cmd_gvsn(char *argbuf)
{
	struct CitContext *CCC = CC;

	if (CtdlAccessCheck(ac_logged_in)) return;

	cprintf("%d valid screen names:\n", LISTING_FOLLOWS);
	cprintf("%s\n", CCC->user.fullname);
	if ( (!IsEmptyStr(CCC->cs_inet_fn)) && (strcasecmp(CCC->user.fullname, CCC->cs_inet_fn)) ) {
		cprintf("%s\n", CCC->cs_inet_fn);
	}
	cprintf("000\n");
}


/*
 * Get Valid Email Addresses
 */
void cmd_gvea(char *argbuf)
{
	struct CitContext *CCC = CC;
	int num_secondary_emails = 0;
	int i;
	char buf[256];

	if (CtdlAccessCheck(ac_logged_in)) return;

	cprintf("%d valid email addresses:\n", LISTING_FOLLOWS);
	if (!IsEmptyStr(CCC->cs_inet_email)) {
		cprintf("%s\n", CCC->cs_inet_email);
	}
	if (!IsEmptyStr(CCC->cs_inet_other_emails)) {
		num_secondary_emails = num_tokens(CCC->cs_inet_other_emails, '|');
		for (i=0; i<num_secondary_emails; ++i) {
			extract_token(buf, CCC->cs_inet_other_emails,i,'|',sizeof CCC->cs_inet_other_emails);
			cprintf("%s\n", buf);
		}
	}
	cprintf("000\n");
}




/*
 * Callback function for cmd_dvca() that hunts for vCard content types
 * and outputs any email addresses found within.
 */
void dvca_mime_callback(char *name, char *filename, char *partnum, char *disp,
		void *content, char *cbtype, char *cbcharset, size_t length, char *encoding,
		char *cbid, void *cbuserdata) {

	struct vCard *v;
	char displayname[256] = "";
	int displayname_len;
	char emailaddr[256] = "";
	int i;
	int has_commas = 0;

	if ( (strcasecmp(cbtype, "text/vcard")) && (strcasecmp(cbtype, "text/x-vcard")) ) {
		return;
	}

	v = vcard_load(content);
	if (v == NULL) return;

	extract_friendly_name(displayname, sizeof displayname, v);
	extract_inet_email_addrs(emailaddr, sizeof emailaddr, NULL, 0, v, 0);

	displayname_len = strlen(displayname);
	for (i=0; i<displayname_len; ++i) {
		if (displayname[i] == '\"') displayname[i] = ' ';
		if (displayname[i] == ';') displayname[i] = ',';
		if (displayname[i] == ',') has_commas = 1;
	}
	striplt(displayname);

	cprintf("%s%s%s <%s>\n",
		(has_commas ? "\"" : ""),
		displayname,
		(has_commas ? "\"" : ""),
		emailaddr
	);

	vcard_free(v);
}


/*
 * Back end callback function for cmd_dvca()
 *
 * It's basically just passed a list of message numbers, which we're going
 * to fetch off the disk and then pass along to the MIME parser via another
 * layer of callback...
 */
void dvca_callback(long msgnum, void *userdata) {
	struct CtdlMessage *msg = NULL;

	msg = CtdlFetchMessage(msgnum, 1, 1);
	if (msg == NULL) return;
	mime_parser(CM_RANGE(msg, eMesageText),
		    *dvca_mime_callback,	/* callback function */
		    NULL, NULL,
		    NULL,			/* user data */
		    0
		);
	CM_Free(msg);
}


/*
 * Dump VCard Addresses
 */
void cmd_dvca(char *argbuf)
{
	if (CtdlAccessCheck(ac_logged_in)) return;

	cprintf("%d addresses:\n", LISTING_FOLLOWS);
	CtdlForEachMessage(MSGS_ALL, 0, NULL, NULL, NULL, dvca_callback, NULL);
	cprintf("000\n");
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
	char internet_addr[256];

	char cmdbuf[SIZ];

	time(&CC->lastcmd);
	memset(cmdbuf, 0, sizeof cmdbuf); /* Clear it, just in case */
	if (client_getln(cmdbuf, sizeof cmdbuf) < 1) {
		syslog(LOG_CRIT, "vcard client disconnected: ending session.");
		CC->kill_me = KILLME_CLIENT_DISCONNECTED;
		return;
	}
	syslog(LOG_INFO, ": %s", cmdbuf);
	while (strlen(cmdbuf) < 3) strcat(cmdbuf, " ");
	syslog(LOG_INFO, "[ %s]", cmdbuf);
	
	if (strncasecmp(cmdbuf, "GET ", 4)==0)
	{
		recptypes *rcpt;
		char *argbuf = &cmdbuf[4];
		
		extract_token(internet_addr, argbuf, 0, '|', sizeof internet_addr);
		rcpt = validate_recipients(internet_addr, NULL, CHECK_EXISTANCE);
		if ((rcpt != NULL)&&
			(
			 (*rcpt->recp_local != '\0')||
			 (*rcpt->recp_room != '\0')||
			 (*rcpt->recp_ignet != '\0')))
		{

			cprintf("200 OK %s\n", internet_addr);
			syslog(LOG_INFO, "sending 200 OK for the room %s", rcpt->display_recp);
		}
		else 
		{
			cprintf("500 REJECT noone here by that name.\n");
			
			syslog(LOG_INFO, "sending 500 REJECT no one here by that name: %s", internet_addr);
		}
		if (rcpt != NULL) 
			free_recipients(rcpt);
	}
	else {
		cprintf("500 REJECT invalid Query.\n");
		syslog(LOG_INFO, "sending 500 REJECT invalid query: %s", internet_addr);
	}
}

void check_get_greeting(void) {
/* dummy function, we have no greeting in this verry simple protocol. */
}


/*
 * We don't know if the Contacts room exists so we just create it at login
 */
void vcard_CtdlCreateRoom(void)
{
	struct ctdlroom qr;
	visit vbuf;

	/* Create the calendar room if it doesn't already exist */
	CtdlCreateRoom(USERCONTACTSROOM, 4, "", 0, 1, 0, VIEW_ADDRESSBOOK);

	/* Set expiration policy to manual; otherwise objects will be lost! */
	if (CtdlGetRoomLock(&qr, USERCONTACTSROOM)) {
		syslog(LOG_ERR, "Couldn't get the user CONTACTS room!");
		return;
	}
	qr.QRep.expire_mode = EXPIRE_MANUAL;
	qr.QRdefaultview = VIEW_ADDRESSBOOK;	/* 2 = address book view */
	CtdlPutRoomLock(&qr);

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
	struct vCard *v = NULL;
	struct CitContext *CCC = CC;		/* put this on the stack, just for speed */

#ifdef HAVE_LDAP
	/*
	 * Is this an LDAP session?  If so, copy various LDAP attributes from the directory entry
	 * into the user's vCard.
	 */
	if ((CtdlGetConfigInt("c_auth_mode") == AUTHMODE_LDAP) || (CtdlGetConfigInt("c_auth_mode") == AUTHMODE_LDAP_AD)) {
		v = vcard_get_user(&CCC->user);
		if (v) {
			if (Ctdl_LDAP_to_vCard(CCC->ldap_dn, v)) {
				CCC->vcard_updated_by_ldap++; /* Make sure changes make it to the global address book and internet directory, not just the user config. */
				syslog(LOG_DEBUG, "LDAP Detected vcard change.\n");
				vcard_write_user(&CCC->user, v);
			}
		}
	}
#endif

	/*
	 * Extract from the user's vCard, any Internet email addresses and the user's real name.
	 * These are inserted into the session data for various message entry commands to use.
	 */
	v = vcard_get_user(&CCC->user);
	if (v) {
		extract_inet_email_addrs(CCC->cs_inet_email, sizeof CCC->cs_inet_email,
					CCC->cs_inet_other_emails, sizeof CCC->cs_inet_other_emails,
					v, 1
		);
		extract_friendly_name(CCC->cs_inet_fn, sizeof CCC->cs_inet_fn, v);
		vcard_free(v);
	}

	/*
	 * Create the user's 'Contacts' room (personal address book) if it doesn't already exist.
	 */
	vcard_CtdlCreateRoom();
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
	for (i=0; uid[i]; ++i) {
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
	struct CtdlMessage *msg = NULL;
	struct vCard *v;
	char *value = NULL;
	int i, j;
	char addr[256], user[256], node[256], name[256];

	collected_addresses = (char *)userdata;

	msg = CtdlFetchMessage(msgnum, 1, 1);
	if (msg == NULL) return;
	v = vcard_load(msg->cm_fields[eMesageText]);
	CM_Free(msg);

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
	char *ser = NULL;
	struct vCard *v = NULL;
	char recipient[256];
	int i;

	/* First remove any addresses we already have in the address book */
	CtdlUserGoto(aptr->roomname, 0, 0, NULL, NULL, NULL, NULL);
	CtdlForEachMessage(MSGS_ALL, 0, NULL, "[Tt][Ee][Xx][Tt]/.*[Vv][Cc][Aa][Rr][Dd]$", NULL,
		strip_addresses_already_have, aptr->collected_addresses);

	if (!IsEmptyStr(aptr->collected_addresses))
	   for (i=0; i<num_tokens(aptr->collected_addresses, ','); ++i) {

		/* Make a vCard out of each address */
		extract_token(recipient, aptr->collected_addresses, i, ',', sizeof recipient);
		striplt(recipient);
		v = vcard_new_from_rfc822_addr(recipient);
		if (v != NULL) {
			const char *s;
			vmsg = malloc(sizeof(struct CtdlMessage));
			memset(vmsg, 0, sizeof(struct CtdlMessage));
			vmsg->cm_magic = CTDLMESSAGE_MAGIC;
			vmsg->cm_anon_type = MES_NORMAL;
			vmsg->cm_format_type = FMT_RFC822;
			CM_SetField(vmsg, eAuthor, HKEY("Citadel"));
			s = vcard_get_prop(v, "UID", 1, 0, 0);
			if (!IsEmptyStr(s)) {
				CM_SetField(vmsg, eExclusiveID, s, strlen(s));
			}
			ser = vcard_serialize(v);
			if (ser != NULL) {
				StrBuf *buf;
				long serlen;
				
				serlen = strlen(ser);
				buf = NewStrBufPlain(NULL, serlen + 1024);

				StrBufAppendBufPlain(buf, HKEY("Content-type: " VCARD_MIME_TYPE "\r\n\r\n"), 0);
				StrBufAppendBufPlain(buf, ser, serlen, 0);
				StrBufAppendBufPlain(buf, HKEY("\r\n"), 0);
				CM_SetAsFieldSB(vmsg, eMesageText, &buf);
				free(ser);
			}
			vcard_free(v);

			syslog(LOG_DEBUG, "Adding contact: %s", recipient);
			CtdlSubmitMsg(vmsg, NULL, aptr->roomname, QP_EADDR);
			CM_Free(vmsg);
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


const char *CitadelServiceDICT_TCP="DICT_TCP";

CTDL_MODULE_INIT(vcard)
{
	struct ctdlroom qr;
	char filename[256];
	FILE *fp;
	int rv = 0;

	if (!threading)
	{
		CtdlRegisterSessionHook(vcard_session_login_hook, EVT_LOGIN, PRIO_LOGIN + 70);
		CtdlRegisterMessageHook(vcard_upload_beforesave, EVT_BEFORESAVE);
		CtdlRegisterMessageHook(vcard_upload_aftersave, EVT_AFTERSAVE);
		CtdlRegisterDeleteHook(vcard_delete_remove);
		CtdlRegisterProtoHook(cmd_regi, "REGI", "Enter registration info");
		CtdlRegisterProtoHook(cmd_greg, "GREG", "Get registration info");
		CtdlRegisterProtoHook(cmd_igab, "IGAB", "Initialize Global Address Book");
		CtdlRegisterProtoHook(cmd_qdir, "QDIR", "Query Directory");
		CtdlRegisterProtoHook(cmd_gvsn, "GVSN", "Get Valid Screen Names");
		CtdlRegisterProtoHook(cmd_gvea, "GVEA", "Get Valid Email Addresses");
		CtdlRegisterProtoHook(cmd_dvca, "DVCA", "Dump VCard Addresses");
		CtdlRegisterUserHook(vcard_newuser, EVT_NEWUSER);
		CtdlRegisterUserHook(vcard_purge, EVT_PURGEUSER);
		CtdlRegisterNetprocHook(vcard_extract_from_network);
		CtdlRegisterSessionHook(store_harvested_addresses, EVT_TIMER, PRIO_CLEANUP + 470);
		CtdlRegisterFixedOutputHook("text/x-vcard", vcard_fixed_output);
		CtdlRegisterFixedOutputHook("text/vcard", vcard_fixed_output);

		/* Create the Global ADdress Book room if necessary */
		CtdlCreateRoom(ADDRESS_BOOK_ROOM, 3, "", 0, 1, 0, VIEW_ADDRESSBOOK);

		/* Set expiration policy to manual; otherwise objects will be lost! */
		if (!CtdlGetRoomLock(&qr, ADDRESS_BOOK_ROOM)) {
			qr.QRep.expire_mode = EXPIRE_MANUAL;
			qr.QRdefaultview = VIEW_ADDRESSBOOK;	/* 2 = address book view */
			CtdlPutRoomLock(&qr);

			/*
			 * Also make sure it has a netconfig file, so the networker runs
			 * on this room even if we don't share it with any other nodes.
			 * This allows the CANCEL messages (i.e. "Purge this vCard") to be
			 * purged.
			 *
			 * FIXME this no longer works
			 */
			assoc_file_name(filename, sizeof filename, &qr, ctdl_netcfg_dir);
			fp = fopen(filename, "a");
			if (fp != NULL) {
				fclose(fp);
				rv = chown(filename, CTDLUID, (-1));
				if (rv == -1) {
					syslog(LOG_ERR, "Failed to adjust ownership of %s: %s", filename, strerror(errno));
				}
				rv = chmod(filename, 0600);
				if (rv == -1) {
					syslog(LOG_ERR, "Failed to adjust ownership of %s: %s", filename, strerror(errno));
				}
			}
			else {
				syslog(LOG_ERR, "Cannot create %s: %s", filename, strerror(errno));
			}
		}

		/* for postfix tcpdict */
		CtdlRegisterServiceHook(CtdlGetConfigInt("c_pftcpdict_port"),	/* Postfix */
					NULL,
					check_get_greeting,
					check_get,
					NULL,
					CitadelServiceDICT_TCP);
	}
	
	/* return our module name for the log */
	return "vcard";
}
