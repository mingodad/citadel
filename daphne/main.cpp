/////////////////////////////////////////////////////////////////////////////
// Name:        main.cpp
// Purpose:     Main screen type thing
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================


#include <stdio.h>
#include <wx/wx.h>
#include <wx/socket.h>

#include "citclient.hpp"
#include "userlogin.hpp"
#include "testwindow.hpp"

// ----------------------------------------------------------------------------
// private classes
// ----------------------------------------------------------------------------

// Define a new application type, each program should derive a class from wxApp
class Daphne : public wxApp
{
public:
    // override base class virtuals
    // ----------------------------

    // this one is called on application startup and is a good place for the app
    // initialization (doing it here and not in the ctor allows to have an error
    // return: if OnInit() returns false, the application terminates)
    virtual bool OnInit();
};

// Define a new frame type: this is going to be our main frame
class MyFrame : public wxMDIParentFrame
{
public:
    // constructor(s)
    MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size);

	// event handlers (these functions should _not_ be virtual)
	void OnQuit(wxCommandEvent& event);
	void OnAbout(wxCommandEvent& event);
	void OnDoCmd(wxCommandEvent& event);
	void OnConnect(wxCommandEvent& event);
private:
	void OnWindowMenu(wxCommandEvent& cmd);
	wxButton *do_cmd;
	// any class wishing to process wxWindows events must use this macro
	DECLARE_EVENT_TABLE()
};

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// IDs for the controls and the menu commands
enum
{
	IG_Quit,
	IG_About,
	IG_Text,
	MENU_CONNECT,
	WMENU_CASCADE,
	WMENU_TILE,
	WMENU_ARRANGE,
	WMENU_NEXT,
	WMENU_PREVIOUS,
	BUTTON_DO_CMD
};

// ----------------------------------------------------------------------------
// event tables and other macros for wxWindows
// ----------------------------------------------------------------------------

// the event tables connect the wxWindows events with the functions (event
// handlers) which process them. It can be also done at run-time, but for the
// simple menu events like this the static method is much simpler.
BEGIN_EVENT_TABLE(	MyFrame, wxMDIParentFrame)
	EVT_MENU(	IG_Quit,		MyFrame::OnQuit)
	EVT_MENU(	IG_About,		MyFrame::OnAbout)
	EVT_MENU(	MENU_CONNECT,		MyFrame::OnConnect)
	EVT_MENU(	WMENU_CASCADE,		MyFrame::OnWindowMenu)
	EVT_MENU(	WMENU_TILE,		MyFrame::OnWindowMenu)
	EVT_MENU(	WMENU_ARRANGE,		MyFrame::OnWindowMenu)
	EVT_MENU(	WMENU_NEXT,		MyFrame::OnWindowMenu)
	EVT_MENU(	WMENU_PREVIOUS,		MyFrame::OnWindowMenu)
	EVT_BUTTON(	BUTTON_DO_CMD,		MyFrame::OnDoCmd)
END_EVENT_TABLE()

// Create a new application object: this macro will allow wxWindows to create
// the application object during program execution (it's better than using a
// static object for many reasons) and also declares the accessor function
// wxGetApp() which will return the reference of the right type (i.e. Daphne and
// not wxApp)
IMPLEMENT_APP(Daphne)

// ============================================================================
// implementation
// ============================================================================

wxRadioBox *DevSelect;
wxSlider *ndims;
CitClient *citadel;

// ----------------------------------------------------------------------------
// the application class
// ----------------------------------------------------------------------------

// `Main program' equivalent: the program execution "starts" here
bool Daphne::OnInit()
{

	citadel = new CitClient();

    // Create the main application window
    MyFrame *frame = new MyFrame("Daphne",
                                 wxPoint(10, 10), wxSize(600, 450));

    // Show it and tell the application that it's our main window
    // @@@ what does it do exactly, in fact? is it necessary here?
    frame->Show(TRUE);
    SetTopWindow(frame);

    // success: wxApp::OnRun() will be called which will enter the main message
    // loop and the application will run. If we returned FALSE here, the
    // application would exit immediately.
    return TRUE;
}

// ----------------------------------------------------------------------------
// main frame
// ----------------------------------------------------------------------------

// frame constructor
MyFrame::MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size)
       : wxMDIParentFrame(
		(wxMDIParentFrame *)NULL,
		-1,
		title, pos, size, wxDEFAULT_FRAME_STYLE
		) {

	// create a menu bar
	wxMenu *menuFile = new wxMenu;
	menuFile->Append(MENU_CONNECT, "&Connect");
	menuFile->AppendSeparator(); 
	menuFile->Append(IG_Quit, "E&xit");

	wxMenu *menuWindow = new wxMenu;
	menuWindow->Append(WMENU_CASCADE, "&Cascade");
	menuWindow->Append(WMENU_TILE, "&Tile");
	menuWindow->Append(WMENU_ARRANGE, "&Arrange icons");
	menuWindow->Append(WMENU_NEXT, "&Next window");
	menuWindow->Append(WMENU_PREVIOUS, "&Previous window");

	wxMenu *menuHelp = new wxMenu;
	menuHelp->Append(IG_About, "&About...");

	// now append the freshly created menu to the menu bar...
	wxMenuBar *menuBar = new wxMenuBar;
	menuBar->Append(menuFile, "&File");
	menuBar->Append(menuWindow, "&Window");
	menuBar->Append(menuHelp, "&Help");

	// ... and attach this menu bar to the frame
	SetMenuBar(menuBar);

	// create a status bar just for fun (by default with 1 pane only)
	CreateStatusBar(1);
	SetStatusText("Not connected");
}


// event handlers

void MyFrame::OnQuit(wxCommandEvent& WXUNUSED(event))
{
	// Kill the client connection
	citadel->detach();

	// TRUE is to force the frame to close
	Close(TRUE);
}


// Window menu handler
void MyFrame::OnWindowMenu(wxCommandEvent& cmd) {
	int id;
	
	id = cmd.GetId();
	if (id == WMENU_CASCADE)		Cascade();
	else if (id == WMENU_TILE)		Tile();
	else if (id = WMENU_ARRANGE)		ArrangeIcons();
	else if (id == WMENU_NEXT)		ActivateNext();
	else if (id == WMENU_PREVIOUS)		ActivatePrevious();
}



void MyFrame::OnAbout(wxCommandEvent& WXUNUSED(event))
{
	wxString msg;
	msg.Printf(
	  "This is the development version of a project code-named 'Daphne',\n"
	  "a GUI client for the Citadel/UX groupware system.  It is being\n"
	  "developed using the wxWindows toolkit, and therefore should be\n"
	  "easy to port to Linux, Windows, and eventually Macintosh."
	  );
	wxMessageBox(msg, "Daphne", wxOK | wxICON_INFORMATION, this);
}

void MyFrame::OnDoCmd(wxCommandEvent& whichbutton) {
}

void MyFrame::OnConnect(wxCommandEvent& unused) {
	int retval;

	if (citadel->IsConnected()) {
		wxMessageBox("You are already connected to a Citadel server.",
			"Oops!");
	} else {
		retval = citadel->attach("uncnsrd", "citadel");
		if (retval == 0) {
    			SetStatusText("** connected **");
			new TestWindow(citadel, this);
			new UserLogin(citadel, this);
		} else {
			wxMessageBox("Could not connect to server.", "Error");
		}
	}
}
