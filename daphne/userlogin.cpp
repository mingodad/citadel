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
			wxDEFAULT_FRAME_STYLE | wxTAB_TRAVERSAL,
			"UserLogin"
			) {

	wxString sendcmd, recvcmd, xferbuf, buf;
	wxPanel *banner;

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
	login_button->SetDefault();

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
		"Close",
		wxPoint(300,300),
		wxSize(100,30),
		0L,
		wxDefaultValidator,
		"exit_button"
		); 

	wxHtmlWindow *hello = new wxHtmlWindow(this);

        // Set up a panel for the title...
        banner = new wxPanel(this, -1);
        banner->SetBackgroundColour(wxColour(0x00, 0x00, 0x77));
        banner->SetForegroundColour(wxColour(0xFF, 0xFF, 0x00));

        wxLayoutConstraints *b1 = new wxLayoutConstraints;
        b1->top.SameAs(this, wxTop, 2);
        b1->left.SameAs(this, wxLeft, 2);
        b1->right.SameAs(this, wxRight, 2);
        b1->height.PercentOf(this, wxHeight, 10);
        banner->SetConstraints(b1);

        wxStaticText *rname = new wxStaticText(banner, -1, citsock->HumanNode);
        rname->SetFont(wxFont(16, wxDEFAULT, wxNORMAL, wxNORMAL));
        rname->SetForegroundColour(wxColour(0xFF, 0xFF, 0x00));
        wxLayoutConstraints *t2 = new wxLayoutConstraints;
        t2->top.SameAs(banner, wxTop, 1);
        t2->centreX.SameAs(banner, wxCentreX);
        t2->width.SameAs(banner, wxWidth);
        t2->height.AsIs();
        rname->SetConstraints(t2);

	wxLayoutConstraints *h0 = new wxLayoutConstraints;
	h0->top.Below(banner, 10);
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
        Maximize(TRUE);
	Show(TRUE);

	sendcmd = "MESG hello";
	if (citsock->serv_trans(sendcmd, recvcmd, xferbuf)==1) {
		variformat_to_html(buf, xferbuf, FALSE); 
		buf = "<HTML><font size=-1><BODY>"
			+ buf + "</BODY></HTML></font>\n"; 
		hello->SetPage(buf); 
	}

	username->SetFocus();
}



void UserLogin::OnButtonPressed(wxCommandEvent& whichbutton) {
	wxString sendbuf;
	wxString recvbuf;
	int r;

	if (whichbutton.GetId() == BUTTON_EXIT) {
		if (citadel->IsConnected()) {
		sendbuf = "QUIT";
		citsock->serv_trans(sendbuf);
		BigMDI->SetStatusText("Not connected");
		delete this; 
	      }	else {
		BigMDI->SetStatusText("Not connected");
		delete this; }

	}

	if (whichbutton.GetId() == BUTTON_LOGIN) {
		sendbuf = "USER " + username->GetValue();
		r = citsock->serv_trans(sendbuf, recvbuf);
		if (r != 3) {
			wxMessageDialog nouser(this,
				recvbuf.Mid(4),
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
					recvbuf.Mid(4),
					"Error",
					wxOK | wxCENTRE | wxICON_INFORMATION,
					wxDefaultPosition);
				nopass.ShowModal();
			} else {
				BeginSession(recvbuf);
				citsock->curr_pass = password->GetValue();
				delete this;	// dismiss the login box
			}
		}
	}

	if (whichbutton.GetId() == BUTTON_NEWUSER) {
		sendbuf = "NEWU " + username->GetValue();
		r = citsock->serv_trans(sendbuf, recvbuf);
		if (r != 2) {
			wxMessageDialog nouser(this,
				recvbuf.Mid(4),
				"Error",
				wxOK | wxCENTRE | wxICON_INFORMATION,
				wxDefaultPosition);
			nouser.ShowModal();
		} else {
			sendbuf = "SETP " + password->GetValue();
			citsock->serv_trans(sendbuf);
			BeginSession(recvbuf);
			citsock->curr_pass = password->GetValue();
			delete this;		// dismiss the login box
		}
	}
}



void UserLogin::BeginSession(wxString serv_response) {
	wxString junk;

	extract(citsock->curr_user, serv_response.Mid(4), 0);
	BigMDI->SetStatusText(citsock->curr_user, 1);
	citsock->GotoRoom("_BASEROOM_", "", junk);
	citsock->axlevel = extract_int(serv_response.Mid(4), 1);

	// FIX ... add code here to perform registration if necessary

	RoomList->LoadRoomList();
	
}
