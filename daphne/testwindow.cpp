// ============================================================================
// declarations
// ============================================================================

#include "includes.hpp"

// ----------------------------------------------------------------------------
// private classes
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// IDs for the controls and the menu commands
enum
{
	BUTTON_SENDCMD,
	BUTTON_CLOSE
};

// ----------------------------------------------------------------------------
// event tables and other macros for wxWindows
// ----------------------------------------------------------------------------

// the event tables connect the wxWindows events with the functions (event
// handlers) which process them. It can be also done at run-time, but for the
// simple menu events like this the static method is much simpler.
BEGIN_EVENT_TABLE(	TestWindow, wxMDIChildFrame)
	EVT_BUTTON(	BUTTON_SENDCMD,		TestWindow::OnButtonPressed)
	EVT_BUTTON(	BUTTON_CLOSE,		TestWindow::OnButtonPressed)
END_EVENT_TABLE()

// ============================================================================
// implementation
// ============================================================================


// ----------------------------------------------------------------------------
// the application class
// ----------------------------------------------------------------------------

// frame constructor
TestWindow::TestWindow(CitClient *sock, wxMDIParentFrame *MyMDI)
       : wxMDIChildFrame(MyMDI,	//parent
			-1,	//window id
			"Test Window",
			wxDefaultPosition,
			wxDefaultSize,
			wxDEFAULT_FRAME_STYLE | wxTAB_TRAVERSAL,
			"TestWindow"
			) {

	citsock = sock;

	// set the frame icon
	/* SetIcon(wxICON(mondrian)); */

	wxStaticText *sendcmd_label = new wxStaticText(this, -1, "Server command:");
	wxStaticText *recvcmd_label = new wxStaticText(this, -1, "Response:");
	wxStaticText *xfercmd_label = new wxStaticText(this, -1, "Send/receive data:");

	sendcmd = new wxTextCtrl(
		this,
		-1,
		"",
		wxDefaultPosition,
		wxDefaultSize,
		0, // no style
		wxDefaultValidator,
		"sendcmd"
		);

        wxLayoutConstraints *c1 = new wxLayoutConstraints;
        c1->top.SameAs(this, wxTop, 10);
        c1->left.SameAs(this, wxLeft, 5);
	c1->width.AsIs();
	c1->height.AsIs();
        sendcmd_label->SetConstraints(c1);

	wxLayoutConstraints *c2 = new wxLayoutConstraints;
	c2->left.RightOf(sendcmd_label, 5);
	c2->centreY.SameAs(sendcmd_label, wxCentreY);
	c2->height.AsIs();
	c2->right.SameAs(this, wxRight, 5);
	sendcmd->SetConstraints(c2);

	recvcmd = new wxTextCtrl(
		this,
		-1,
		"",
		wxDefaultPosition,
		wxDefaultSize,
		wxTE_READONLY,
		wxDefaultValidator,
		"sendcmd"
		);

	wxLayoutConstraints *c3 = new wxLayoutConstraints;
	c3->left.SameAs(sendcmd, wxLeft);
	c3->right.SameAs(sendcmd, wxRight);
	c3->height.AsIs();
	c3->top.Below(sendcmd, 5);
	recvcmd->SetConstraints(c3);

	wxLayoutConstraints *c4 = new wxLayoutConstraints;
	c4->left.SameAs(sendcmd_label, wxLeft);
	c4->centreY.SameAs(recvcmd, wxCentreY);
	c4->height.AsIs();
	c4->width.AsIs();
	recvcmd_label->SetConstraints(c4);
	
	cmd_button = new wxButton(
		this,
		BUTTON_SENDCMD,
		"Send command",
		wxPoint(10,300),
		wxSize(100,30),
		0L,
		wxDefaultValidator,
		"cmd_button"
		);

	close_button = new wxButton(
		this,
		BUTTON_CLOSE,
		"Close",
		wxPoint(150,300),
		wxSize(100,30),
		0L,
		wxDefaultValidator,
		"close_button"
		);

	wxLayoutConstraints *c5 = new wxLayoutConstraints;
	c5->left.SameAs(this, wxLeft, 5);
	c5->bottom.SameAs(this, wxBottom, 5);
	c5->width.AsIs();
	c5->height.AsIs();
	cmd_button->SetConstraints(c5);

	wxLayoutConstraints *c6 = new wxLayoutConstraints;
	c6->right.SameAs(this, wxRight, 5);
	c6->bottom.SameAs(this, wxBottom, 5);
	c6->width.AsIs();
	c6->height.AsIs();
	close_button->SetConstraints(c6);

	wxLayoutConstraints *c7 = new wxLayoutConstraints;
	c7->top.Below(recvcmd, 5);
	c7->centreX.SameAs(this, wxCentreX);
	c7->width.AsIs();
	c7->height.AsIs();
	xfercmd_label->SetConstraints(c7);

	xfercmd = new wxTextCtrl(
		this,
		-1,
		"",
		wxDefaultPosition,
		wxDefaultSize,
		wxTE_MULTILINE,
		wxDefaultValidator,
		"xfercmd"
		);

	wxLayoutConstraints *c8 = new wxLayoutConstraints;
	c8->left.SameAs(this, wxLeft, 5);
	c8->right.SameAs(this, wxRight, 5);
	c8->top.Below(xfercmd_label, 5);
	c8->bottom.Above(cmd_button, -5);
	xfercmd->SetConstraints(c8);

	cmd_button->SetDefault();
        SetAutoLayout(TRUE);
        Show(TRUE);
        Layout();

}



void TestWindow::OnButtonPressed(wxCommandEvent& whichbutton) {
	wxString sendbuf = "";
	wxString recvbuf = "";
	wxString xferbuf = "";

	if (whichbutton.GetId()==BUTTON_CLOSE) {
		delete this;
	}
	else if (whichbutton.GetId()==BUTTON_SENDCMD) {
		sendbuf = sendcmd->GetValue();
		recvcmd->Clear();
		xferbuf = xfercmd->GetValue();
		citsock->serv_trans(sendbuf, recvbuf, xferbuf);
		recvcmd->SetValue(recvbuf);
		xfercmd->SetValue(xferbuf);
	}
}
