#include "includes.hpp"
#include <time.h>


enum {
	BUTTON_GOTO,
	BUTTON_SKIP,
	BUTTON_CLOSE,
	BUTTON_READNEW,
	BUTTON_READALL,
	BUTTON_ENTER
};


BEGIN_EVENT_TABLE(RoomView, wxMDIChildFrame)
	EVT_BUTTON(	BUTTON_GOTO,		RoomView::OnButtonPressed)
	EVT_BUTTON(	BUTTON_SKIP,		RoomView::OnButtonPressed)
	EVT_BUTTON(	BUTTON_CLOSE,		RoomView::OnButtonPressed)
	EVT_BUTTON(	BUTTON_READNEW,		RoomView::OnButtonPressed)
	EVT_BUTTON(	BUTTON_READALL,		RoomView::OnButtonPressed)
	EVT_BUTTON(	BUTTON_ENTER,		RoomView::OnButtonPressed)
END_EVENT_TABLE()


// frame constructor
RoomView::RoomView(
	CitClient *sock, wxMDIParentFrame *MyMDI, wxString roomname)
	: wxMDIChildFrame(MyMDI,	//parent
			-1,	//window id
			roomname,
			wxDefaultPosition,
			wxDefaultSize,
			wxDEFAULT_FRAME_STYLE | wxTAB_TRAVERSAL,
			roomname
			) {

	wxString sendcmd, recvcmd;

	citsock = sock;
	citMyMDI = MyMDI;
	message_window = NULL;

        if (citsock->GotoRoom(roomname, "", recvcmd) != TRUE) {
		delete this;
	}

	extract(ThisRoom, recvcmd.Mid(4), 0);	// actual name of room
	SetTitle(ThisRoom);	// FIX why doesn't this work?

	SetAutoLayout(TRUE);
	Show(TRUE);

	// Set up a top panel for the room banner...
	banner = new wxPanel(this, -1);
	banner->SetBackgroundColour(wxColour(0x00, 0x00, 0x77));
	banner->SetForegroundColour(wxColour(0xFF, 0xFF, 0x00));
	wxLayoutConstraints *b1 = new wxLayoutConstraints;
	b1->top.SameAs(this, wxTop, 2);
	b1->left.SameAs(this, wxLeft, 2);
	b1->right.SameAs(this, wxRight, 2);
	b1->height.PercentOf(this, wxHeight, 25);
	banner->SetConstraints(b1);

	wxStaticText *rname = new wxStaticText(banner, -1, ThisRoom);
	rname->SetFont(wxFont(18, wxDEFAULT, wxNORMAL, wxNORMAL));
	rname->SetForegroundColour(wxColour(0xFF, 0xFF, 0x00));
	wxLayoutConstraints *b2 = new wxLayoutConstraints;
	b2->top.SameAs(banner, wxTop, 1);
	b2->left.SameAs(banner, wxLeft, 1);
	b2->width.PercentOf(banner, wxWidth, 50);
	b2->height.PercentOf(banner, wxHeight, 50);
	rname->SetConstraints(b2);

	close_button = new wxButton(
		this,
		BUTTON_CLOSE,
		" Close ",
		wxDefaultPosition);

	wxLayoutConstraints *c1 = new wxLayoutConstraints;
	c1->bottom.SameAs(this, wxBottom, 2);
	c1->height.AsIs();
	c1->width.AsIs();
	c1->right.SameAs(this, wxRight, 2);
	close_button->SetConstraints(c1);

	wxButton *goto_button = new wxButton(
		this,
		BUTTON_GOTO,
		" Goto ",
		wxDefaultPosition);

	wxLayoutConstraints *g2 = new wxLayoutConstraints;
	g2->top.SameAs(close_button, wxTop);
	g2->bottom.SameAs(close_button, wxBottom);
	g2->width.AsIs();
	g2->right.LeftOf(close_button, 5);
	goto_button->SetConstraints(g2);

	wxButton *skip_button = new wxButton(
		this,
		BUTTON_SKIP,
		" Skip ",
		wxDefaultPosition);

	wxLayoutConstraints *g3 = new wxLayoutConstraints;
	g3->top.SameAs(goto_button, wxTop);
	g3->bottom.SameAs(goto_button, wxBottom);
	g3->width.AsIs();
	g3->right.LeftOf(goto_button, 5);
	skip_button->SetConstraints(g3);

	wxButton *readnew_button = new wxButton(
		this,
		BUTTON_READNEW,
		" Read new ",
		wxDefaultPosition);

	wxLayoutConstraints *c2 = new wxLayoutConstraints;
	c2->top.SameAs(skip_button, wxTop);
	c2->bottom.SameAs(skip_button, wxBottom);
	c2->width.AsIs();
	c2->right.LeftOf(skip_button, 5);
	readnew_button->SetConstraints(c2);

	wxButton *readall_button = new wxButton(
		this,
		BUTTON_READALL,
		" Read all ",
		wxDefaultPosition);

	wxLayoutConstraints *c3 = new wxLayoutConstraints;
	c3->top.SameAs(readnew_button, wxTop);
	c3->bottom.SameAs(readnew_button, wxBottom);
	c3->width.AsIs();
	c3->right.LeftOf(readnew_button, 5);
	readall_button->SetConstraints(c3);

	wxButton *enter_button = new wxButton(
		this,
		BUTTON_ENTER,
		" Enter ",
		wxDefaultPosition);

	wxLayoutConstraints *c4 = new wxLayoutConstraints;
	c4->top.SameAs(readall_button, wxTop);
	c4->bottom.SameAs(readall_button, wxBottom);
	c4->width.AsIs();
	c4->right.LeftOf(readall_button, 5);
	enter_button->SetConstraints(c4);

        Layout();
	wxYield();
	do_readloop("MSGS NEW");	// FIX make this configurable

}



