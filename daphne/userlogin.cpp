// ============================================================================
// declarations
// ============================================================================

#include "includes.hpp"

// ----------------------------------------------------------------------------
// private classes
// ----------------------------------------------------------------------------

// userlogin is the frame for logging in.

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// IDs for the controls and the menu commands
enum
{
	BUTTON_LOGIN,
	BUTTON_NEWUSER,
	BUTTON_EXIT
};

// ----------------------------------------------------------------------------
// event tables and other macros for wxWindows
// ----------------------------------------------------------------------------

// the event tables connect the wxWindows events with the functions (event
// handlers) which process them. It can be also done at run-time, but for the
// simple menu events like this the static method is much simpler.
BEGIN_EVENT_TABLE(	UserLogin, wxMDIChildFrame)
	EVT_BUTTON(	BUTTON_LOGIN,		UserLogin::OnButtonPressed)
	EVT_BUTTON(	BUTTON_NEWUSER,		UserLogin::OnButtonPressed)
	EVT_BUTTON(	BUTTON_EXIT,		UserLogin::OnButtonPressed)
END_EVENT_TABLE()

// ============================================================================
// implementation
// ============================================================================


// ----------------------------------------------------------------------------
// the application class
// ----------------------------------------------------------------------------

// frame constructor
UserLogin::UserLogin(CitClient *sock, wxMDIParentFrame *MyMDI)
       : wxMDIChildFrame(MyMDI,	//parent
			-1,	//window id
			" Please log in ",
			wxDefaultPosition,
			wxDefaultSize,
			wxDEFAULT_FRAME_STYLE,
			"UserLogin"
			) {

	wxString sendcmd, recvcmd;
	wxStringList xferbuf;

	citsock = sock;
	citMyMDI = MyMDI;

	// set the frame icon
	/* SetIcon(wxICON(mondrian)); */

	wxStaticText *username_label = new wxStaticText(
		this, -1, "User name:",
		wxDefaultPosition, wxDefaultSize, 0, ""
		);

	username = new wxTextCtrl(
		this,
		-1,
		"",
		wxPoint(10,10),
		wxSize(300,30),
		0, // no style
		wxDefaultValidator,
		"sendcmd"
		);

	wxStaticText *password_label = new wxStaticText(
		this, -1, "Password:",
		wxDefaultPosition, wxDefaultSize, 0, ""
		);

	password = new wxTextCtrl(
		this,
		-1,
		"",
		wxPoint(10,100),
		wxSize(300,30),
		wxTE_PASSWORD,
		wxDefaultValidator,
		"sendcmd"
		);

	login_button = new wxButton(
		this,
		BUTTON_LOGIN,
		"Login",
		wxPoint(100,100),
		wxSize(100,30),
		0L,
		wxDefaultValidator,
		"login_button"
		);

	newuser_button = new wxButton(
		this,
		BUTTON_NEWUSER,
		"New user",
		wxPoint(200,200),
		wxSize(100,30),
		0L,
		wxDefaultValidator,
		"newuser_button"
		);

	exit_button = new wxButton(
		this,
		BUTTON_EXIT,
		"Exit",
		wxPoint(300,300),
		wxSize(100,30),
		0L,
		wxDefaultValidator,
		"exit_button"
		);

	wxTextCtrl *hello = new wxTextCtrl(this, -1,
		"", //value
		wxDefaultPosition, wxDefaultSize,
		wxTE_MULTILINE | wxTE_READONLY,
		wxDefaultValidator, "");

	wxStaticText *humannode = new wxStaticText(
		this, -1, citsock->HumanNode,
		wxDefaultPosition, wxDefaultSize, 0, "");
	humannode->SetBackgroundColour(wxColour(0x00, 0x00, 0x77));
	humannode->SetForegroundColour(wxColour(0xFF, 0xFF, 0x00));
	/*humannode->SetFont(wxFont(24, wxDEFAULT, wxNORMAL, wxNORMAL,
				FALSE, ""));*/

	wxLayoutConstraints *t1 = new wxLayoutConstraints;
	t1->top.SameAs(this, wxTop, 10);
	t1->height.AsIs();
	t1->centreX.SameAs(this, wxCentreX);
	t1->width.AsIs();
	humannode->SetConstraints(t1);

	wxLayoutConstraints *h0 = new wxLayoutConstraints;
	h0->top.Below(humannode, 10);
	h0->bottom.Above(username, -10);
	h0->left.SameAs(this, wxLeft, 10);
	h0->right.SameAs(this, wxRight, 10);
	hello->SetConstraints(h0);

	wxLayoutConstraints *c1 = new wxLayoutConstraints;
	c1->bottom.SameAs(this, wxBottom, 10);		// 10 from the bottom
	c1->centreX.SameAs(this, wxCentreX);		// in the middle
	c1->height.AsIs(); c1->width.AsIs();
	newuser_button->SetConstraints(c1);

	wxLayoutConstraints *c2 = new wxLayoutConstraints;
	c2->bottom.SameAs(newuser_button, wxBottom);
	c2->right.LeftOf(newuser_button, 10);		// 10 from middle btn
	c2->height.AsIs(); c2->width.AsIs();
	login_button->SetConstraints(c2);

	wxLayoutConstraints *c3 = new wxLayoutConstraints;
	c3->bottom.SameAs(newuser_button, wxBottom);
	c3->left.RightOf(newuser_button, 10);		// 10 from middle btn
	c3->height.AsIs(); c3->width.AsIs();
	exit_button->SetConstraints(c3);

	wxLayoutConstraints *c6 = new wxLayoutConstraints;
	c6->left.SameAs(this, wxLeft, 10);
	c6->bottom.SameAs(password, wxBottom);
	c6->width.AsIs(); c6->height.AsIs();
	password_label->SetConstraints(c6);

	wxLayoutConstraints *c7 = new wxLayoutConstraints;
	c7->left.SameAs(this, wxLeft, 10);
	c7->bottom.SameAs(username, wxBottom);
	c7->width.AsIs(); c7->height.AsIs();
	username_label->SetConstraints(c7);

	wxLayoutConstraints *c4 = new wxLayoutConstraints;
	c4->bottom.Above(newuser_button, -10);
	c4->left.RightOf(username_label, 10);
	c4->height.AsIs();
	c4->right.SameAs(this, wxRight, 10);
	password->SetConstraints(c4);

	wxLayoutConstraints *c5 = new wxLayoutConstraints;
	c5->bottom.Above(password, -10);
	c5->left.SameAs(password, wxLeft);
	c5->height.AsIs();
	c5->width.SameAs(password, wxWidth);
	username->SetConstraints(c5);

	SetAutoLayout(TRUE);
	Show(TRUE);

	sendcmd = "MESG hello";
	if (citsock->serv_trans(sendcmd, recvcmd, xferbuf)==1) {
		ListToMultiline(recvcmd, xferbuf);
		hello->SetValue(recvcmd);
	}


}



