// ============================================================================
// declarations
// ============================================================================


#include <wx/wx.h>
#include <wx/listctrl.h>
#include "citclient.hpp"
#include "express_message.hpp"
#include "utils.h"

// ----------------------------------------------------------------------------
// private classes
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// IDs for the controls and the menu commands
enum
{
	BUTTON_OK,
	BUTTON_REPLY
};

// ----------------------------------------------------------------------------
// event tables and other macros for wxWindows
// ----------------------------------------------------------------------------

// the event tables connect the wxWindows events with the functions (event
// handlers) which process them. It can be also done at run-time, but for the
// simple menu events like this the static method is much simpler.
BEGIN_EVENT_TABLE(	express_message, wxFrame)
	EVT_BUTTON(	BUTTON_OK,	express_message::OnButtonPressed)
END_EVENT_TABLE()

// ============================================================================
// implementation
// ============================================================================


// ----------------------------------------------------------------------------
// the application class
// ----------------------------------------------------------------------------

// frame constructor
express_message::express_message(
			CitClient *sock,
			wxString sender,
			wxString sendsys,
			wxStringList msg)
       		: wxFrame(
			NULL,	//parent
			-1,	//window id
			"Express message",
			wxDefaultPosition,
			wxSize(500, 200),
			wxDEFAULT_FRAME_STYLE,
			"express_message"
			) {

	wxString more_informative_title;
	wxString stringized_message;

	citsock = sock;

	ListToMultiline(stringized_message, msg);

	// set the frame icon
	/* SetIcon(wxICON(mondrian)); */

	more_informative_title = 
		"Express message from " + sender + " @ " + sendsys + "..." ;

	SetTitle(more_informative_title);

        wxButton *ok_button = new wxButton(
                this,
                BUTTON_OK,
                " OK ",
                wxPoint(100,100),
                wxSize(100,30),
                0L,
                wxDefaultValidator,
                "ok_button"
                );

        wxButton *reply_button = new wxButton(
                this,
                BUTTON_REPLY,
                " Reply ",
                wxPoint(100,100),
                wxSize(100,30),
                0L,
                wxDefaultValidator,
                "reply_button"
                );

	wxTextCtrl *msgbox = new wxTextCtrl(
		this,
		-1,
		stringized_message,
		wxDefaultPosition,
		wxDefaultSize,
		wxTE_MULTILINE | wxTE_READONLY,
		wxDefaultValidator,
		"msgbox"
		);

        wxLayoutConstraints *c0 = new wxLayoutConstraints;
        c0->bottom.SameAs(this, wxBottom, 10);
        c0->left.SameAs(this, wxLeft, 10);
        c0->height.AsIs(); c0->width.AsIs();
        ok_button->SetConstraints(c0);

        wxLayoutConstraints *c1 = new wxLayoutConstraints;
        c1->bottom.SameAs(this, wxBottom, 10);
        c1->right.SameAs(this, wxRight, 10);
        c1->height.AsIs(); c1->width.AsIs();
        reply_button->SetConstraints(c1);

	wxLayoutConstraints *c2 = new wxLayoutConstraints;
	c2->top.SameAs(this, wxTop, 10);
	c2->left.SameAs(this, wxLeft, 10);
	c2->right.SameAs(this, wxRight, 10);
	c2->bottom.Above(ok_button, -10);
	msgbox->SetConstraints(c2);

	SetAutoLayout(TRUE);
	Show(TRUE);

}


void express_message::OnButtonPressed(wxCommandEvent& whichbutton) {
        if (whichbutton.GetId() == BUTTON_OK) {
                delete this;
        }
}
