#include "includes.hpp"


enum {
	BUTTON_CLOSE,
	BUTTON_READNEW
};


BEGIN_EVENT_TABLE(RoomView, wxMDIChildFrame)
	EVT_BUTTON(	BUTTON_CLOSE,		RoomView::OnButtonPressed)
	EVT_BUTTON(	BUTTON_READNEW,		RoomView::OnButtonPressed)
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


	if (citsock->GotoRoom(roomname, "", recvcmd) != 2) {
		delete this;
	}


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


}



void RoomView::OnButtonPressed(wxCommandEvent& whichbutton) {
	if (whichbutton.GetId() == BUTTON_CLOSE) {
		delete this;
	}

	if (whichbutton.GetId() == BUTTON_READNEW) {
		do_readloop("MSGS NEW");
	}
}


void RoomView::do_readloop(wxString readcmd) {
	wxString sendcmd, recvcmd, buf;
	wxStringList xferbuf;
	
	if (message_window != NULL) {
		delete message_window;
		message_window = NULL;
	}
	
	message_window = new wxTextCtrl(
		this,
		-1,
		"Loading messages...\n\n",
		wxDefaultPosition, wxDefaultSize,
		wxTE_MULTILINE | wxTE_READONLY
		);

	wxLayoutConstraints *m1 = new wxLayoutConstraints;
	m1->top.Below(banner, 2);
	m1->bottom.Above(close_button, -2);
	m1->left.SameAs(this, wxLeft, 2);
	m1->right.SameAs(this, wxRight, 2);
	message_window->SetConstraints(m1);

	sendcmd = readcmd;
	if (citsock->serv_trans(sendcmd, recvcmd, xferbuf) != 1) {
		message_window->SetValue(recvcmd.Mid(4,32767));
		return;
	}

	ListToMultiline(buf, xferbuf);
	message_window->SetValue(buf);
}
