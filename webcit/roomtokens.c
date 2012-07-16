/*
 * Lots of different room-related operations.
 *
 * Copyright (c) 1996-2012 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "webcit.h"
#include "webserver.h"


/*
 * Embed the room banner
 *
 * got			The information returned from a GOTO server command
 * navbar_style 	Determines which navigation buttons to display
 */
void tmplput_roombanner(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;
	
	/* Refresh current room states.  Doesn't work? gotoroom(NULL); */

	wc_printf("<div id=\"banner\">\n");

	/* The browser needs some information for its own use */
	wc_printf("<script type=\"text/javascript\">	\n"
		  "	room_is_trash = %d;		\n"
		  "</script>\n",
		  ((WC->CurRoom.RAFlags & UA_ISTRASH) != 0)
	);

	/*
	 * If the user happens to select the "make this my start page" link,
	 * we want it to remember the URL as a "/dotskip" one instead of
	 * a "skip" or "gotonext" or something like that.
	 */
	if (WCC->Hdr->this_page == NULL) {
		WCC->Hdr->this_page = NewStrBuf();
	}
	StrBufPrintf(WCC->Hdr->this_page, "dotskip?room=%s", ChrPtr(WC->CurRoom.name));

	do_template("roombanner");

	do_template("navbar");
	wc_printf("</div>\n");
}


/*******************************************************************************
 ********************** FLOOR Tokens *******************************************
 *******************************************************************************/


void tmplput_FLOOR_ID(StrBuf *Target, WCTemplputParams *TP) 
{
	Floor *myFloor = (Floor *)CTX;

	StrBufAppendPrintf(Target, "%d", myFloor->ID);
}


void tmplput_ROOM_FLOORID(StrBuf *Target, WCTemplputParams *TP) 
{
	folder *Folder = (folder *)CTX;
	StrBufAppendPrintf(Target, "%d", Folder->floorid);
}


void tmplput_ROOM_FLOOR_ID(StrBuf *Target, WCTemplputParams *TP) 
{
	folder *Folder = (folder *)CTX;
	const Floor *pFloor = Folder->Floor;

	if (pFloor == NULL)
		return;

	StrBufAppendPrintf(Target, "%d", pFloor->ID);
}


void tmplput_ROOM_FLOOR_NAME(StrBuf *Target, WCTemplputParams *TP) 
{
	folder *Folder = (folder *)CTX;
	const Floor *pFloor = Folder->Floor;

	if (pFloor == NULL)
		return;

	StrBufAppendTemplate(Target, TP, pFloor->Name, 0);
}


void tmplput_ThisRoomFloorName(StrBuf *Target, WCTemplputParams *TP) 
{
	wcsession *WCC = WC;
	folder *Folder = &WCC->CurRoom;
	const Floor *pFloor;

	if (Folder == NULL)
		return;

	pFloor = Folder->Floor;
	if (pFloor == NULL)
		return;

	StrBufAppendTemplate(Target, TP, pFloor->Name, 0);
}


void tmplput_FLOOR_NAME(StrBuf *Target, WCTemplputParams *TP) 
{
	Floor *myFloor = (Floor *)CTX;

	StrBufAppendTemplate(Target, TP, myFloor->Name, 0);
}


void tmplput_FLOOR_NROOMS(StrBuf *Target, WCTemplputParams *TP) 
{
	Floor *myFloor = (Floor *)CTX;

	StrBufAppendPrintf(Target, "%d", myFloor->NRooms);
}


void tmplput_ROOM_FLOOR_NROOMS(StrBuf *Target, WCTemplputParams *TP) 
{
	folder *Folder = (folder *)CTX;
	const Floor *pFloor = Folder->Floor;

	if (pFloor == NULL)
		return;
	StrBufAppendPrintf(Target, "%d", pFloor->NRooms);
}


int ConditionalFloorHaveNRooms(StrBuf *Target, WCTemplputParams *TP)
{
	Floor *MyFloor = (Floor *)CTX;
	int HaveN;

	HaveN = GetTemplateTokenNumber(Target, TP, 0, 0);

	return HaveN == MyFloor->NRooms;
}