void UserLogin::OnButtonPressed(wxCommandEvent& whichbutton) {
	wxString sendbuf;
	wxString recvbuf;
	int r;

	if (whichbutton.GetId() == BUTTON_EXIT) {
		sendbuf = "QUIT";
		citsock->serv_trans(sendbuf);
		exit(0);
	}

	if (whichbutton.GetId() == BUTTON_LOGIN) {
		sendbuf = "USER " + username->GetValue();
		r = citsock->serv_trans(sendbuf, recvbuf);
		if (r != 3) {
			wxMessageDialog nouser(this,
				recvbuf.Mid(4,32767),
				"Error",
				wxOK | wxCENTRE | wxICON_INFORMATION,
				wxDefaultPosition);
			nouser.ShowModal();
		} else {
			sendbuf = "PASS ";
			sendbuf += password->GetValue();
			r = citsock->serv_trans(sendbuf, recvbuf);
			if (r != 2) {
				wxMessageDialog nopass(this,
					recvbuf.Mid(4,32767),
					"Error",
					wxOK | wxCENTRE | wxICON_INFORMATION,
					wxDefaultPosition);
				nopass.ShowModal();
			} else {
				BeginSession(recvbuf);
				delete this;	// dismiss the login box
			}
		}
	}

	if (whichbutton.GetId() == BUTTON_NEWUSER) {
		sendbuf = "NEWU " + username->GetValue();
		r = citsock->serv_trans(sendbuf, recvbuf);
		if (r != 2) {
			wxMessageDialog nouser(this,
				recvbuf.Mid(4,32767),
				"Error",
				wxOK | wxCENTRE | wxICON_INFORMATION,
				wxDefaultPosition);
			nouser.ShowModal();
		} else {
			sendbuf = "SETP " + password->GetValue();
			citsock->serv_trans(sendbuf);
			BeginSession(recvbuf);
			delete this;		// dismiss the login box
		}
	}
}



void UserLogin::BeginSession(wxString serv_response) {
	wxString junk, username;

	extract(username, serv_response.Mid(4, 255), 0);
	BigMDI->SetStatusText(username, 1);
	citsock->GotoRoom("_BASEROOM_", "", junk);

	// FIX ... add code here to perform registration if necessary

	RoomList->LoadRoomList();
	
}
