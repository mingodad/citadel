#include <wx/wx.h>
#include <wx/listctrl.h>
#include <wx/socket.h>
#include <wx/log.h>
#include <wx/imaglist.h>
#include <wx/treectrl.h>
#include <wx/toolbar.h>
#include <wx/tokenzr.h>
#include <wxhtml/wxhtml.h>

#define MAXFLOORS	128
#define DEFAULT_HOST	"uncnsrd.mt-kisco.ny.us"
#define DEFAULT_PORT	"504"

// TCPsocket represents a socket-level TCP connection to a server.
class TCPsocket {
public:
	TCPsocket::TCPsocket(void);
	int attach(const char *, const char *);
	void detach(void);
	void serv_read(char *, int);
	void serv_write(char *, int);
	void serv_gets(char *);
	void serv_puts(const char *);
	bool is_connected(void);
private:
	int serv_sock;
	int connectsock(const char *, const char *, const char *);
	static void timeout(int);
};



// CitClient represents an application-level connection to a Citadel server.
class CitClient {
	friend TCPsocket::timeout(int);
public:
	CitClient(void);
	~CitClient(void);

	// Citadel session-layer commands
	int attach(wxString host, wxString port);
	void detach(void);
	bool IsConnected(void);
	int CitClient::serv_trans(
			wxString& command,
			wxString& response,
			wxString& xferbuf,
			wxString desired_room
                        );
	int CitClient::serv_trans(
			wxString& command,
			wxString& response,
			wxString& xferbuf
                        );
	int CitClient::serv_trans(wxString& command, wxString& response);
	int CitClient::serv_trans(wxString& command);

	// Citadel presentation-layer commands
	bool CitClient::GotoRoom(wxString roomname, wxString password,
				wxString& server_response);

	// Various things we learn about the server ...
	int SessionID;
	wxString NodeName;
	wxString HumanNode;
	wxString FQDN;
	wxString ServerSoftware;
	int ServerRev;
	wxString GeoLocation;
	wxString SysAdmin;
	int ServerType;
	wxString MorePrompt;
	bool UseFloors;
	int PagingLevel;

	// ... and about the user ...
	wxString curr_user;
	wxString curr_pass;
	
	// Stuff we have to keep track of ...
	wxString CurrentRoom;

private:
	TCPsocket sock;					// transport layer
	void serv_gets(wxString& buf);			// session layer
	void serv_puts(wxString buf);			// session layer
	void download_express_messages(void);		// presentation layer
	void CitClient::initialize_session(void);	// presentation layer
};




// This is a timer that does keepalives...
class keepalive : public wxTimer {
public:
        keepalive(CitClient *sock);
private:
	CitClient *which_sock;
        void Notify(void);
};


// Receive an express message (page)
class express_message : public wxFrame {
public:
	express_message(CitClient *sock, wxString sender,
		wxString sendsys, wxString msg);
private:
	void OnButtonPressed(wxCommandEvent& whichbutton);
	CitClient *citsock;
	wxString reply_to;
	DECLARE_EVENT_TABLE()
};




// SendExpress is the screen for sending an express message (page).

class SendExpress : public wxMDIChildFrame {
public:
	SendExpress(	CitClient *sock,
			wxMDIParentFrame *MyMDI, 
			wxString touser);
private:
	void OnButtonPressed(wxCommandEvent& whichbutton);
	CitClient *citsock;
	wxMDIParentFrame *citMyMDI;
	wxListBox *ToWhom;
	wxTextCtrl *TheMessage;
	DECLARE_EVENT_TABLE()
};


// Preferences for the application.

class Preferences : public wxMDIChildFrame {
public:
	Preferences(	CitClient *sock,
			wxMDIParentFrame *MyMDI);
private:
	void OnButtonPressed(wxCommandEvent& whichbutton);
	CitClient *citsock;
	wxMDIParentFrame *citMyMDI;
	wxTextCtrl *server_host, *server_port;
	wxCheckBox *server_autoconnect;
	DECLARE_EVENT_TABLE()
};