int ConditionalFloorIsRESTSubFloor(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession  *WCC = WC;
	Floor *MyFloor = (Floor *)CTX;
	/** if we have dav_depth the client just wants the subfloors */
	if ((WCC->Hdr->HR.dav_depth == 1) && 
	    (GetCount(WCC->Directory) == 0))
		return 1;
	return WCC->CurrentFloor == MyFloor;
}


int ConditionalFloorIsSUBROOM(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession  *WCC = WC;
	Floor *MyFloor = (Floor *)CTX;

	return WCC->CurRoom.floorid == MyFloor->ID;
}


int ConditionalFloorIsVirtual(StrBuf *Target, WCTemplputParams *TP)
{
	Floor *MyFloor = (Floor *)CTX;

	return MyFloor->ID == VIRTUAL_MY_FLOOR;
}


/*******************************************************************************
 ********************** ROOM Tokens ********************************************
 *******************************************************************************/
/**** Name ******/

void tmplput_ThisRoom(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;

	if (WCC != NULL) {
		StrBufAppendTemplate(Target, TP, 
		     WCC->CurRoom.name, 
		     0
		);
	}
}


void tmplput_ROOM_NAME(StrBuf *Target, WCTemplputParams *TP) 
{
	folder *Folder = (folder *)CTX;

	StrBufAppendTemplate(Target, TP, Folder->name, 0);
}


void tmplput_ROOM_BASENAME(StrBuf *Target, WCTemplputParams *TP) 
{
	folder *room = (folder *)CTX;

	if (room->nRoomNameParts > 1)
		StrBufAppendTemplate(Target, TP, 
				      room->RoomNameParts[room->nRoomNameParts - 1], 0);
	else 
		StrBufAppendTemplate(Target, TP, room->name, 0);
}


void tmplput_ROOM_LEVEL_N_TIMES(StrBuf *Target, WCTemplputParams *TP) 
{
	folder *room = (folder *)CTX;
	int i;
        const char *AppendMe;
        long AppendMeLen;


	if (room->nRoomNameParts > 1)
	{
		GetTemplateTokenString(Target, TP, 0, &AppendMe, &AppendMeLen);
		for (i = 0; i < room->nRoomNameParts; i++)
			StrBufAppendBufPlain(Target, AppendMe, AppendMeLen, 0);
	}
}


int ConditionalRoomIsInbox(StrBuf *Target, WCTemplputParams *TP)
{
	folder *Folder = (folder *)CTX;
	return Folder->is_inbox;
}


/****** Properties ******/
int ConditionalRoom_MayEdit(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;

	LoadRoomXA ();

	return WCC->CurRoom.XALoaded == 1;
}

int ConditionalThisRoomHas_QRFlag(StrBuf *Target, WCTemplputParams *TP)
{
	long QR_CheckFlag;
	wcsession *WCC = WC;
	
	QR_CheckFlag = GetTemplateTokenNumber(Target, TP, 2, 0);
	if (QR_CheckFlag == 0)
		LogTemplateError(Target, "Conditional", ERR_PARM1, TP,
				 "requires one of the #\"QR*\"- defines or an integer flag 0 is invalid!");
	
	if (WCC == NULL)
		return 0;

	if ((TP->Tokens->Params[2]->MaskBy == eOR) ||
	    (TP->Tokens->Params[2]->MaskBy == eNO))
		return (WCC->CurRoom.QRFlags & QR_CheckFlag) != 0;
	else
		return (WCC->CurRoom.QRFlags & QR_CheckFlag) == QR_CheckFlag;
}


