// =========================================================================
// declarations
// =========================================================================

#include "includes.hpp"
#include <wx/tab.h>

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// IDs for the controls and the menu commands
enum
{
	BUTTON_SAVE,
	BUTTON_CANCEL,
	TAB_SERVER,
	TAB_MESSAGES
};

// ----------------------------------------------------------------------------
// event tables and other macros for wxWindows
// ----------------------------------------------------------------------------

// the event tables connect the wxWindows events with the functions (event
// handlers) which process them. It can be also done at run-time, but for the
// simple menu events like this the static method is much simpler.
BEGIN_EVENT_TABLE(	Preferences, wxMDIChildFrame)
	EVT_BUTTON(	BUTTON_SAVE,		Preferences::OnButtonPressed)
	EVT_BUTTON(	BUTTON_CANCEL,		Preferences::OnButtonPressed)
END_EVENT_TABLE()

// ============================================================================
// implementation
// ============================================================================


// ----------------------------------------------------------------------------
// the application class
// ----------------------------------------------------------------------------

// frame constructor
Preferences::Preferences(	CitClient *sock,
				wxMDIParentFrame *MyMDI)
       : wxMDIChildFrame(MyMDI,	//parent
			-1,	//window id
			" Preferences ",
			wxDefaultPosition,
			wxDefaultSize,
			wxDEFAULT_FRAME_STYLE | wxTAB_TRAVERSAL,
			"Preferences"
			) {
	
	wxString buf;

	citsock = sock;
	citMyMDI = MyMDI;

	// set the frame icon
	/* SetIcon(wxICON(mondrian)); */

	wxButton *save_button = new wxButton(
		this,
		BUTTON_SAVE,
		"Save",
		wxPoint(200,200),
		wxSize(100,30),
		0L,
		wxDefaultValidator,
		"save_button"
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
	save_button->SetConstraints(c1);

	wxLayoutConstraints *c3 = new wxLayoutConstraints;
	c3->bottom.SameAs(save_button, wxBottom);
	c3->right.SameAs(this, wxRight, 10);
	c3->height.AsIs(); c3->width.AsIs();
	cancel_button->SetConstraints(c3);

	wxStaticText *server_host_label = new wxStaticText(
		this, -1, "Server host");

	server_host = new wxTextCtrl(this, -1);
	
	wxStaticText *server_port_label = new wxStaticText(
		this, -1, "Server port");
	
	server_port = new wxTextCtrl(this, -1);

	wxLayoutConstraints *c4 = new wxLayoutConstraints;
	c4->top.SameAs(this, wxTop, 10);
	c4->left.SameAs(this, wxLeft, 10);
	c4->height.AsIs(); c4->width.AsIs();
	server_host_label->SetConstraints(c4);

	wxLayoutConstraints *c5 = new wxLayoutConstraints;
	c5->centreY.SameAs(server_host_label, wxCentreY);
	c5->left.RightOf(server_host_label, 10);
	c5->right.SameAs(this, wxRight, 10);
	c5->height.AsIs();
	server_host->SetConstraints(c5);

	wxLayoutConstraints *c6 = new wxLayoutConstraints;
	c6->top.Below(server_host_label, 15);
	c6->left.SameAs(this, wxLeft, 10);
	c6->height.AsIs(); c6->width.AsIs();
	server_port_label->SetConstraints(c6);

	wxLayoutConstraints *c7 = new wxLayoutConstraints;
	c7->centreY.SameAs(server_port_label, wxCentreY);
	c7->left.SameAs(server_host, wxLeft);
	c7->right.PercentOf(this, wxWidth, 50);
	c7->height.AsIs();
	server_port->SetConstraints(c7);

	server_autoconnect = new wxCheckBox(this, -1,
		"Automatically connect at startup");

	wxLayoutConstraints *c8 = new wxLayoutConstraints;
	c8->centreY.SameAs(server_port, wxCentreY);
	c8->left.RightOf(server_port, 5);
	c8->width.AsIs(); c8->height.AsIs();
	server_autoconnect->SetConstraints(c8);

	ini->Read("/Citadel Server/Host", &buf, DEFAULT_HOST);
	server_host->SetValue(buf);

	ini->Read("/Citadel Server/Port", &buf, DEFAULT_PORT);
	server_port->SetValue(buf);

	ini->Read("/Citadel Server/ConnectOnStartup", &buf, "no");
	server_autoconnect->SetValue(
		((!buf.CmpNoCase("yes")) ? TRUE : FALSE));

	SetAutoLayout(TRUE);
	Show(TRUE);
        Layout();
}



void Preferences::OnButtonPressed(wxCommandEvent& whichbutton) {
	if (whichbutton.GetId() == BUTTON_CANCEL) {
		delete this;
	} else if (whichbutton.GetId() == BUTTON_SAVE) {
		ini->Write("/Citadel Server/Host", server_host->GetValue());
		ini->Write("/Citadel Server/Port", server_port->GetValue());
		ini->Write("/Citadel Server/ConnectOnStartup",
		    ((server_autoconnect->GetValue()==TRUE) ? "yes" : "no"));
		ini->Flush();
		delete this;
	}
}
