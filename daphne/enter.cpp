#include "includes.hpp"

enum {
	BUTTON_SAVE,
	BUTTON_CANCEL
};


BEGIN_EVENT_TABLE(EnterMessage, wxMDIChildFrame)
	EVT_BUTTON(BUTTON_CANCEL,	EnterMessage::OnCancel)
	EVT_BUTTON(BUTTON_SAVE,		EnterMessage::OnSave)
END_EVENT_TABLE()


// frame constructor
EnterMessage::EnterMessage(
	CitClient *sock, wxMDIParentFrame *MyMDI, wxString roomname)
	: wxMDIChildFrame(MyMDI,	//parent
			-1,	//window id
			roomname + ": enter message",
			wxDefaultPosition,
			wxDefaultSize,
			wxDEFAULT_FRAME_STYLE,
			roomname
			) {

	wxString sendcmd, recvcmd;

	citsock = sock;
	citMyMDI = MyMDI;
	ThisRoom = roomname;

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
	c6->top.SameAs(this, wxTop, 12);
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
	c7->bottom.SameAs(fromlabel, wxBottom);
	c7->left.RightOf(fromlabel, 3);
	c7->width.AsIs();
	c7->height.AsIs();
	fromname->SetConstraints(c7);

	TheMessage = new wxTextCtrl(this, -1, "",
		wxDefaultPosition, wxDefaultSize,
		wxTE_MULTILINE);

	wxLayoutConstraints *c9 = new wxLayoutConstraints;
	c9->top.SameAs(fromlabel, wxBottom, 2);
	c9->bottom.Above(cancel_button, -5);
	c9->left.SameAs(this, wxLeft, 2);
	c9->right.SameAs(this, wxRight, 2);
	TheMessage->SetConstraints(c9);

	SetAutoLayout(TRUE);
	Show(TRUE);

}



void EnterMessage::OnCancel(wxCommandEvent& whichbutton) {
	delete this;
}


void EnterMessage::OnSave(wxCommandEvent& whichbutton) {
	wxString sendcmd, recvcmd;

	sendcmd = "ENT0 1";
	if (citsock->serv_trans(sendcmd, recvcmd,
				TheMessage->GetValue(),
				ThisRoom) == 4) {
		delete this;
	}
}