int ConditionalRoomHas_QRFlag(StrBuf *Target, WCTemplputParams *TP)
{
	long QR_CheckFlag;
	folder *Folder = (folder *)(TP->Context);

	QR_CheckFlag = GetTemplateTokenNumber(Target, TP, 2, 0);
	if (QR_CheckFlag == 0)
		LogTemplateError(Target, "Conditional", ERR_PARM1, TP,
				 "requires one of the #\"QR*\"- defines or an integer flag 0 is invalid!");

	if ((TP->Tokens->Params[2]->MaskBy == eOR) ||
	    (TP->Tokens->Params[2]->MaskBy == eNO))
		return (Folder->QRFlags & QR_CheckFlag) != 0;
	else
		return (Folder->QRFlags & QR_CheckFlag) == QR_CheckFlag;
}


void tmplput_ROOM_QRFLAGS(StrBuf *Target, WCTemplputParams *TP) 
{
	folder *Folder = (folder *)CTX;
	StrBufAppendPrintf(Target, "%d", Folder->QRFlags);
}


int ConditionalThisRoomHas_QRFlag2(StrBuf *Target, WCTemplputParams *TP)
{
	long QR2_CheckFlag;
	wcsession *WCC = WC;
	
	QR2_CheckFlag = GetTemplateTokenNumber(Target, TP, 2, 0);
	if (QR2_CheckFlag == 0)
		LogTemplateError(Target, "Conditional", ERR_PARM1, TP,
				 "requires one of the #\"QR2*\"- defines or an integer flag 0 is invalid!");

	
	if (WCC == NULL)
		return 0;

	if ((TP->Tokens->Params[2]->MaskBy == eOR) ||
	    (TP->Tokens->Params[2]->MaskBy == eNO))
		return (WCC->CurRoom.QRFlags2 & QR2_CheckFlag) != 0;
	else
		return (WCC->CurRoom.QRFlags2 & QR2_CheckFlag) == QR2_CheckFlag;
}


int ConditionalRoomHas_QRFlag2(StrBuf *Target, WCTemplputParams *TP)
{
	long QR2_CheckFlag;
	folder *Folder = (folder *)(TP->Context);

	QR2_CheckFlag = GetTemplateTokenNumber(Target, TP, 2, 0);
	if (QR2_CheckFlag == 0)
		LogTemplateError(Target, "Conditional", ERR_PARM1, TP,
				 "requires one of the #\"QR2*\"- defines or an integer flag 0 is invalid!");
	return ((Folder->QRFlags2 & QR2_CheckFlag) != 0);
}


int ConditionalRoomHas_UAFlag(StrBuf *Target, WCTemplputParams *TP)
{
	folder *Folder = (folder *)(TP->Context);
	long UA_CheckFlag;
		
	UA_CheckFlag = GetTemplateTokenNumber(Target, TP, 2, 0);
	if (UA_CheckFlag == 0)
		LogTemplateError(Target, "Conditional", ERR_PARM1, TP,
				 "requires one of the #\"UA_*\"- defines or an integer flag 0 is invalid!");

	return ((Folder->RAFlags & UA_CheckFlag) != 0);
}


void tmplput_ROOM_ACL(StrBuf *Target, WCTemplputParams *TP) 
{
	folder *Folder = (folder *)CTX;

	StrBufAppendPrintf(Target, "%ld", Folder->RAFlags, 0);
}


void tmplput_ROOM_RAFLAGS(StrBuf *Target, WCTemplputParams *TP) 
{
	folder *Folder = (folder *)(TP->Context);
	StrBufAppendPrintf(Target, "%d", Folder->RAFlags);
}


void tmplput_ThisRoomAide(StrBuf *Target, WCTemplputParams *TP) 
{
	wcsession *WCC = WC;

	LoadRoomAide();

	StrBufAppendTemplate(Target, TP, WCC->CurRoom.RoomAide, 0);
}


int ConditionalRoomAide(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;
	return (WCC != NULL)? 
		((WCC->CurRoom.RAFlags & UA_ADMINALLOWED) != 0) : 0;
}


int ConditionalRoomAcessDelete(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;
	return (WCC == NULL)? 0 : 
		( ((WCC->CurRoom.RAFlags & UA_ADMINALLOWED) != 0) ||
		   (WCC->CurRoom.is_inbox) || 
		   (WCC->CurRoom.QRFlags2 & QR2_COLLABDEL) );
}