void RoomView::OnButtonPressed(wxCommandEvent& whichbutton) {
	wxString sendcmd, recvcmd, xferbuf;

	if (whichbutton.GetId() == BUTTON_CLOSE) {
		delete this;
	} else if (whichbutton.GetId() == BUTTON_READNEW) {
		do_readloop("MSGS NEW");
	} else if (whichbutton.GetId() == BUTTON_READALL) {
		do_readloop("MSGS ALL");
	} else if (whichbutton.GetId() == BUTTON_ENTER) {
		new EnterMessage(citsock, citMyMDI, ThisRoom);
	} else if (whichbutton.GetId() == BUTTON_SKIP) {
		new RoomView(citsock, citMyMDI, RoomList->GetNextRoom());
		delete this;
	} else if (whichbutton.GetId() == BUTTON_GOTO) {
		sendcmd = "SLRP HIGHEST";	// mark messages as read
		citsock->serv_trans(sendcmd, recvcmd, xferbuf, ThisRoom);
		new RoomView(citsock, citMyMDI, RoomList->GetNextRoom());
		delete this;
	}
}


void RoomView::do_readloop(wxString readcmd) {
	wxString sendcmd, recvcmd, buf, allmsgs;
	wxString xferbuf;
        int i, r, pos;
	CitMessage *message;
	
	if (message_window != NULL) {
		delete message_window;
		message_window = NULL;
	}


	// Transmit the "read messages" command
	sendcmd = readcmd;
	if (citsock->serv_trans(sendcmd, recvcmd, xferbuf, ThisRoom) != 1) {
		wxMessageDialog cantread(this,
			recvcmd.Mid(4),
			"Error",
			wxOK | wxCENTRE | wxICON_INFORMATION,
			wxDefaultPosition);
		cantread.ShowModal();
		return;
	}


	// Read the messages into the window, one at a time
	allmsgs = "<HTML><BODY><CENTER><H1>List of Messages</H1></CENTER><HR>";
	i = 0;
	while (pos = xferbuf.Find('\n', FALSE), (pos >= 0)) {

		buf.Printf("Reading message %d", ++i);
		citMyMDI->SetStatusText(buf, 0);
                wxYield();

		buf = xferbuf.Left(pos);
		xferbuf = xferbuf.Mid(pos+1);

		sendcmd = "MSG0 " + buf;
		message = new CitMessage(citsock, sendcmd, ThisRoom);

		allmsgs += "&nbsp;&nbsp;&nbsp;<em><font size=+1>";
		allmsgs += asctime(localtime(&message->timestamp));
		allmsgs += " from " + message->author;
		if (message->room.CmpNoCase(ThisRoom))
			allmsgs += " in " + message->room + "> ";
		if (message->nodename.CmpNoCase(citsock->NodeName))
			allmsgs += " @ " + message->nodename;
		if (message->recipient.Length() > 0)
			allmsgs += " to " + message->recipient;
		allmsgs += "</font></em><br>";
		allmsgs += message->msgtext;

		delete message;

		allmsgs += "<HR>";
        }
	citMyMDI->SetStatusText("Done", 0);
	allmsgs += "</BODY></HTML>";

	message_window = new wxHtmlWindow(this);

	wxLayoutConstraints *m1 = new wxLayoutConstraints;
	m1->top.Below(banner, 2);
	m1->bottom.Above(close_button, -2);
	m1->left.SameAs(this, wxLeft, 2);
	m1->right.SameAs(this, wxRight, 2);
	message_window->SetConstraints(m1);

	message_window->SetPage(allmsgs);
        Layout();
}


