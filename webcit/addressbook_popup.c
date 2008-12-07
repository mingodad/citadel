/*
 * $Id$
 *
 * AJAX-powered auto-completion
 */

#include "webcit.h"


/*
 * Call this right before wDumpContent() on any page which requires the address book popup
 */
void address_book_popup(void) {
	/* Open a new div, hidden initially, for address book popups. */
	wprintf("</div>\n");	/* End of 'content' div */
	wprintf("<div id=\"address_book_popup\" style=\"display:none;\">");
	wprintf("<div id=\"address_book_popup_container_div\">");
	wprintf("<div id=\"address_book_popup_middle_div\"></div>");
	wprintf("<div id=\"address_book_inner_div\"></div>");
	wprintf("</div>");
	/* The 'address_book_popup' div will be closed by wDumpContent() */
}

/*
 * Address book popup window
 */
void display_address_book_middle_div(void) {
	char buf[256];
	long len;
	char *Name;
	const char *VCName;
	void *Namee;
	StrBuf *DefAddrBook;
	HashList *List;
	HashPos  *it;

	begin_ajax_response();

	DefAddrBook = get_room_pref("defaddrbook");

	wprintf("<table border=0 width=100%%><tr valign=middle>");
	wprintf("<td align=left><img src=\"static/viewcontacts_32x.gif\"></td>");
	wprintf("<td align=center>");

	wprintf("<form>"
		"<select class=\"address_book_popup_title\" size=1 id=\"which_addr_book\" "
		" onChange=\"PopulateAddressBookInnerDiv($('which_addr_book').value,'%s')\">",
		bstr("target_input")
	);

	wprintf("<option value=\"__LOCAL_USERS__\" %s>", 
		(strcmp(ChrPtr(DefAddrBook), "__LOCAL_USERS__") == 0)?
		"selected=\"selected\" ":"");
	escputs(serv_info.serv_humannode);
	wprintf("</option>\n");

	
	List = NewHash(1, NULL);
	serv_puts("LKRA");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') while(len = serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		if (extract_int(buf, 6) == VIEW_ADDRESSBOOK) {
			Name = (char*) malloc(len + 1);
			len = extract_token(Name, buf, 0, '|', len);
			Put(List, Name, len, Name, NULL);
		}
	}

	SortByHashKey(List, 1);
	it = GetNewHashPos(List, 0);
	while (GetNextHashPos(List, it, &len, &VCName, &Namee)) {
		wprintf("<option value=\"");
		urlescputs((char*)Namee);
		if (strcmp(ChrPtr(DefAddrBook), Namee) == 0)
			wprintf("\" selected=\"selected\" >");
		else
			wprintf("\">");
		escputs((char*)Namee);
		wprintf("</option>\n");
	}
	DeleteHashPos(&it);
	DeleteHash(&List);
	wprintf("</select></form>");

	wprintf("</td>");
	wprintf("<td align=right "
		"onclick=\"javascript:$('address_book_popup').style.display='none';\" "
		"><img src=\"static/closewindow.gif\">");
	wprintf("</td></tr></table>");

	StrBufAppendPrintf(WC->trailing_javascript,
		"PopulateAddressBookInnerDiv($('which_addr_book').value,'%s');",
		bstr("target_input")
	);

	end_ajax_response();
}



/*
 * Address book popup results
 */
void display_address_book_inner_div() {
	char buf[256];
	int num_targets = 0;
	char target_id[64];
	char target_label[64];
	long len;
	char *Name;
	const char *VCName;
	void *Namee;
	HashList *List;
	HashPos  *it;
	int i;
	char saved_roomname[128];

	begin_ajax_response();

	List = NewHash(1, NULL);
	wprintf("<div align=center><form onSubmit=\"return false;\">"
		"<select multiple name=\"whichaddr\" id=\"whichaddr\" size=\"15\">\n");

	if (!strcasecmp(bstr("which_addr_book"), "__LOCAL_USERS__")) {
		serv_puts("LIST");
		serv_getln(buf, sizeof buf);
		if (buf[0] == '1') while(len = serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
			Name = (char*) malloc(len + 1);
			len = extract_token(Name, buf, 0, '|', len + 1);
			if((len > 5) && (strncmp(Name, "SYS_", 4) == 0)) {
				free(Name);
				continue;
			}
			Put(List, Name, len, Name, NULL);

		}
		SortByHashKey(List, 1);
		it = GetNewHashPos(List, 0);
		while (GetNextHashPos(List, it, &len, &VCName, &Namee)) {
			wprintf("<option value=\"");
			escputs((char*)Namee);
			wprintf("\">");
			escputs((char*)Namee);
			wprintf("</option>\n");
		}
		DeleteHashPos(&it);
		DeleteHash(&List);
	}

	else {
		set_room_pref("defaddrbook",NewStrBufDup(sbstr("which_addr_book")), 0);
		safestrncpy(saved_roomname, WC->wc_roomname, sizeof saved_roomname);
		gotoroom(bstr("which_addr_book"));
		serv_puts("DVCA");
		serv_getln(buf, sizeof buf);
		if (buf[0] == '1') while(len = serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
			Name = (char*) malloc(len + 1);
			len = extract_token(Name, buf, 0, '|', len + 1);
			Put(List, Name, len, Name, NULL);

		}
		SortByHashKey(List, 1);
		it = GetNewHashPos(List, 0);
		while (GetNextHashPos(List, it, &len, &VCName, (void**)&Namee)) {
			wprintf("<option value=\"");
			escputs((char*)Namee);
			wprintf("\">");
			escputs((char*)Namee);
			wprintf("</option>\n");
		}
		DeleteHashPos(&it);
		DeleteHash(&List);
		gotoroom(saved_roomname);
	}

	wprintf("</select>\n");

	wprintf("%s: ", _("Add"));

	num_targets = num_tokens(bstr("target_input"), '|');
	for (i=0; i<num_targets; i+=2) {
		extract_token(target_id, bstr("target_input"), i, '|', sizeof target_id);
		extract_token(target_label, bstr("target_input"), i+1, '|', sizeof target_label);
		wprintf("<INPUT TYPE=\"submit\" NAME=\"select_button\" VALUE=\"%s\" ", target_label);
		wprintf("onClick=\"AddContactsToTarget($('%s'),$('whichaddr'));\">", target_id);
	}

	/* This 'close window' button works.  Omitting it because we already have a close button
	 * in the upper right corner, and this one takes up space.
	 *
	wprintf("<INPUT TYPE=\"submit\" NAME=\"close_button\" VALUE=\"%s\" ", _("Close window"));
	wprintf("onclick=\"javascript:$('address_book_popup').style.display='none';\">");
	 */

	wprintf("</form></div>\n");

	end_ajax_response();
}




void 
InitModule_ADDRBOOK_POPUP
(void)
{
	WebcitAddUrlHandler(HKEY("display_address_book_middle_div"), display_address_book_middle_div, 0);
	WebcitAddUrlHandler(HKEY("display_address_book_inner_div"), display_address_book_inner_div, 0);
}