int ConditionalHaveRoomeditRights(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;

	return (	(WCC != NULL)
			&& (WCC->logged_in)
			&& (
				(WCC->axlevel >= 6)
				|| ((WCC->CurRoom.RAFlags & UA_ADMINALLOWED) != 0)
				|| (WCC->CurRoom.is_inbox)
			)
		);
}


void tmplput_ThisRoomPass(StrBuf *Target, WCTemplputParams *TP) 
{
	wcsession *WCC = WC;

	LoadRoomXA();
	StrBufAppendTemplate(Target, TP, WCC->CurRoom.XAPass, 0);
}


void tmplput_ThisRoom_nNewMessages(StrBuf *Target, WCTemplputParams *TP) 
{
	wcsession *WCC = WC;

	StrBufAppendPrintf(Target, "%d", WCC->CurRoom.nNewMessages);
}


void tmplput_ThisRoom_nTotalMessages(StrBuf *Target, WCTemplputParams *TP) 
{
	wcsession *WCC = WC;

	StrBufAppendPrintf(Target, "%d", WCC->CurRoom.nTotalMessages);
}


void tmplput_ThisRoomOrder(StrBuf *Target, WCTemplputParams *TP) 
{
	wcsession *WCC = WC;

	LoadRoomXA();

	StrBufAppendPrintf(Target, "%d", WCC->CurRoom.Order);
}


int ConditionalThisRoomOrder(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;
	long CheckThis;

	if (WCC == NULL)
		return 0;

	LoadRoomXA();

	CheckThis = GetTemplateTokenNumber(Target, TP, 2, 0);
	return CheckThis == WCC->CurRoom.Order;
}


void tmplput_ROOM_LISTORDER(StrBuf *Target, WCTemplputParams *TP) 
{
	folder *Folder = (folder *)CTX;
	StrBufAppendPrintf(Target, "%d", Folder->Order);
}


int ConditionalThisRoomXHavePic(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;
	
	if (WCC == NULL)
		return 0;

	LoadXRoomPic();
	return WCC->CurRoom.XHaveRoomPic == 1;
}


int ConditionalThisRoomXHaveInfoText(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;
	
	if (WCC == NULL)
		return 0;

	LoadXRoomInfoText();
	return (StrLength(WCC->CurRoom.XInfoText)>0);
}


void tmplput_ThisRoomInfoText(StrBuf *Target, WCTemplputParams *TP) 
{
	wcsession *WCC = WC;
	long nchars = 0;

	LoadXRoomInfoText();

	nchars = GetTemplateTokenNumber(Target, TP, 0, 0);
	if (!nchars) {
		/* the whole thing */
		StrBufAppendTemplate(Target, TP, WCC->CurRoom.XInfoText, 1);
	}
	else {
		/* only a certain number of characters */
		StrBuf *SubBuf;
		SubBuf = NewStrBufDup(WCC->CurRoom.XInfoText);
		if (StrLength(SubBuf) > nchars) {
			StrBuf_Utf8StrCut(SubBuf, nchars);
			StrBufAppendBufPlain(SubBuf, HKEY("..."), 0);
		}
		StrBufAppendTemplate(Target, TP, SubBuf, 1);
		FreeStrBuf(&SubBuf);
	}
}


void tmplput_ROOM_LASTCHANGE(StrBuf *Target, WCTemplputParams *TP) 
{
	folder *Folder = (folder *)CTX;
	StrBufAppendPrintf(Target, "%d", Folder->lastchange);
}


void tmplput_ThisRoomDirectory(StrBuf *Target, WCTemplputParams *TP) 
{
	wcsession *WCC = WC;

	LoadRoomXA();

	StrBufAppendTemplate(Target, TP, WCC->CurRoom.Directory, 0);
}


