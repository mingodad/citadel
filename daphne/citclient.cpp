#include "includes.hpp"
#include <wx/protocol/protocol.h>

//
//	TRANSPORT LAYER OPERATIONS
//



// Attach to the Citadel server
// FIX (add check for not allowed to log in)
int CitClient::attach(wxString host, wxString port) {
	wxString ServerReady;
	wxIPV4address addr;

        if (sock->IsConnected())
        sock->Close();

        addr.Hostname(host);
        addr.Service(port);
        sock->SetNotify(0);
        sock->Connect(addr, TRUE);
        if (sock->IsConnected()) {
                cout << "Connect succeeded\n" ;
                serv_gets(ServerReady);
                initialize_session();
		curr_host = host;	// Remember host and port, in case
		curr_port = port;	// we need to auto-reconnect later
                return(0);
        } else {
                cout << "Connect failed\n" ;
                return(1);
        }
}


// constructor
CitClient::CitClient(void) {

        //wxSocketHandler::Master();
        sock = new wxSocketClient();

	// The WAITALL flag causes reads to block.  Don't use it.
        // sock->SetFlags(wxSocketBase::WAITALL);

	// Guilhem Lavaux suggested using the SPEED flag to keep from
	// blocking, but it just freezes the app in mid-transaction.
	sock->SetFlags(wxSocketBase::SPEED);

        //wxSocketHandler::Master().Register(sock);
        // sock->SetNotify(wxSocketBase::REQ_LOST);

	(void)new keepalive(this);
}


// destructor
CitClient::~CitClient(void) {
	// Be nice and log out from the server if it's still connected
	sock->Close();
}



void CitClient::detach(void) {
        wxString buf;

        if (sock->IsConnected()) {
                serv_puts("QUIT");
                serv_gets(buf);
                sock->Close();
        }
}



// Is this client connected?  Simply return the IsConnected status of sock.
bool CitClient::IsConnected(void) {
        return sock->IsConnected();
}





// Read a line of text from the server
void CitClient::serv_gets(wxString& buf) {
	static char charbuf[512];
	static size_t nbytes = 0;
	int i;
	int nl_pos = (-1);

	do {
		for (i=nbytes; i>=0; --i)
			if (charbuf[i] == 10) nl_pos = i;
		if (nl_pos < 0) {
			sock->Read(&charbuf[nbytes], (sizeof(charbuf)-nbytes) );
			nbytes += sock->LastCount();
			cout << "Read " << sock->LastCount() << " bytes \n";
		}
		for (i=nbytes; i>=0; --i)
			if (charbuf[i] == 10) nl_pos = i;
	} while (nl_pos < 0);

        //if (nl_pos != nbytes)
        //  sock->Unread(&charbuf[nl_pos], nbytes-nl_pos);

	charbuf[nbytes] = 0;
	charbuf[nl_pos] = 0;

	buf = charbuf;
	strcpy(charbuf, &charbuf[nl_pos + 1]);
	nbytes = nbytes - (nl_pos + 1);

/*
        GetLine(sock, buf);
*/

	cout << "> " << buf << "(len=" << buf.Len() << ")\n";
}






// Write a line of text to the server
void CitClient::serv_puts(wxString buf) {

        cout << "< " << buf << "\n" ;
        sock->Write((const char *)buf, buf.Len());
        sock->Write("\n", 1);
}









//
//		SESSION LAYER OPERATIONS
//


