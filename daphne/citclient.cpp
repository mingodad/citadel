#include <ctype.h>
#include <string.h>

#include <wx/wx.h>

#include "citclient.hpp"


// methods

// Attach to the Citadel server
int CitClient::attach(const wxString& host, const wxString& port) {
	wxString ServerReady;

	if (sock.IsConnected())
		sock.Close();

	addr.Hostname(host);
	addr.Service(port);
	sock.SetNotify(0);
	sock.Connect(addr, TRUE);
	if (sock.IsConnected()) {
		serv_gets(ServerReady);
		// FIX ... add check for not allowed to log in
		return(0);
	}
	else return(1);
}



// constructor
CitClient::CitClient(void) {
  wxSocketHandler::Master();
  sock.SetFlags(wxSocketBase::WAITALL);
  wxSocketHandler::Master().Register(&sock);
  sock.SetNotify(wxSocketBase::REQ_LOST);
}



// destructor
CitClient::~CitClient(void) {

	// Be nice and log out from the server if it's still connected
	detach();
}

void CitClient::detach(void) {
	wxString buf;

	if (sock.IsConnected()) {
		serv_puts("QUIT");
		serv_gets(buf);
		sock.Close();
	}
}


// Is this client connected?  Simply return the IsConnected status of sock.
bool CitClient::IsConnected(void) {
	return sock.IsConnected();
}



// Read a line of text from the server
void CitClient::serv_gets(wxString& buf) {
	char charbuf[2];

	do {
		sock.Read(charbuf, 1);
		if (isprint(charbuf[0])) buf.Append(charbuf[0], 1);
	} while(isprint(charbuf[0]));
	printf("<%s\n", (const char *)buf);
}


// Write a line of text to the server
void CitClient::serv_puts(wxString buf) {
	printf(">%s\n", (const char *)buf);
	sock.Write(buf, strlen(buf));
	sock.Write("\n", 1);
}

// Server transaction (returns first digit of server response code)
int CitClient::serv_trans(
			wxString& command,
			wxString& response,
			wxStringList& xferbuf
			) {

	int first_digit;
	wxString buf;

	serv_puts(command);
	serv_gets(response);
	first_digit = (response.GetChar(0)) - '0';

	if (first_digit == 1) {			// LISTING_FOLLOWS
		xferbuf.Clear();
		while (serv_gets(buf), buf != "000") {
			xferbuf.Add(buf);
		}
	} else if (first_digit == 4) {		// SEND_LISTING
		// FIX do this!!
		serv_puts("000");
	}



	return first_digit;
}

// Shorter forms of serv_trans()
int CitClient::serv_trans(wxString& command, wxString& response) {
	wxStringList junklist;
	return serv_trans(command, response, junklist);
}

int CitClient::serv_trans(wxString& command) {
	wxString junkbuf;
	return serv_trans(command, junkbuf);
}
