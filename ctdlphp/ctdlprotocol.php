<?PHP

// $Id$
// 
// Implements various Citadel server commands.
//
// Copyright (c) 2003 by Art Cancro <ajc@uncensored.citadel.org>
// This program is released under the terms of the GNU General Public License.


//
// serv_gets() -- generic function to read one line of text from the server
//
function serv_gets() {
	global $clientsocket;

	$buf = fgets($clientsocket, 4096);		// Read line
	$buf = substr($buf, 0, (strlen($buf)-1) );	// strip trailing LF
	return $buf;
}


//
// serv_puts() -- generic function to write one line of text to the server
//
function serv_puts($buf) {
	global $clientsocket;
	
	fwrite($clientsocket, $buf . "\n", (strlen($buf)+1) );
	fflush($clientsocket);
}


// 
// text_to_server() -- sends a block of text to the server.  Assumes that
//                     the server just sent back a SEND_LISTING response code
//                     and is now expecting a 000-terminated series of lines.
//                     Set 'convert_to_html' to TRUE to convert the block of
//                     text to HTML along the way.
//
function text_to_server($thetext, $convert_to_html) {

	// HTML mode
	if ($convert_to_html) {

		// Strip CR's; we only want the LF's
		$thetext = trim($thetext, "\r");

		// Replace hard line breaks with <BR>'s
		$thetext = str_replace("\n", "<BR>\n", $thetext);

	}

	// Either mode ... send it to the server now
	$this_line = strtok($thetext, "\n");
	while ($this_line !== FALSE) {
		$this_line = trim($this_line, "\n\r");
		if ($this_line == "000") $this_line = "-000" ;
		serv_puts($this_line);
		$this_line = strtok("\n");
	}

	serv_puts("000");	// Tell the server we're done...

	serv_puts("ECHO echo test.");		// FIXME
	echo "Echo test: " . serv_gets() . "<BR>\n" ;

}



//
// Identify ourselves to the Citadel server (do this once after connection)
//
function ctdl_iden() {
	global $clientsocket;

	// Identify client and hostname
	serv_puts("IDEN 0|8|001|PHP web client|" . $_SERVER['REMOTE_ADDR'] );
	$buf = serv_gets();

	// Also express our message format preferences
	serv_puts("MSGP text/html|text/plain");
	$buf = serv_gets();
}



//
// login_existing_user() -- attempt to login using a supplied username/password
// Returns an array with two variables:
// 0. TRUE or FALSE to determine success or failure
// 1. String error message (if relevant)
//
function login_existing_user($user, $pass) {
	global $clientsocket;

	serv_puts("USER " . $user);
	$resp = serv_gets();
	if (substr($resp, 0, 1) != "3") {
		return array(FALSE, substr($resp, 4));
	}

	serv_puts("PASS " . $pass);
	$resp = serv_gets();
	if (substr($resp, 0, 1) != "2") {
		return array(FALSE, substr($resp, 4));
	}

	$_SESSION["username"] = $user;
	$_SESSION["password"] = $pass;
	become_logged_in(substr($resp, 4));

	return array(TRUE, "Login successful.  Have fun.");
}


//
// create_new_user() -- attempt to create a new user 
//                      using a supplied username/password
// Returns an array with two variables:
// 0. TRUE or FALSE to determine success or failure
// 1. String error message (if relevant)
//
function create_new_user($user, $pass) {
	global $clientsocket;

	serv_puts("NEWU " . $user);
	$resp = serv_gets();
	if (substr($resp, 0, 1) != "2") {
		return array(FALSE, substr($resp, 4));
	}

	serv_puts("SETP " . $pass);
	$resp = serv_gets();
	if (substr($resp, 0, 1) != "2") {
		return array(FALSE, substr($resp, 4));
	}

	$_SESSION["username"] = $user;
	$_SESSION["password"] = $pass;
	become_logged_in(substr($resp, 4));

	return array(TRUE, "Login successful.  Have fun.");
}


