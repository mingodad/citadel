// ============================================================================
// declarations
// ============================================================================


#include <wx/wx.h>
#include "citclient.hpp"
#include "userlogin.hpp"

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
BEGIN_EVENT_TABLE(	UserLogin, wxFrame)
	EVT_BUTTON(	BUTTON_LOGIN,		UserLogin::OnButtonPressed)
END_EVENT_TABLE()

// ============================================================================
// implementation
// ============================================================================


// ----------------------------------------------------------------------------
// the application class
// ----------------------------------------------------------------------------

// frame constructor
UserLogin::UserLogin(CitClient *sock)
       : wxFrame((wxFrame *)NULL, -1, "", wxPoint(50,50), wxSize(500,350)) {

    // set the frame icon
    /* SetIcon(wxICON(mondrian)); */

	panel = new wxPanel(this);

	username = new wxTextCtrl(
		panel,
		-1,
		"",
		wxPoint(10,10),
		wxSize(300,30),
		0, // no style
		wxDefaultValidator,
		"sendcmd"
		);

	password = new wxTextCtrl(
		panel,
		-1,
		"",
		wxPoint(10,100),
		wxSize(300,30),
		0, // no style
		wxDefaultValidator,
		"sendcmd"
		);

	login_button = new wxButton(
		panel,
		BUTTON_LOGIN,
		"Login",
		wxPoint(10,300),
		wxSize(100,30),
		0L,
		wxDefaultValidator,
		"login_button"
		);

	newuser_button = new wxButton(
		panel,
		BUTTON_NEWUSER,
		"New user",
		wxPoint(120,300),
		wxSize(100,30),
		0L,
		wxDefaultValidator,
		"newuser_button"
		);

	exit_button = new wxButton(
		panel,
		BUTTON_EXIT,
		"Exit",
		wxPoint(230,300),
		wxSize(100,30),
		0L,
		wxDefaultValidator,
		"exit_button"
		);

	Show(TRUE);
}



void UserLogin::OnButtonPressed(wxCommandEvent& whichbutton) {
}
