#include "includes.hpp"

enum {
	BUTTON_CLOSE
};


BEGIN_EVENT_TABLE(EnterMessage, wxMDIChildFrame)
	EVT_BUTTON(BUTTON_CLOSE,	EnterMessage::OnButtonPressed)
END_EVENT_TABLE()


// frame constructor
EnterMessage::EnterMessage(
	CitClient *sock, wxMDIParentFrame *MyMDI, wxString roomname)
	: wxMDIChildFrame(MyMDI,	//parent
			-1,	//window id
			roomname + ": enter message",
			wxDefaultPosition,
			wxDefaultSize,
			wxDEFAULT_FRAME_STYLE,
			roomname
			) {

	wxString sendcmd, recvcmd;

	citsock = sock;
	citMyMDI = MyMDI;
	ThisRoom = roomname;

        wxButton *close_button = new wxButton(
                this,
                BUTTON_CLOSE,
                " Close ",
                wxDefaultPosition);

        wxLayoutConstraints *c1 = new wxLayoutConstraints;
        c1->bottom.SameAs(this, wxBottom, 2);
        c1->height.AsIs();
        c1->width.AsIs();
        c1->right.SameAs(this, wxRight, 2);
        close_button->SetConstraints(c1);

	SetAutoLayout(TRUE);
	Show(TRUE);
}



void EnterMessage::OnButtonPressed(wxCommandEvent& whichbutton) {
        if (whichbutton.GetId() == BUTTON_CLOSE) {
                delete this;
	}
}