//
// Code common to both existing-user and new-user logins
//
function become_logged_in($server_parms) {
	$_SESSION["logged_in"] = 1;

	$tok = strtok($server_parms, "|");
	if ($tok) $thisline["username"] = $tok;

	$tok = strtok("|");
	if ($tok) $thisline["axlevel"] = $tok;
		
	$tok = strtok("|");
	if ($tok) $thisline["calls"] = $tok;
		
	$tok = strtok("|");
	if ($tok) $thisline["posts"] = $tok;
		
	$tok = strtok("|");
	if ($tok) $thisline["userflags"] = $tok;
		
	$tok = strtok("|");
	if ($tok) $thisline["usernum"] = $tok;
		
	$tok = strtok("|");
	if ($tok) $thisline["lastcall"] = $tok;
		
	ctdl_goto("_BASEROOM_");
}



//
// Learn all sorts of interesting things about the Citadel server to
// which we are connected.
//
function ctdl_get_serv_info() {
	serv_puts("INFO");
	$buf = serv_gets();
	if (substr($buf, 0, 1) == "1") {
		$i = 0;
		do {
			$buf = serv_gets();
			if ($i == 1) $_SESSION["serv_nodename"] = $buf;
			if ($i == 2) $_SESSION["serv_humannode"] = $buf;
			if ($i == 3) $_SESSION["serv_fqdn"] = $buf;
			if ($i == 4) $_SESSION["serv_software"] = $buf;
			if ($i == 6) $_SESSION["serv_city"] = $buf;
			if ($i == 7) $_SESSION["serv_sysadmin"] = $buf;
			$i = $i + 1;
		} while (strcasecmp($buf, "000"));
	}

}


//
// Display a system banner.  (Returns completed HTML.)
// (This is probably temporary because it outputs more or less finalized
// markup.  For now it's just usable.)
//
function ctdl_mesg($msgname) {
	global $clientsocket;

	$msgtext = "<DIV ALIGN=CENTER>\n";

	serv_puts("MESG " . $msgname);
	$response = serv_gets();

	if (substr($response, 0, 1) == "1") {
		while (strcmp($buf = serv_gets(), "000")) {
			$msgtext .= "<TT>" . htmlspecialchars($buf)
				. "</TT><BR>\n" ;
		}
	}
	else {
		$msgtext .= "<B><I>" . substr($response, 4) . "</I></B><BR>\n";
	}

	$msgtext .= "</DIV>\n";
	return($msgtext);
}


//
// Fetch the list of users currently logged in.
//
function ctdl_rwho() {
	global $clientsocket;

	serv_puts("RWHO");
	$response = serv_gets();

	if (substr($response, 0, 1) != "1") {
		return array(0, NULL);
	}
	
	$all_lines = array();
	$num_lines = 0;

	while (strcmp($buf = serv_gets(), "000")) {

		$thisline = array();

		$tok = strtok($buf, "|");
		if ($tok) $thisline["session"] = $tok;

		$tok = strtok("|");
		if ($tok) $thisline["user"] = $tok;
		
		$tok = strtok("|");
		if ($tok) $thisline["room"] = $tok;
		
		$tok = strtok("|");
		if ($tok) $thisline["host"] = $tok;
		
		$tok = strtok("|");
		if ($tok) $thisline["client"] = $tok;

		// IGnore the rest of the fields for now.

		$num_lines = array_push($all_lines, $thisline);
	}

	return array($num_lines, $all_lines);

}


//
// Goto a room.
//
function ctdl_goto($to_where) {
	
	serv_puts("GOTO " . $to_where);
	$response = serv_gets();

	if (substr($response, 0, 1) == "2") {
		$_SESSION["room"] = strtok(substr($response, 4), "|");
		return array(TRUE, substr($response, 0, 3));
	}

	else {
		return array(FALSE, substr($response, 0, 3));
	}

}



