#include "includes.hpp"

enum {
	BUTTON_SAVE,
	BUTTON_CANCEL,
	BUTTON_FIND
};


BEGIN_EVENT_TABLE(EnterMessage, wxMDIChildFrame)
	EVT_BUTTON(BUTTON_CANCEL,	EnterMessage::OnCancel)
	EVT_BUTTON(BUTTON_SAVE,		EnterMessage::OnSave)
	EVT_BUTTON(BUTTON_FIND,		EnterMessage::OnFind)
END_EVENT_TABLE()


// frame constructor
EnterMessage::EnterMessage(
	CitClient *sock, wxMDIParentFrame *MyMDI, wxString roomname)
	: wxMDIChildFrame(MyMDI,	//parent
			-1,	//window id
			roomname + ": enter message",
			wxDefaultPosition,
			wxDefaultSize,
			wxDEFAULT_FRAME_STYLE | wxTAB_TRAVERSAL,
			roomname
			) {

	wxString sendcmd, recvcmd;

	citsock = sock;
	citMyMDI = MyMDI;
	ThisRoom = roomname;
	finduser_panel = (SelectUser *) NULL;

        wxButton *cancel_button = new wxButton(
                this,
                BUTTON_CANCEL,
                " Cancel ",
                wxDefaultPosition);

        wxLayoutConstraints *c1 = new wxLayoutConstraints;
        c1->bottom.SameAs(this, wxBottom, 2);
        c1->height.AsIs();
        c1->width.AsIs();
        c1->right.SameAs(this, wxRight, 2);
        cancel_button->SetConstraints(c1);

        wxButton *save_button = new wxButton(
                this,
                BUTTON_SAVE,
                " Save ",
                wxDefaultPosition);

	wxLayoutConstraints *c2 = new wxLayoutConstraints;
	c2->bottom.SameAs(cancel_button, wxBottom);
	c2->right.LeftOf(cancel_button, 5);
	c2->height.SameAs(cancel_button, wxHeight);
	c2->width.AsIs();
	save_button->SetConstraints(c2);



	wxStaticText *fromlabel = new wxStaticText(this, -1, "From: ");

	wxLayoutConstraints *c6 = new wxLayoutConstraints;
	c6->top.SameAs(this, wxTop, 10);
	c6->left.SameAs(this, wxLeft, 2);
	c6->width.AsIs();
	c6->height.AsIs();
	fromlabel->SetConstraints(c6);

	wxString posting_name_choices[] = {
		citsock->curr_user
		};
	fromname = new wxChoice(this, -1,
		wxDefaultPosition, wxSize(150,25), 1, posting_name_choices);

	wxLayoutConstraints *c7 = new wxLayoutConstraints;
	c7->centreY.SameAs(fromlabel, wxCentreY);
	c7->left.RightOf(fromlabel, 3);
	c7->width.AsIs();
	c7->height.AsIs();
	fromname->SetConstraints(c7);

	wxStaticText *tolabel = new wxStaticText(this, -1, "To: ");

	wxLayoutConstraints *c8 = new wxLayoutConstraints;
	c8->centreY.SameAs(fromname, wxCentreY);
	c8->left.RightOf(fromname, 5);
	c8->width.AsIs();
	c8->height.AsIs();
	tolabel->SetConstraints(c8);

	toname = new wxTextCtrl(this, -1, "",
		wxDefaultPosition, wxSize(150,25));
	
	wxLayoutConstraints *c9 = new wxLayoutConstraints;
	c9->centreY.SameAs(tolabel, wxCentreY);
	c9->left.RightOf(tolabel, 3);
	c9->width.AsIs();
	c9->height.AsIs();
	toname->SetConstraints(c9);

        wxButton *findrecp = new wxButton(
                this,
                BUTTON_FIND,
                " Find "
		);

	wxLayoutConstraints *d1 = new wxLayoutConstraints;
	d1->centreY.SameAs(toname, wxCentreY);
	d1->left.RightOf(toname, 3);
	d1->width.AsIs();
	d1->height.AsIs();
	findrecp->SetConstraints(d1);



	TheMessage = new wxTextCtrl(this, -1, "",
		wxDefaultPosition, wxDefaultSize,
		wxTE_MULTILINE);

	wxLayoutConstraints *d9 = new wxLayoutConstraints;
	d9->top.Below(fromname, 2);
	d9->bottom.Above(cancel_button, -5);
	d9->left.SameAs(this, wxLeft, 2);
	d9->right.SameAs(this, wxRight, 2);
	TheMessage->SetConstraints(d9);

	SetAutoLayout(TRUE);
	Show(TRUE);
        Layout();
}



void EnterMessage::OnCancel(wxCommandEvent& whichbutton) {
	delete this;
}


// The user clicked "Find" ... so we have to go looking for a recipient.
// Shove a FindUser panel right in front of everything else.
//
void EnterMessage::OnFind(wxCommandEvent& whichbutton) {
	finduser_panel = new SelectUser(citsock, this,
				"Please select a recipient",
				"Recipients",
				0,
				toname);

	wxLayoutConstraints *f1 = new wxLayoutConstraints;
	f1->centreX.SameAs(this, wxCentreX);
	f1->centreY.SameAs(this, wxCentreY);
	f1->width.SameAs(this, wxWidth);
	f1->height.SameAs(this, wxHeight);
	finduser_panel->SetConstraints(f1);
	Layout();
}


void EnterMessage::OnSave(wxCommandEvent& whichbutton) {
	wxString sendcmd, recvcmd, xferbuf;

	sendcmd = "ENT0 1|" + toname->GetValue();
	xferbuf = TheMessage->GetValue();
	if (citsock->serv_trans(sendcmd, recvcmd,
				xferbuf, ThisRoom) == 4) {
		delete this;
	} else {
		// Display the error message
                wxMessageDialog save_error(this,
                        recvcmd.Mid(4),
                        "Error",
                        wxOK | wxCENTRE | wxICON_INFORMATION,
                        wxDefaultPosition);
                        save_error.ShowModal();

	}
}
