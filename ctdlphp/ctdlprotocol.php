<?PHP

// $Id$
// 
// Implements various Citadel server commands.
//
// Copyright (c) 2003 by Art Cancro <ajc@uncensored.citadel.org>
// This program is released under the terms of the GNU General Public License.
//


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
}



//
// Identify this client, and the hostname where the user is, to Citadel.
//
function ctdl_iden() {
	global $clientsocket;

	serv_puts("IDEN 0|8|001|PHP web client|" . $_SERVER['REMOTE_ADDR'] );
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



?>
