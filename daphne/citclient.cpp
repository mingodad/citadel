
#include "includes.hpp"


//  
//              LOW LEVEL OPERATIONS
//

// Attach to the Citadel server
// FIX (add check for not allowed to log in)
int CitClient::attach(const wxString& host, const wxString& port) {
	wxString ServerReady;

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
	(void)new keepalive(this);
}


// destructor
CitClient::~CitClient(void) {
	// Be nice and log out from the server if it's still connected
	sock.detach();
}

void CitClient::detach(void) {
	wxString buf;

	if (sock.is_connected()) {
		serv_puts("QUIT");
		serv_gets(buf);
		sock.detach();
	}
}


// Is this client connected?  Simply return the IsConnected status of sock.
bool CitClient::IsConnected(void) {
	return sock.is_connected();
}



// Read a line of text from the server
void CitClient::serv_gets(wxString& buf) {
	char charbuf[256];
	
	sock.serv_gets(charbuf);
	buf = charbuf;
}


// Write a line of text to the server
void CitClient::serv_puts(wxString buf) {
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
	bool express_messages_waiting = FALSE;

	serv_puts(command);
	serv_gets(response);
	first_digit = (response.GetChar(0)) - '0';

	if (response.GetChar(3) == '*')
		express_messages_waiting = TRUE;

	if (first_digit == 1) {			// LISTING_FOLLOWS
		xferbuf.Clear();
		while (serv_gets(buf), buf != "000") {
			xferbuf.Add(buf);
		}
	} else if (first_digit == 4) {		// SEND_LISTING
		// FIX do this!!
		serv_puts("000");
	}

	if (express_messages_waiting) {
		download_express_messages();
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

	sendcmd = "IDEN 0|6|001|Daphne";
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



void CitClient::download_express_messages(void) {
	wxString sendcmd, recvcmd, x_user, x_sys;
	wxStringList xferbuf;

	sendcmd = "GEXP";
	while (serv_trans(sendcmd, recvcmd, xferbuf) == 1) {
		extract(x_user, recvcmd, 3);
		extract(x_sys, recvcmd, 4);
		(void)new express_message(this, x_user, x_sys, xferbuf);
	}
}



// This is a simple timer that periodically wakes up and sends a NOOP to the
// server.  This accomplishes two things: it keeps the server connection
// alive by trickling some data through when it's otherwise idle, and it allows
// the "check for express messages" loop to activate if it has to.

keepalive::keepalive(CitClient *sock)
	: wxTimer() {

	which_sock = sock;		// Know which instance to refresh
	Start(15000, FALSE);		// Call every 15 seconds
}


void keepalive::Notify(void) {
	if (which_sock->IsConnected()) {
		which_sock->serv_trans("NOOP");
	}
}