void tmplput_ThisRoomXNFiles(StrBuf *Target, WCTemplputParams *TP) 
{
	wcsession *WCC = WC;

	LoadXRoomXCountFiles();

	StrBufAppendPrintf(Target, "%d", WCC->CurRoom.XDownloadCount);
}


void tmplput_ThisRoomX_FileString(StrBuf *Target, WCTemplputParams *TP) 
{
	wcsession *WCC = WC;

	LoadXRoomXCountFiles();

	if (WCC->CurRoom.XDownloadCount == 1)
		StrBufAppendBufPlain(Target, _("file"), -1, 0);
	else
		StrBufAppendBufPlain(Target, _("files"), -1, 0);
}


int ConditionalIsThisThatRoom(StrBuf *Target, WCTemplputParams *TP)
{
	folder *Folder = (folder *)CTX;
	wcsession *WCC = WC;

	if (WCC == NULL)
		return 0;

	return Folder == WCC->ThisRoom;
}


void 
InitModule_ROOMTOKENS
(void)
{
	RegisterNamespace("ROOMBANNER", 0, 1, tmplput_roombanner, NULL, CTX_NONE);

	RegisterNamespace("FLOOR:ID", 0, 0, tmplput_FLOOR_ID, NULL, CTX_FLOORS);
	RegisterNamespace("ROOM:INFO:FLOORID", 0, 1, tmplput_ROOM_FLOORID, NULL, CTX_ROOMS);
	RegisterNamespace("ROOM:INFO:FLOOR:ID", 0, 0, tmplput_ROOM_FLOOR_ID, NULL, CTX_ROOMS);

	RegisterNamespace("FLOOR:NAME", 0, 1, tmplput_FLOOR_NAME, NULL, CTX_FLOORS);
	RegisterNamespace("ROOM:INFO:FLOOR:NAME", 0, 1, tmplput_ROOM_FLOOR_NAME, NULL, CTX_ROOMS);
	RegisterNamespace("THISROOM:FLOOR:NAME", 0, 1, tmplput_ThisRoomFloorName, NULL, CTX_NONE);

	RegisterNamespace("FLOOR:NROOMS", 0, 0, tmplput_FLOOR_NROOMS, NULL, CTX_FLOORS);
	RegisterNamespace("ROOM:INFO:FLOOR:NROOMS", 0, 0, tmplput_ROOM_FLOOR_NROOMS, NULL, CTX_ROOMS);

	RegisterConditional(HKEY("COND:FLOOR:ISSUBROOM"), 0, ConditionalFloorIsSUBROOM, CTX_FLOORS);
	RegisterConditional(HKEY("COND:FLOOR:NROOMS"), 1, ConditionalFloorHaveNRooms, CTX_FLOORS);
	RegisterConditional(HKEY("COND:ROOM:REST:ISSUBFLOOR"), 0, ConditionalFloorIsRESTSubFloor, CTX_FLOORS);
	RegisterConditional(HKEY("COND:FLOOR:ISVIRTUAL"), 0, ConditionalFloorIsVirtual, CTX_FLOORS);

	/**** Room... ******/
        /**** Name ******/
	RegisterNamespace("THISROOM:NAME", 0, 1, tmplput_ThisRoom, NULL, CTX_NONE);

	RegisterNamespace("ROOM:INFO:NAME", 0, 1, tmplput_ROOM_NAME, NULL, CTX_ROOMS);
	RegisterNamespace("ROOM:INFO:BASENAME", 0, 1, tmplput_ROOM_BASENAME, NULL, CTX_ROOMS);
	RegisterNamespace("ROOM:INFO:LEVELNTIMES", 1, 2, tmplput_ROOM_LEVEL_N_TIMES, NULL, CTX_ROOMS);
	RegisterConditional(HKEY("COND:ROOM:INFO:IS_INBOX"), 0, ConditionalRoomIsInbox, CTX_ROOMS);

	/****** Properties ******/
	RegisterNamespace("ROOM:INFO:QRFLAGS", 0, 1, tmplput_ROOM_QRFLAGS, NULL, CTX_ROOMS);
	RegisterConditional(HKEY("COND:THISROOM:FLAG:QR"), 0, ConditionalThisRoomHas_QRFlag, CTX_NONE);
	RegisterConditional(HKEY("COND:THISROOM:EDIT"), 0, ConditionalRoom_MayEdit, CTX_NONE);
	RegisterConditional(HKEY("COND:ROOM:FLAG:QR"), 0, ConditionalRoomHas_QRFlag, CTX_ROOMS);

	RegisterConditional(HKEY("COND:THISROOM:FLAG:QR2"), 0, ConditionalThisRoomHas_QRFlag2, CTX_NONE);
	RegisterConditional(HKEY("COND:ROOM:FLAG:QR2"), 0, ConditionalRoomHas_QRFlag2, CTX_ROOMS);

	RegisterConditional(HKEY("COND:ROOM:FLAG:UA"), 0, ConditionalRoomHas_UAFlag, CTX_ROOMS);
	RegisterNamespace("ROOM:INFO:RAFLAGS", 0, 1, tmplput_ROOM_RAFLAGS, NULL, CTX_ROOMS);


	RegisterNamespace("ROOM:INFO:LISTORDER", 0, 1, tmplput_ROOM_LISTORDER, NULL, CTX_ROOMS);
	RegisterNamespace("THISROOM:ORDER", 0, 0, tmplput_ThisRoomOrder, NULL, CTX_NONE);
	RegisterConditional(HKEY("COND:THISROOM:ORDER"), 0, ConditionalThisRoomOrder, CTX_NONE);

	RegisterNamespace("ROOM:INFO:LASTCHANGE", 0, 1, tmplput_ROOM_LASTCHANGE, NULL, CTX_ROOMS);

	RegisterNamespace("THISROOM:MSGS:NEW", 0, 0, tmplput_ThisRoom_nNewMessages, NULL, CTX_NONE);
	RegisterNamespace("THISROOM:MSGS:TOTAL", 0, 0, tmplput_ThisRoom_nTotalMessages, NULL, CTX_NONE);

	RegisterNamespace("THISROOM:PASS", 0, 1, tmplput_ThisRoomPass, NULL, CTX_NONE);
	RegisterNamespace("THISROOM:AIDE", 0, 1, tmplput_ThisRoomAide, NULL, CTX_NONE);
	RegisterConditional(HKEY("COND:ROOMAIDE"), 2, ConditionalRoomAide, CTX_NONE);
	RegisterConditional(HKEY("COND:ACCESS:DELETE"), 2, ConditionalRoomAcessDelete, CTX_NONE);
	RegisterConditional(HKEY("COND:ROOM:EDITACCESS"), 0, ConditionalHaveRoomeditRights, CTX_NONE);

	RegisterConditional(HKEY("COND:THISROOM:HAVE_PIC"), 0, ConditionalThisRoomXHavePic, CTX_NONE);

	RegisterNamespace("THISROOM:INFOTEXT", 1, 2, tmplput_ThisRoomInfoText, NULL, CTX_NONE);
	RegisterConditional(HKEY("COND:THISROOM:HAVE_INFOTEXT"), 0, ConditionalThisRoomXHaveInfoText, CTX_NONE);

	RegisterNamespace("THISROOM:FILES:N", 0, 1, tmplput_ThisRoomXNFiles, NULL, CTX_NONE);
	RegisterNamespace("THISROOM:FILES:STR", 0, 1, tmplput_ThisRoomX_FileString, NULL, CTX_NONE);
	RegisterNamespace("THISROOM:DIRECTORY", 0, 1, tmplput_ThisRoomDirectory, NULL, CTX_NONE);

	RegisterNamespace("ROOM:INFO:ACL", 0, 1, tmplput_ROOM_ACL, NULL, CTX_ROOMS);
	RegisterConditional(HKEY("COND:THIS:THAT:ROOM"), 0, ConditionalIsThisThatRoom, CTX_ROOMS);
}