//
// Fetch the list of known rooms.
//
function ctdl_knrooms() {
	global $clientsocket;

	serv_puts("LKRA");
	$response = serv_gets();

	if (substr($response, 0, 1) != "1") {
		return array(0, NULL);
	}
	
	$all_lines = array();
	$num_lines = 0;

	while (strcmp($buf = serv_gets(), "000")) {

		$thisline = array();

		$tok = strtok($buf, "|");
		if ($tok) $thisline["name"] = $tok;

		$tok = strtok("|");
		if ($tok) $thisline["flags"] = $tok;
		
		$tok = strtok("|");
		if ($tok) $thisline["floor"] = $tok;
		
		$tok = strtok("|");
		if ($tok) $thisline["order"] = $tok;
		
		$tok = strtok("|");
		if ($tok) $thisline["flags2"] = $tok;

		$tok = strtok("|");
		if ($tok) $thisline["access"] = $tok;

		if ($thisline["access"] & 8) {
			$thisline["hasnewmsgs"] = TRUE;
		}
		else {
			$thisline["hasnewmsgs"] = FALSE;
		}

		$num_lines = array_push($all_lines, $thisline);
	}

	return array($num_lines, $all_lines);

}


//
// Fetch the list of messages in this room.
// Returns: count, response, message array
//
function ctdl_msgs($mode, $count) {
	global $clientsocket;

	serv_puts("MSGS " . $mode . "|" . $count);
	$response = serv_gets();

	if (substr($response, 0, 1) != "1") {
		return array(0, substr($response, 4), NULL);
	}
	
	$msgs = array();
	$num_msgs = 0;

	while (strcmp($buf = serv_gets(), "000")) {
		$num_msgs = array_push($msgs, $buf);
	}

	return array($num_msgs, substr($response, 4), $msgs);
}


// Load a message from the server.
function ctdl_fetch_message($msgnum) {
	global $clientsocket;

	serv_puts("MSG4 " . $msgnum);
	$response = serv_gets();

	if (substr($response, 0, 1) != "1") {
		return array(FALSE, substr($response, 4), NULL);
	}

	$fields = array();
	while (strcmp($buf = serv_gets(), "000")) {
		if (substr($buf, 0, 4) == "text") {
			// We're in the text body.  New loop here.
			$fields["text"] = ctdl_msg4_from_server();
			return array(TRUE, substr($response, 4), $fields);
		}
		else {
			$fields[substr($buf, 0, 4)] = substr($buf, 5);
		}
	}

	// Message terminated prematurely (no text body)
	return array(FALSE, substr($response, 4), $fields);
}

// Support function for ctdl_fetch_message().  This handles the text body
// portion of the message, converting various formats to HTML as
// appropriate.
function ctdl_msg4_from_server() {

	$txt = "";
	$msgformat = "text/plain";
	$in_body = FALSE;

	$previous_line = "";
	while (strcmp($buf = serv_gets(), "000")) {
		if ($in_body == FALSE) {
			if (strlen($buf) == 0) {
				$in_body = TRUE;
			}
			else {
				if (!strncasecmp($buf, "content-type: ", 14)) {
					$msgformat = substr($buf, 14);
				}
			}
		}
		else {
			if (!strcasecmp($msgformat, "text/html")) {
				$txt .= $buf;
			}
			else if (!strcasecmp($msgformat, "text/plain")) {
				$txt .= "<TT>" . htmlspecialchars($buf) . "</TT><BR>\n" ;
			}
			else if (!strcasecmp($msgformat, "text/x-citadel-variformat")) {
				if (substr($previous_line, 0, 1) == " ") {
					$txt .= "<BR>\n" ;
				}
				$txt .= htmlspecialchars($buf);
			}
			else {
				$txt .= htmlspecialchars($buf);
			}
			$previous_line = $buf;
		}
	}

	return($txt);
}


?>
