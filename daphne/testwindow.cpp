// ============================================================================
// declarations
// ============================================================================


#include <wx/wx.h>
#include "citclient.hpp"
#include "testwindow.hpp"

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
			wxDEFAULT_FRAME_STYLE,
			"TestWindow"
			) {

	citsock = sock;

	// set the frame icon
	/* SetIcon(wxICON(mondrian)); */

	panel = new wxPanel(this);

	sendcmd = new wxTextCtrl(
		panel,
		-1,
		"",
		wxPoint(10,10),
		wxSize(300,30),
		0, // no style
		wxDefaultValidator,
		"sendcmd"
		);

	recvcmd = new wxTextCtrl(
		panel,
		-1,
		"",
		wxPoint(10,100),
		wxSize(300,30),
		0, // no style
		wxDefaultValidator,
		"sendcmd"
		);

	cmd_button = new wxButton(
		panel,
		BUTTON_SENDCMD,
		"Send command",
		wxPoint(10,300),
		wxSize(100,30),
		0L,
		wxDefaultValidator,
		"cmd_button"
		);

	close_button = new wxButton(
		panel,
		BUTTON_CLOSE,
		"Close",
		wxPoint(100,300),
		wxSize(100,30),
		0L,
		wxDefaultValidator,
		"close_button"
		);

	Show(TRUE);
}



void TestWindow::OnButtonPressed(wxCommandEvent& whichbutton) {
	wxString sendbuf = "";
	wxString recvbuf = "";

	if (whichbutton.GetId()==BUTTON_CLOSE) {
		delete this;
	}
	else if (whichbutton.GetId()==BUTTON_SENDCMD) {
		sendbuf = sendcmd->GetValue();
		recvcmd->Clear();
		citsock->serv_trans(sendbuf, recvbuf);
		recvcmd->SetValue(recvbuf);
	}
}
