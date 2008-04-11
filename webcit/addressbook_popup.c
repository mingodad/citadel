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
	void *Namee;
	HashList *List;
	HashPos  *it;

	begin_ajax_response();

	wprintf("<table border=0 width=100%%><tr valign=middle>");
	wprintf("<td align=left><img src=\"static/viewcontacts_32x.gif\"></td>");
	wprintf("<td align=center>");

	wprintf("<form>"
		"<select class=\"address_book_popup_title\" size=1 id=\"which_addr_book\" "
		" onChange=\"PopulateAddressBookInnerDiv($('which_addr_book').value,'%s')\">",
		bstr("target_input")
	);

	wprintf("<option value=\"__LOCAL_USERS__\">");
	escputs(serv_info.serv_humannode);
	wprintf("</option>\n");

	
	List = NewHash();
	serv_puts("LKRA");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') while(len = serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		if (extract_int(buf, 6) == VIEW_ADDRESSBOOK) {
			Name = (char*) malloc(len + 1);
			len = extract_token(Name, buf, 0, '|', len);
			Put(List, Name, len, Name, NULL);
		}
	}

	SortByHashKey(List);
	it = GetNewHashPos();
	while (GetNextHashPos(List, it, &len, &Name, &Namee)) {
		wprintf("<option value=\"");
		urlescputs((char*)Namee);
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

	wprintf("<script type=\"text/javascript\">"
		"PopulateAddressBookInnerDiv($('which_addr_book').value,'%s');"
		"</script>\n",
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
	void *Namee;
	HashList *List;
	HashPos  *it;
	int i;
	char saved_roomname[128];

	begin_ajax_response();

	List = NewHash();
	wprintf("<div align=center><form onSubmit=\"return false;\">"
		"<select multiple name=\"whichaddr\" id=\"whichaddr\" size=\"15\">\n");

	if (!strcasecmp(bstr("which_addr_book"), "__LOCAL_USERS__")) {
		serv_puts("LIST");
		serv_getln(buf, sizeof buf);
		if (buf[0] == '1') while(len = serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
			Name = (char*) malloc(len + 1);
			len = extract_token(Name, buf, 0, '|', len + 1);
			Put(List, Name, len, Name, NULL);

		}
		SortByHashKey(List);
		it = GetNewHashPos();
		while (GetNextHashPos(List, it, &len, &Name, &Namee)) {
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
		safestrncpy(saved_roomname, WC->wc_roomname, sizeof saved_roomname);
		gotoroom(bstr("which_addr_book"));
		serv_puts("DVCA");
		serv_getln(buf, sizeof buf);
		if (buf[0] == '1') while(len = serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
			Name = (char*) malloc(len + 1);
			len = extract_token(Name, buf, 0, '|', len + 1);
			Put(List, Name, len, Name, NULL);

		}
		SortByHashKey(List);
		it = GetNewHashPos();
		while (GetNextHashPos(List, it, &len, &Name, (void**)&Namee)) {
			wprintf("<option value=\"");
			escputs((char*)Namee);
			wprintf("\">");
			escputs((char*)Namee);
			wprintf("</option>\n");
		}
		DeleteHashPos(&it);
		DeleteHash(&List);
		gotoroom((char*)BSTR(saved_roomname)); /* TODO: get rid of typecast */
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
