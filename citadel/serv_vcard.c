/* */
#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <pwd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <string.h>
#include <limits.h>
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif
#include "citadel.h"
#include "server.h"
#include <syslog.h>
#include <time.h>
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
#include "tools.h"
#include "vcard.h"

struct vcard_internal_info {
	long msgnum;
};

/* Message number symbol used internally by these functions */
unsigned long SYM_VCARD;
#define VC ((struct vcard_internal_info *)CtdlGetUserData(SYM_VCARD))


/*
 * This handler detects whether the user is attempting to save a new
 * vCard as part of his/her personal configuration, and handles the replace
 * function accordingly.
 */
int vcard_personal_upload(struct CtdlMessage *msg) {
	char *ptr;
	int linelen;

	/* If this isn't the configuration room, or if this isn't a MIME
	 * message, don't bother.
	 */
	if (strcasecmp(msg->cm_fields['O'], CONFIGROOM)) return(0);
	if (msg->cm_format_type != 4) return(0);

	ptr = msg->cm_fields['M'];
	while (ptr != NULL) {
	
		linelen = strcspn(ptr, "\n");
		if (linelen == 0) return(0);	/* end of headers */	
		
		if (!strncasecmp(ptr, "Content-type: text/x-vcard", 26)) {
			/* Bingo!  The user is uploading a new vCard, so
			 * delete the old one.
			 */
			CtdlDeleteMessages(msg->cm_fields['O'],
					0L, "text/x-vcard");
			return(0);
		}

		ptr = strchr((char *)ptr, '\n');
		if (ptr != NULL) ++ptr;
	}

	return(0);
}



/*
 * back end function used by vcard_get_user()
 */
void vcard_gm_backend(long msgnum) {
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
        MailboxName(config_rm, u, CONFIGROOM);

        if (getroom(&CC->quickroom, config_rm) != 0) {
                getroom(&CC->quickroom, hold_rm);
                return vcard_new();
        }

        /* We want the last (and probably only) vcard in this room */
	VC->msgnum = (-1);
        CtdlForEachMessage(MSGS_LAST, 1, "text/x-vcard", vcard_gm_backend);
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
	fprintf(fp, "Content-type: text/x-vcard\r\n\r\n");
	if (ser == NULL) {
		fprintf(fp, "begin:vcard\r\nend:vcard\r\n");
	} else {
		fwrite(ser, strlen(ser), 1, fp);
		phree(ser);
	}
        fclose(fp);

        /* This handy API function does all the work for us */
        CtdlWriteObject(CONFIGROOM, "text/x-vcard", temp, u, 0, 1);

        unlink(temp);
}



/*
 * old style "enter registration info" command
 */
void cmd_regi(char *argbuf) {
	int a,b,c;
	char buf[256];
	struct vCard *my_vcard;

	char tmpaddr[256];
	char tmpcity[256];
	char tmpstate[256];
	char tmpzip[256];
	char tmpphone[256];
	char tmpaddress[512];

	if (!(CC->logged_in)) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
		}

	my_vcard = vcard_get_user(&CC->usersupp);
	strcpy(tmpaddr, "");
	strcpy(tmpcity, "");
	strcpy(tmpstate, "");
	strcpy(tmpzip, "");

	cprintf("%d Send registration...\n", SEND_LISTING);
	a=0;
	while (client_gets(buf), strcmp(buf,"000")) {
		if (a==0) vcard_set_prop(my_vcard, "n", buf);
		if (a==1) strcpy(tmpaddr,buf);
		if (a==2) strcpy(tmpcity,buf);
		if (a==3) strcpy(tmpstate,buf);
		if (a==4) {
			for (c=0; c<strlen(buf); ++c) {
				if ((buf[c]>='0')&&(buf[c]<='9')) {
					b=strlen(tmpzip);
					tmpzip[b]=buf[c];
					tmpzip[b+1]=0;
					}
				}
			}
		if (a==5) {
			strcpy(tmpphone, "");
			for (c=0; c<strlen(buf); ++c) {
				if ((buf[c]>='0')&&(buf[c]<='9')) {
					b=strlen(tmpphone);
					tmpphone[b]=buf[c];
					tmpphone[b+1]=0;
					}
				}
			vcard_set_prop(my_vcard, "tel;home", tmpphone);
			}
		if (a==6) vcard_set_prop(my_vcard, "email;internet", buf);
		++a;
		}
	sprintf(tmpaddress, ";;%s;%s;%s;%s;USA",
		tmpaddr, tmpcity, tmpstate, tmpzip);
	vcard_set_prop(my_vcard, "adr", tmpaddress);
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
 * get registration info for a user
 */
void cmd_greg(char *argbuf)
{
	struct usersupp usbuf;
	struct vCard *v;
	char *tel;
	char who[256];
	char adr[256];
	char buf[256];

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
	cprintf("%s\n", vcard_get_prop(v, "n", 0));	/* name */

	sprintf(adr, "%s", vcard_get_prop(v, "adr", 0));/* address... */

	lprintf(9, "adr is <%s>\n", adr);
	extract_token(buf, adr, 2, ';');
	cprintf("%s\n", buf);				/* street */
	extract_token(buf, adr, 3, ';');
	cprintf("%s\n", buf);				/* city */
	extract_token(buf, adr, 4, ';');
	cprintf("%s\n", buf);				/* state */
	extract_token(buf, adr, 5, ';');
	cprintf("%s\n", buf);				/* zip */

	tel = vcard_get_prop(v, "tel;home", 0);
	if (tel == NULL) tel = vcard_get_prop(v, "tel", 1);
	if (tel != NULL) {
		cprintf("%s\n", tel);
		}
	else {
		cprintf(" \n");
	}

	cprintf("%d\n", usbuf.axlevel);

	cprintf("%s\n", vcard_get_prop(v, "email;internet", 0));
	cprintf("000\n");
	}



void vcard_session_startup_hook(void) {
	CtdlAllocUserData(SYM_VCARD, sizeof(struct vcard_internal_info));
}



char *Dynamic_Module_Init(void)
{
	SYM_VCARD = CtdlGetDynamicSymbol();
	CtdlRegisterSessionHook(vcard_session_startup_hook, EVT_START);
	CtdlRegisterMessageHook(vcard_personal_upload, EVT_BEFORESAVE);
	CtdlRegisterProtoHook(cmd_regi, "REGI", "Enter registration info");
	CtdlRegisterProtoHook(cmd_greg, "GREG", "Get registration info");
	return "$Id$";
}