// Server transaction (returns first digit of server response code)
int CitClient::serv_trans(
			wxString& command,
			wxString& response,
			wxString& xferbuf,
			wxString desired_room
			) {

        int first_digit, i, pos;
	wxString buf, pw, junk;
	bool express_messages_waiting = FALSE;

	// If the caller specified that this transaction must take place
	// in a particular room, make sure we're in that room.
	if (desired_room.Length() > 0) {
		if (desired_room.CmpNoCase(CurrentRoom) != 0) {
			pw = "";
			GotoRoom(desired_room, pw, junk);
		}
	}


	// If a mutex is to be wrapped around this function in the future,
	// it must begin HERE.
	cout << "Beginning transaction\n";
	Critter.Enter();
	wxBeginBusyCursor();

	serv_puts(command);

	if (IsConnected() == FALSE) {
		wxSleep(5);	// Give a crashed server some time to restart
		reconnect_session();
		serv_puts(command);
	}

	serv_gets(response);

	first_digit = (response.GetChar(0)) - '0';

	if (response.GetChar(3) == '*')
		express_messages_waiting = TRUE;

	if (first_digit == 1) {			// LISTING_FOLLOWS
		xferbuf.Empty();
		while (serv_gets(buf), buf != "000") {
			xferbuf.Append(buf + "\n");
		}
	} else if (first_digit == 4) {		// SEND_LISTING
		buf = xferbuf;
		while (buf.Length() > 0) {
			pos = buf.Find('\n', FALSE);
			if ((pos < 0) && (buf.Length() < 250)) {
				serv_puts(buf + "\n");
				buf.Empty();
			} else if ((pos < 250) && (pos >= 0)) {
				serv_puts(buf.Left(pos+1));
				buf = buf.Mid(pos+1);
			} else {
				pos = buf.Left(250).Find(' ', TRUE);
				if ((pos < 250) && (pos >= 0)) {
					serv_puts(buf.Left(pos) + "\n");
					buf = buf.Mid(pos+1);
				} else {
					serv_puts(buf.Left(250) + "\n");
					buf = buf.Mid(pos);
				}
			}
		}
		serv_puts("000");
	}

	// If a mutex is to be wrapped around this function in the future,
	// it must end HERE.
	cout << "Ending transaction...\n";
	wxEndBusyCursor();
	Critter.Leave();
	cout << "...done.\n";

	if (express_messages_waiting) {
		download_express_messages();
	}

	cout << "serv_trans() returning " << first_digit << "\n";
	return first_digit;
}

// Shorter forms of serv_trans()
int CitClient::serv_trans(
			wxString& command,
			wxString& response,
			wxString& xferbuf
			) {
	return serv_trans(command, response, xferbuf, "");
}

int CitClient::serv_trans(wxString& command, wxString& response) {
	wxString junklist;
	return serv_trans(command, response, junklist);
}

int CitClient::serv_trans(wxString& command) {
	wxString junkbuf;
	return serv_trans(command, junkbuf);
}

//
//		PRESENTATION LAYER OPERATIONS
//


void CitClient::download_express_messages(void) {
	wxString sendcmd, recvcmd, x_user, x_sys;
	wxString xferbuf;

	sendcmd = "GEXP";
	while (serv_trans(sendcmd, recvcmd, xferbuf) == 1) {
		extract(x_user, recvcmd, 3);
		extract(x_sys, recvcmd, 4);
		(void)new express_message(this, x_user, x_sys, xferbuf);
	}
}



// Set up some things that we do at the beginning of every session
void CitClient::initialize_session(void)  {
	wxString info;
	wxString sendcmd;
	wxString recvcmd;
	int i, pos;
	wxString *infoptr;
	wxString infoline;

	CurrentRoom = "";

	sendcmd = "IDEN 0|6|001|Daphne";
	serv_trans(sendcmd);

	sendcmd = "INFO";
	if (serv_trans(sendcmd, recvcmd, info)==1) {
		i = 0;
		while (pos = info.Find('\n', FALSE),  (pos >= 0) ) {
			infoline = info.Left(pos);
			info = info.Mid(pos+1);
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

			++i;
			}
		}
	}
}



// Goto a room

bool CitClient::GotoRoom(wxString roomname, wxString password,
			wxString& server_response) {
	int retval;
	wxString sendcmd, recvcmd;

	sendcmd = "GOTO " + roomname + "|" + password;
	retval = serv_trans(sendcmd, recvcmd);
	server_response = recvcmd;

	if (retval != 2) return FALSE;

	extract(CurrentRoom, recvcmd.Mid(4), 0);
	BigMDI->SetStatusText(CurrentRoom, 2);
	return TRUE;
}



// Reconnect a broken session

void CitClient::reconnect_session(void) {
	wxString sendcmd;

	CurrentRoom = "__ This is not the name of any valid room __";

	if (attach(curr_host, curr_port) != 0) {
		// FIX do this more elegantly
		cout << "Could not re-establish session (1)\n";
	}

	sendcmd = "USER " + curr_user;
	if (serv_trans(sendcmd) != 3) {
		// FIX do this more elegantly
		cout << "Could not re-establish session (2)\n";
	}

	sendcmd = "PASS " + curr_pass;
	if (serv_trans(sendcmd) != 2) {
		// FIX do this more elegantly
		cout << "Could not re-establish session (3)\n";
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
		wxString noop = "NOOP";
		which_sock->serv_trans(noop);
	}
}
