#include "includes.hpp"

// frame constructor
RoomView::RoomView(
	CitClient *sock, wxMDIParentFrame *MyMDI, wxString roomname)
	: wxMDIChildFrame(MyMDI,	//parent
			-1,	//window id
			roomname,
			wxDefaultPosition,
			wxDefaultSize,
			wxDEFAULT_FRAME_STYLE,
			roomname
			) {

	wxString sendcmd, recvcmd;

	citsock = sock;

	if (citsock->GotoRoom(roomname, "", recvcmd) != 2) {
		delete this;
	}


	SetAutoLayout(TRUE);
	Show(TRUE);

}
