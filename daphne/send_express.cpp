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
				wxString *touser)
       : wxMDIChildFrame(MyMDI,	//parent
			-1,	//window id
			" Page another user ",
			wxDefaultPosition,
			wxDefaultSize,
			wxDEFAULT_FRAME_STYLE,
			"SendExpress"
			) {

	wxString sendcmd, recvcmd;
	wxStringList xferbuf;

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

	SetAutoLayout(TRUE);
	Show(TRUE);

}



void SendExpress::OnButtonPressed(wxCommandEvent& whichbutton) {
	wxString sendbuf;
	wxString recvbuf;
	int r;

	if (whichbutton.GetId() == BUTTON_CANCEL) {
		delete this;
	}

}
