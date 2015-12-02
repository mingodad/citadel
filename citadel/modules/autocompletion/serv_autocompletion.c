/*
 * Autocompletion of email recipients, etc.
 *
 * Copyright (c) 1987-2015 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#include "ctdl_module.h"
#include "serv_autocompletion.h"
#include "config.h"


/*
 * Convert a structured name into a friendly name.  Caller must free the
 * returned pointer.
 */
char *n_to_fn(char *value) {
	char *nnn = NULL;
	int i;

	nnn = malloc(strlen(value) + 10);
	strcpy(nnn, "");
	extract_token(&nnn[strlen(nnn)] , value, 3, ';', 999);
	strcat(nnn, " ");
	extract_token(&nnn[strlen(nnn)] , value, 1, ';', 999);
	strcat(nnn, " ");
	extract_token(&nnn[strlen(nnn)] , value, 2, ';', 999);
	strcat(nnn, " ");
	extract_token(&nnn[strlen(nnn)] , value, 0, ';', 999);
	strcat(nnn, " ");
	extract_token(&nnn[strlen(nnn)] , value, 4, ';', 999);
	strcat(nnn, " ");
	for (i=0; i<strlen(nnn); ++i) {
		if (!strncmp(&nnn[i], "  ", 2)) strcpy(&nnn[i], &nnn[i+1]);
	}
	striplt(nnn);
	return(nnn);
}




/*
 * Back end for cmd_auto()
 */
void hunt_for_autocomplete(long msgnum, char *search_string) {
	struct CtdlMessage *msg;
	struct vCard *v;
	char *value = NULL;
	char *value2 = NULL;
	int i = 0;
	char *nnn = NULL;

	msg = CtdlFetchMessage(msgnum, 1, 1);
	if (msg == NULL) return;

	v = vcard_load(msg->cm_fields[eMesageText]);
	CM_Free(msg);

	/*
	 * Try to match from a friendly name (the "fn" field).  If there is
	 * a match, return the entry in the form of:
	 *     Display Name <user@domain.org>
	 */
	value = vcard_get_prop(v, "fn", 0, 0, 0);
	if (value != NULL) if (bmstrcasestr(value, search_string)) {
		value2 = vcard_get_prop(v, "email", 1, 0, 0);
		if (value2 == NULL) value2 = "";
		cprintf("%s <%s>\n", value, value2);
		vcard_free(v);
		return;
	}

	/*
	 * Try to match from a structured name (the "n" field).  If there is
	 * a match, return the entry in the form of:
	 *     Display Name <user@domain.org>
	 */
	value = vcard_get_prop(v, "n", 0, 0, 0);
	if (value != NULL) if (bmstrcasestr(value, search_string)) {

		value2 = vcard_get_prop(v, "email", 1, 0, 0);
		if (value2 == NULL) value2 = "";
		nnn = n_to_fn(value);
		cprintf("%s <%s>\n", nnn, value2);
		free(nnn);
		vcard_free(v);
		return;
	}

	/*
	 * Try a partial match on all listed email addresses.
	 */
	i = 0;
	while (value = vcard_get_prop(v, "email", 1, i++, 0), value != NULL) {
		if (bmstrcasestr(value, search_string)) {
			if (vcard_get_prop(v, "fn", 0, 0, 0)) {
				cprintf("%s <%s>\n", vcard_get_prop(v, "fn", 0, 0, 0), value);
			}
			else if (vcard_get_prop(v, "n", 0, 0, 0)) {
				nnn = n_to_fn(vcard_get_prop(v, "n", 0, 0, 0));
				cprintf("%s <%s>\n", nnn, value);
				free(nnn);
			
			}
			else {
				cprintf("%s\n", value);
			}
			vcard_free(v);
			return;
		}
	}

	vcard_free(v);
}



/*
 * Attempt to autocomplete an address based on a partial...
 */
void cmd_auto(char *argbuf) {
	char hold_rm[ROOMNAMELEN];
	char search_string[256];
	long *msglist = NULL;
	int num_msgs = 0;
	long *fts_msgs = NULL;
	int fts_num_msgs = 0;
	struct cdbdata *cdbfr;
	int r = 0;
	int i = 0;
	int j = 0;
	int search_match = 0;
	char *rooms_to_try[] = { USERCONTACTSROOM, ADDRESS_BOOK_ROOM };
		
	if (CtdlAccessCheck(ac_logged_in)) return;
	extract_token(search_string, argbuf, 0, '|', sizeof search_string);
	if (IsEmptyStr(search_string)) {
		cprintf("%d You supplied an empty partial.\n",
			ERROR + ILLEGAL_VALUE);
		return;
	}

	strcpy(hold_rm, CC->room.QRname);       /* save current room */
	cprintf("%d try these:\n", LISTING_FOLLOWS);

	/*
	 * Gather up message pointers in rooms containing vCards
	 */
	for (r=0; r < (sizeof(rooms_to_try) / sizeof(char *)); ++r) {
		if (CtdlGetRoom(&CC->room, rooms_to_try[r]) == 0) {
			cdbfr = cdb_fetch(CDB_MSGLISTS, &CC->room.QRnumber, sizeof(long));
			if (cdbfr != NULL) {
				msglist = realloc(msglist, (num_msgs * sizeof(long)) + cdbfr->len + 1);
				memcpy(&msglist[num_msgs], cdbfr->ptr, cdbfr->len);
				num_msgs += (cdbfr->len / sizeof(long));
				cdb_free(cdbfr);
			}
		}
	}

	/*
	 * Search-reduce the results if we have the full text index available
	 */
	if (CtdlGetConfigInt("c_enable_fulltext")) {
		CtdlModuleDoSearch(&fts_num_msgs, &fts_msgs, search_string, "fulltext");
		if (fts_msgs) {
			for (i=0; i<num_msgs; ++i) {
				search_match = 0;
				for (j=0; j<fts_num_msgs; ++j) {
					if (msglist[i] == fts_msgs[j]) {
						search_match = 1;
						j = fts_num_msgs + 1;	/* end the search */
					}
				}
				if (!search_match) {
					msglist[i] = 0;		/* invalidate this result */
				}
			}
			free(fts_msgs);
		}
		else {
			/* If no results, invalidate the whole list */
			free(msglist);
			msglist = NULL;
			num_msgs = 0;
		}
	}

	/*
	 * Now output the ones that look interesting
	 */
	if (num_msgs > 0) for (i=0; i<num_msgs; ++i) {
		if (msglist[i] != 0) {
			hunt_for_autocomplete(msglist[i], search_string);
		}
	}
	
	cprintf("000\n");
	if (strcmp(CC->room.QRname, hold_rm)) {
		CtdlGetRoom(&CC->room, hold_rm);    /* return to saved room */
	}

	if (msglist) {
		free(msglist);
	}
	
}


CTDL_MODULE_INIT(autocompletion)
{
	if (!threading)
	{
		CtdlRegisterProtoHook(cmd_auto, "AUTO", "Do recipient autocompletion");
	}
	/* return our module name for the log */
	return "autocompletion";
}
