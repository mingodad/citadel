// $Id$

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
	BUTTON_ADD,
	BUTTON_FINISH,
};

// ----------------------------------------------------------------------------
// event tables and other macros for wxWindows
// ----------------------------------------------------------------------------

// the event tables connect the wxWindows events with the functions (event
// handlers) which process them. It can be also done at run-time, but for the
// simple menu events like this the static method is much simpler.
BEGIN_EVENT_TABLE(	Hosts, wxMDIChildFrame)
	EVT_BUTTON(	BUTTON_ADD, 		Hosts::OnButtonPressed)
	EVT_BUTTON(	BUTTON_FINISH,		Hosts::OnButtonPressed)
END_EVENT_TABLE()

// ============================================================================
// implementation
// ============================================================================


// ----------------------------------------------------------------------------
// the application class
// ----------------------------------------------------------------------------

// frame constructor
Hosts::Hosts(	CitClient *sock,
				wxMDIParentFrame *MyMDI)
       : wxMDIChildFrame(MyMDI,	//parent
			-1,	//window id
			" Hosts ",
			wxDefaultPosition,
			wxDefaultSize,
			wxDEFAULT_FRAME_STYLE | wxTAB_TRAVERSAL,
			"Hosts"
			) {
	
	wxString buf;

	citsock = sock;
	citMyMDI = MyMDI;

	// set the frame icon
	/* SetIcon(wxICON(mondrian)); */

	wxButton *add_button = new wxButton(
		this,
		BUTTON_ADD,
		"Add",
		wxPoint(200,200),
		wxSize(100,30),
		0L,
		wxDefaultValidator,
		"add_button"
		);

	wxButton *finish_button = new wxButton(
		this,
		BUTTON_FINISH,
		"Finish",
		wxPoint(300,300),
		wxSize(100,30),
		0L,
		wxDefaultValidator,
		"finish_button"
		);

	wxLayoutConstraints *c1 = new wxLayoutConstraints;
	c1->bottom.SameAs(this, wxBottom, 5);
	c1->left.SameAs(this, wxLeft, 10);
	c1->height.AsIs(); c1->width.AsIs();
	add_button->SetConstraints(c1);

	wxLayoutConstraints *c3 = new wxLayoutConstraints;
	c3->bottom.SameAs(add_button, wxBottom);
	c3->right.SameAs(this, wxRight, 10);
	c3->height.AsIs(); c3->width.AsIs();
	finish_button->SetConstraints(c3);

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


/*	wxLayoutConstraints *c8 = new wxLayoutConstraints;
	c8->centreY.SameAs(server_port, wxCentreY);
	c8->left.RightOf(server_port, 5);
	c8->width.AsIs(); c8->height.AsIs();
	server_autoconnect->SetConstraints(c8); */





	SetAutoLayout(TRUE);
	Show(TRUE);
        Layout();
}



void Hosts::OnButtonPressed(wxCommandEvent& whichbutton) {
	if (whichbutton.GetId() == BUTTON_FINISH) {
		delete this;
	} else if (whichbutton.GetId() == BUTTON_ADD) {
		ini->Write("/BBSList/Host", server_host->GetValue());
		ini->Write("/BBSList/Port", server_port->GetValue());
				
		ini->Flush(FALSE);
	}
}
