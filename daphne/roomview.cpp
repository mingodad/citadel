#include "includes.hpp"


enum {
	BUTTON_CLOSE,
	BUTTON_READNEW,
	BUTTON_READALL
};


BEGIN_EVENT_TABLE(RoomView, wxMDIChildFrame)
	EVT_BUTTON(	BUTTON_CLOSE,		RoomView::OnButtonPressed)
	EVT_BUTTON(	BUTTON_READNEW,		RoomView::OnButtonPressed)
	EVT_BUTTON(	BUTTON_READALL,		RoomView::OnButtonPressed)
END_EVENT_TABLE()


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
	message_window = NULL;


        if (citsock->GotoRoom(roomname, "", recvcmd) != TRUE) {
		delete this;
	}

	extract(ThisRoom, recvcmd.Mid(4), 0);	// actual name of room

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

	wxStaticText *rname = new wxStaticText(banner, -1, roomname);
	rname->SetFont(wxFont(18, wxDEFAULT, wxNORMAL, wxNORMAL));
	rname->SetForegroundColour(wxColour(0xFF, 0xFF, 0x00));
	wxLayoutConstraints *b2 = new wxLayoutConstraints;
	b2->top.SameAs(banner, wxTop, 1);
	b2->left.SameAs(banner, wxLeft, 1);
	b2->width.AsIs();
	b2->height.AsIs();
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

	wxButton *readnew_button = new wxButton(
		this,
		BUTTON_READNEW,
		" Read new messages ",
		wxDefaultPosition);

	wxLayoutConstraints *c2 = new wxLayoutConstraints;
	c2->top.SameAs(close_button, wxTop);
	c2->bottom.SameAs(close_button, wxBottom);
	c2->width.AsIs();
	c2->right.LeftOf(close_button, 5);
	readnew_button->SetConstraints(c2);

	wxButton *readall_button = new wxButton(
		this,
		BUTTON_READALL,
		" Read all messages ",
		wxDefaultPosition);

	wxLayoutConstraints *c3 = new wxLayoutConstraints;
	c3->top.SameAs(close_button, wxTop);
	c3->bottom.SameAs(readnew_button, wxBottom);
	c3->width.AsIs();
	c3->right.LeftOf(readnew_button, 5);
	readall_button->SetConstraints(c3);
}



void RoomView::OnButtonPressed(wxCommandEvent& whichbutton) {
	if (whichbutton.GetId() == BUTTON_CLOSE) {
		delete this;
	} else if (whichbutton.GetId() == BUTTON_READNEW) {
		do_readloop("MSGS NEW");
	} else if (whichbutton.GetId() == BUTTON_READALL) {
		do_readloop("MSGS ALL");
	}
}


void RoomView::do_readloop(wxString readcmd) {
	wxString sendcmd, recvcmd, buf, allmsgs;
	wxStringList xferbuf, msgbuf;
        int i, r;
	
	if (message_window != NULL) {
		delete message_window;
		message_window = NULL;
	}

	message_window = new wxHtmlWindow(this);

	wxLayoutConstraints *m1 = new wxLayoutConstraints;
	m1->top.Below(banner, 2);
	m1->bottom.Above(close_button, -2);
	m1->left.SameAs(this, wxLeft, 2);
	m1->right.SameAs(this, wxRight, 2);
	message_window->SetConstraints(m1);


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
	message_window->SetPage("<html><body>Loading...</body></html>");
	allmsgs = "<HTML><BODY><CENTER><H1>List of Messages</H1></CENTER><HR>";
        for (i=0; i<xferbuf.Number(); ++i) {
                buf.Printf("%s", (wxString *)xferbuf.Nth(i)->GetData());
		sendcmd = "MSG0 " + buf;
		r = citsock->serv_trans(sendcmd, recvcmd, msgbuf, ThisRoom);
		if (r == 1) {
			ListToMultiline(buf, msgbuf);
			allmsgs += buf;
			allmsgs += "<HR>";
		}
		allmsgs += "</HTML>";
		message_window->SetPage(allmsgs);
        }
}