// Just testing...
class TestWindow : public wxMDIChildFrame {
public:
	TestWindow(CitClient *sock, wxMDIParentFrame *MyMDI);
private:
	wxPanel *panel;
	wxTextCtrl *sendcmd;
	wxTextCtrl *recvcmd;
	wxButton *cmd_button;
	wxButton *close_button;
	void OnButtonPressed(wxCommandEvent& whichbutton);
	CitClient *citsock;
	DECLARE_EVENT_TABLE()
};




// userlogin is the frame for logging in.
class UserLogin : public wxMDIChildFrame {
public:
	UserLogin(CitClient *sock, wxMDIParentFrame *MyMDI);
	int do_login();
private:
	wxPanel *panel;
	wxTextCtrl *username;
	wxTextCtrl *password;
	wxButton *login_button;
	wxButton *newuser_button;
	wxButton *exit_button;
	void OnButtonPressed(wxCommandEvent& whichbutton);
	void UserLogin::BeginSession(wxString serv_response);
	CitClient *citsock;
	wxMDIParentFrame *citMyMDI;
	DECLARE_EVENT_TABLE()
};


// Who is online?
class who : public wxMDIChildFrame {
public:
	who(CitClient *sock, wxMDIParentFrame *MyMDI);
	int do_login();
	void LoadWholist();
private:
	void OnButtonPressed(wxCommandEvent& whichbutton);
	CitClient *citsock;
	wxListCtrl *wholist;
	DECLARE_EVENT_TABLE()
};


// This is a timer that periodically refreshes the wholist.
class who_refresh : public wxTimer {
public:
	who_refresh(who *parent_who);
private:
	who *which_who;
	void Notify(void);
};



// The ever-present tree of floors and rooms

class RoomTree : public wxTreeCtrl {
public:
	RoomTree(wxMDIParentFrame *parent, CitClient *sock);
	void LoadRoomList(void);
	wxString GetNextRoom(void);
private:
	wxTreeItemId GetNextRoomId(void);
	void InitTreeIcons(void);
	void OnDoubleClick(wxTreeEvent& evt);
	CitClient *citsock;
	wxMDIParentFrame *citMyMDI;
	wxTreeItemId floorboards[MAXFLOORS];
	wxImageList *TreeIcons;
	wxTreeItemId march_next;
	DECLARE_EVENT_TABLE()
};



// A window representing an open room.
class RoomView : public wxMDIChildFrame {
public:
	RoomView(CitClient *sock, wxMDIParentFrame *MyMDI, wxString roomname);
private:
	void OnButtonPressed(wxCommandEvent& whichbutton);
	CitClient *citsock;
	void do_readloop(wxString readcmd);
	wxMDIParentFrame *citMyMDI;
	wxHtmlWindow *message_window;
        wxPanel *banner;
        wxButton *close_button;
	wxString ThisRoom;
	DECLARE_EVENT_TABLE()
};


class EnterMessage : public wxMDIChildFrame {
public:
	EnterMessage(CitClient *sock, wxMDIParentFrame *MyMDI,
		wxString roomname);
private:
	void OnCancel(wxCommandEvent& whichbutton);
	void OnSave(wxCommandEvent& whichbutton);
	CitClient *citsock;
	wxMDIParentFrame *citMyMDI;
	wxString ThisRoom;
	wxChoice *fromname;
	wxTextCtrl *TheMessage;
	DECLARE_EVENT_TABLE()
};





	
// A message.  (This is not a GUI class; it's used for internal
// representation.)
class CitMessage {
public:
	CitMessage(CitClient *sock, wxString getmsg_cmd, wxString inRoom);
	wxString author;
	wxString recipient;
	long timestamp;
	wxString room;
	wxString msgtext;
	wxString nodename;
private:
	int format_type;
};





// Stuff from utils.cpp

void extract(wxString& outputbuf, wxString inputbuf, int parmnum);
int extract_int(wxString inputbuf, int parmnum);
void load_roomlist(RoomTree *tree, CitClient *citsock);
void variformat_to_html(wxString& outputbuf,
                        wxString inputbuf,
                        bool add_header_and_footer);
void cleanup(int);



// Globals

extern wxMDIParentFrame *BigMDI;
extern RoomTree *RoomList;
extern CitClient *citadel;
extern wxConfig *ini;
