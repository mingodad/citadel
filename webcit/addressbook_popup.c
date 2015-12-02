/*
 * AJAX-powered auto-completion
 *
 * Copyright (c) 1996-2012 by the citadel.org team
 *
 * This program is open source software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "webcit.h"


/*
 * Address book popup results
 */
HashList* GetAddressbookList()
{
	HashList *List = NULL;
	const StrBuf *WhichAddrBook;
	StrBuf *saved_roomname;
	StrBuf *Name;
	StrBuf *Line;
	long BufLen;
	int IsLocalAddrBook;

	WhichAddrBook = sbstr("which_addr_book");
	IsLocalAddrBook = strcasecmp(ChrPtr(WhichAddrBook), "__LOCAL_USERS__") == 1;

	if (IsLocalAddrBook) {
		serv_puts("LIST");
	}
	else {
		/* remember the default addressbook for this room */
		set_room_pref("defaddrbook", NewStrBufDup(WhichAddrBook), 0);
		saved_roomname = NewStrBufDup(WC->CurRoom.name);
		gotoroom(WhichAddrBook);
		serv_puts("DVCA");
	}
	
	Line = NewStrBuf();
	StrBuf_ServGetln(Line);
	if (GetServerStatus(Line, NULL)  == 1) {
		List = NewHash(1, NULL);
		while (BufLen = StrBuf_ServGetln(Line), 
		       ((BufLen >= 0) && 
			((BufLen != 3) || strcmp(ChrPtr(Line), "000"))))
		{
			if (IsLocalAddrBook &&
			    (BufLen > 5) &&
			    (strncmp(ChrPtr(Line), "SYS_", 4) == 0))
			{
				continue;
			}
			Name = NewStrBufPlain(NULL, StrLength(Line));
			StrBufExtract_token(Name, Line, 0, '|');
			Put(List, SKEY(Name), Name, HFreeStrBuf);

		}
		SortByHashKey(List, 1);
	}

	if (!IsLocalAddrBook) {
		gotoroom(saved_roomname);
		FreeStrBuf(&saved_roomname);
	}

	return List;
}




void 
InitModule_ADDRBOOK_POPUP
(void)
{

	RegisterIterator("ITERATE:ABNAMES", 0, NULL, GetAddressbookList, NULL, NULL, CTX_STRBUF, CTX_NONE, IT_NOFLAG);
}
