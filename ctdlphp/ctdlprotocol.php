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

	return array(TRUE, "Login successful.  Have fun.");
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
			if ($i == 2) $_SESSION["serv_humannode"] = $buf;
			if ($i == 4) $_SESSION["serv_software"] = $buf;
			$i = $i + 1;
		} while (strcasecmp($buf, "000"));
	}

}



function test_for_echo() {

	global $clientsocket, $session;

	$command = "ECHO Video vertigo ... test for echo.";

	serv_puts($command);
	$response = serv_gets();
	echo $response, "<BR>";
	flush();
}

function ctdl_mesg($msgname) {
	global $clientsocket;

	serv_puts("MESG " . $msgname);
	$response = serv_gets();
	
	if (substr($response, 0, 1) == "1") {
		echo "<DIV ALIGN=CENTER>\n";
		do {
			$buf = serv_gets();
			if (strcasecmp($buf, "000")) {
				echo "<TT>", $buf, "</TT><BR>\n" ;
			}
		} while (strcasecmp($buf, "000"));
		echo "</DIV>\n";
	}
	else {
		echo "<B><I>", substr($response, 4), "</I></B><BR>\n";
	}
}

?>
