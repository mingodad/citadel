#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <wx/wx.h>
#include <wx/listctrl.h>
#include <wx/socket.h>
#include "utils.h"



// Globals

extern wxMDIParentFrame *BigMDI;





// TCPsocket represents a socket-level TCP connection to a server.
class TCPsocket {
public:
	TCPsocket::TCPsocket(void);
	int attach(char *, char *);
	void detach(void);
	void serv_read(char *, int);
	void serv_write(char *, int);
	void serv_gets(char *);
	void serv_puts(char *);
	bool is_connected(void);
private:
	int serv_sock;
	int connectsock(const char *, const char *, const char *);
	void timeout(int);
};



// CitClient represents an application-level connection to a Citadel server.
class CitClient {
public:
	CitClient(void);
	~CitClient(void);

	// High-level Citadel IPC methods
	int attach(const wxString& host, const wxString& port);
	void detach(void);
	bool IsConnected(void);
	int CitClient::serv_trans(
			wxString& command,
			wxString& response,
			wxStringList& xferbuf
                        );
	int CitClient::serv_trans(wxString& command, wxString& response);
	int CitClient::serv_trans(wxString& command);

	// Various things we learn about the server
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

private:
	void serv_gets(wxString& buf);
	void serv_puts(wxString buf);
	void download_express_messages(void);
	TCPsocket sock;
	void CitClient::initialize_session(void);
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
		wxString sendsys, wxStringList msg);
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

