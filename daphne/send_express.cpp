// =========================================================================
// declarations
// =========================================================================

#include "includes.hpp"

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// IDs for the controls and the menu commands
enum
{
	BUTTON_SEND,
	BUTTON_CANCEL
};

// ----------------------------------------------------------------------------
// event tables and other macros for wxWindows
// ----------------------------------------------------------------------------

// the event tables connect the wxWindows events with the functions (event
// handlers) which process them. It can be also done at run-time, but for the
// simple menu events like this the static method is much simpler.
BEGIN_EVENT_TABLE(	SendExpress, wxMDIChildFrame)
	EVT_BUTTON(	BUTTON_SEND,		SendExpress::OnButtonPressed)
	EVT_BUTTON(	BUTTON_CANCEL,		SendExpress::OnButtonPressed)
END_EVENT_TABLE()

// ============================================================================
// implementation
// ============================================================================


// ----------------------------------------------------------------------------
// the application class
// ----------------------------------------------------------------------------

// frame constructor
SendExpress::SendExpress(	CitClient *sock,
				wxMDIParentFrame *MyMDI,
				wxString touser)
       : wxMDIChildFrame(MyMDI,	//parent
			-1,	//window id
			" Page another user ",
			wxDefaultPosition,
			wxDefaultSize,
			wxDEFAULT_FRAME_STYLE,
			"SendExpress"
			) {

	wxString sendcmd, recvcmd, buf;
	wxString xferbuf;
        wxString user;

	citsock = sock;
	citMyMDI = MyMDI;

	// set the frame icon
	/* SetIcon(wxICON(mondrian)); */

	wxButton *send_button = new wxButton(
		this,
		BUTTON_SEND,
		"Send",
		wxPoint(200,200),
		wxSize(100,30),
		0L,
		wxDefaultValidator,
		"send_button"
		);

	wxButton *cancel_button = new wxButton(
		this,
		BUTTON_CANCEL,
		"Cancel",
		wxPoint(300,300),
		wxSize(100,30),
		0L,
		wxDefaultValidator,
		"cancel_button"
		);

	wxStaticBox *ToWhomFrame = new wxStaticBox(
		this,
		-1,
		"Send to:",
		wxDefaultPosition, wxDefaultSize, 0,
		"ToWhomFrame"
		);

	ToWhom = new wxListBox(
		this,
		-1,
		wxDefaultPosition, wxDefaultSize,
		0, NULL,
		wxLB_SORT,
		wxDefaultValidator,
		"ToWhom"
		);

	wxStaticBox *TheMessageFrame = new wxStaticBox(
		this,
		-1,
		"Message:",
		wxDefaultPosition, wxDefaultSize, 0,
		"TheMessageFrame"
		);

	TheMessage = new wxTextCtrl(
		this,
		-1,
		"",
		wxDefaultPosition, wxDefaultSize,
		wxTE_MULTILINE,
		wxDefaultValidator,
		"TheMessage"
		);

	wxLayoutConstraints *c1 = new wxLayoutConstraints;
	c1->bottom.SameAs(this, wxBottom, 10);
	c1->left.SameAs(this, wxLeft, 10);
	c1->height.AsIs(); c1->width.AsIs();
	send_button->SetConstraints(c1);

	wxLayoutConstraints *c3 = new wxLayoutConstraints;
	c3->bottom.SameAs(send_button, wxBottom);
	c3->right.SameAs(this, wxRight, 10);
	c3->height.AsIs(); c3->width.AsIs();
	cancel_button->SetConstraints(c3);

	wxLayoutConstraints *c4 = new wxLayoutConstraints;
	c4->top.SameAs(this, wxTop, 10);
	c4->left.SameAs(this, wxLeft, 10);
	c4->right.SameAs(this, wxRight, 10);
	c4->height.PercentOf(this, wxHeight, 30);
	ToWhomFrame->SetConstraints(c4);

	wxLayoutConstraints *c5 = new wxLayoutConstraints;
	c5->top.SameAs(ToWhomFrame, wxTop, 15);
	c5->left.SameAs(ToWhomFrame, wxLeft, 5);
	c5->right.SameAs(ToWhomFrame, wxRight, 5);
	c5->bottom.SameAs(ToWhomFrame, wxBottom, 5);
	ToWhom->SetConstraints(c5);

	wxLayoutConstraints *c6 = new wxLayoutConstraints;
	c6->top.SameAs(ToWhomFrame, wxBottom, 10);
	c6->left.SameAs(ToWhomFrame, wxLeft);
	c6->right.SameAs(ToWhomFrame, wxRight);
	c6->bottom.SameAs(send_button, wxTop, 10);
	TheMessageFrame->SetConstraints(c6);

	wxLayoutConstraints *c7 = new wxLayoutConstraints;
	c7->top.SameAs(TheMessageFrame, wxTop, 15);
	c7->left.SameAs(TheMessageFrame, wxLeft, 5);
	c7->right.SameAs(TheMessageFrame, wxRight, 5);
	c7->bottom.SameAs(TheMessageFrame, wxBottom, 5);
	TheMessage->SetConstraints(c7);

	SetAutoLayout(TRUE);
	Show(TRUE);
        Layout();

        sendcmd = "RWHO";
        if (citsock->serv_trans(sendcmd, recvcmd, xferbuf) != 1) return;

	if (touser.Length() > 0) {
		ToWhom->Append(touser);
		ToWhom->SetSelection(0, TRUE);
	} else {
		wxStringTokenizer *wl =
			new wxStringTokenizer(xferbuf, "\n", FALSE);
		while (wl->HasMoreToken()) {
			buf = wl->NextToken();
                	extract(user, buf, 1);
			ToWhom->Append(user);
		}
	}

}



void SendExpress::OnButtonPressed(wxCommandEvent& whichbutton) {
	wxString target_user, sendcmd, recvcmd, msg;

	if (whichbutton.GetId() == BUTTON_CANCEL) {
		delete this;
	} else if (whichbutton.GetId() == BUTTON_SEND) {
		target_user = ToWhom->GetStringSelection();
		msg = TheMessage->GetValue();
		sendcmd = "SEXP " + target_user + "|-" ;
		if (citsock->serv_trans(sendcmd, recvcmd, msg) != 4) {
			wxMessageBox(recvcmd.Mid(4),
					"Error",
					wxOK | wxICON_EXCLAMATION,
					NULL, -1, -1);
		} else {
			delete this;
		}
	}
}
