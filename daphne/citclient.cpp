#include <ctype.h>
#include <string.h>

#include <wx/wx.h>

#include "citclient.hpp"


//  
//              LOW LEVEL OPERATIONS
//

// Attach to the Citadel server
// FIX (add check for not allowed to log in)
int CitClient::attach(const wxString& host, const wxString& port) {
	wxString ServerReady;

	//if (sock.IsConnected())
	//	sock.Close();
	//addr.Hostname(host);
	//addr.Service(port);
	//sock.SetNotify(0);
	//sock.Connect(addr, TRUE);
	//if (sock.IsConnected()) {
	//	serv_gets(ServerReady);
	//	initialize_session();
	//	return(0);
	//}
	//else return(1);

	if (sock.is_connected())
		sock.detach();
	if (sock.attach(host, port)==0) {
		serv_gets(ServerReady);
		initialize_session();
		return(0);
	}
	else return(1);

}



// constructor
CitClient::CitClient(void) {
  //wxSocketHandler::Master();
  //sock.SetFlags(wxSocketBase::WAITALL);
  //wxSocketHandler::Master().Register(&sock);
  //sock.SetNotify(wxSocketBase::REQ_LOST);
}



// destructor
CitClient::~CitClient(void) {

	// Be nice and log out from the server if it's still connected
	sock.detach();
}

void CitClient::detach(void) {
	wxString buf;

	//if (sock.IsConnected()) {
	if (sock.is_connected()) {
		serv_puts("QUIT");
		serv_gets(buf);
		//sock.Close();
		sock.detach();
	}
}


// Is this client connected?  Simply return the IsConnected status of sock.
bool CitClient::IsConnected(void) {
	//return sock.IsConnected();
	return sock.is_connected();
}



// Read a line of text from the server
void CitClient::serv_gets(wxString& buf) {
	char charbuf[256];
	
	sock.serv_gets(charbuf);
	buf = charbuf;

	//buf.Empty();
	//do {
	//	while (sock.IsData()==FALSE) ;;
	//	sock.Read(charbuf, 1);
	//	if (isprint(charbuf[0])) buf.Append(charbuf[0], 1);
	//} while(isprint(charbuf[0]));
	printf("<%s\n", (const char *)buf);
}


// Write a line of text to the server
void CitClient::serv_puts(wxString buf) {
	printf(">%s\n", (const char *)buf);
	//sock.Write(buf, strlen(buf));
	//sock.Write("\n", 1);
	sock.serv_puts(buf);
}


//
//            HIGH LEVEL COMMANDS
//


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
	// if (response.Length()==0) serv_gets(response);
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

// Set up some things that we do at the beginning of every session
void CitClient::initialize_session(void)  {
	wxStringList info;
	wxString sendcmd;
	wxString recvcmd;
	int i;
	wxString *infoptr;
	wxString infoline;

	sendcmd = "IDEN 0|6|000|Daphne";
	serv_trans(sendcmd);

	sendcmd = "INFO";
	if (serv_trans(sendcmd, recvcmd, info)==1) {
		for (i=0; i<info.Number(); ++i) {
			infoptr = (wxString *) info.Nth(i)->GetData();
			infoline.Printf("%s", infoptr);
			switch(i) {
			case 0:		SessionID	= atoi(infoline);
			case 1:		NodeName	= infoline;
			case 2:		HumanNode	= infoline;
			case 3:		FQDN		= infoline;
			case 4:		ServerSoftware	= infoline;
			case 5:		ServerRev	= atoi(infoline);
			case 6:		GeoLocation	= infoline;
			case 7:		SysAdmin	= infoline;
			case 8:		ServerType	= atoi(infoline);
			case 9:		MorePrompt	= infoline;
			case 10:	UseFloors	= ((atoi(infoline)>0)
							? TRUE : FALSE);
			case 11:	PagingLevel	= atoi(infoline);
			}
		}
	}
}
