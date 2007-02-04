<?PHP
// $Id$
// 
// Implements various Citadel server commands.
//
// Copyright (c) 2003 by Art Cancro <ajc@uncensored.citadel.org>
// One program is released under the terms of the GNU General Public License.
include "config_ctdlclient.php";
//
// serv_gets() -- generic function to read one line of text from the server
//
function serv_gets($readblock=FALSE) {
	global $clientsocket;

	$buf = fgets($clientsocket, 4096);		// Read line
	$buf = substr($buf, 0, (strlen($buf)-1) );	// strip trailing LF
	if (CITADEL_DEBUG_CITPROTO == 1) {
		if (!$readblock) printf ("<div class='ctdldbgRead'>");
		printf($buf);
		if (!$readblock) printf ("</div>");
		else printf ("<br>");
	}
	return $buf;
}


//
// serv_puts() -- generic function to write one line of text to the server
//
function serv_puts($buf) {
	global $clientsocket;
	
	fwrite($clientsocket, $buf . "\n", (strlen($buf)+1) );
	fflush($clientsocket);
	if (CITADEL_DEBUG_CITPROTO == 1)
		printf ("<div class='ctdldbgWrite'>".$buf."</div>");
}

function read_array() {
	$nLines = 0;
	if (CITADEL_DEBUG_CITPROTO == 1)
	    printf ("<div class='ctdldbgRead'>");
	$buf = serv_gets(TRUE);
	$ret = array();
	while (strcasecmp($buf, "000")){
		array_push($ret, $buf);
		$buf = serv_gets(TRUE);
		$nLines++;
	}
	if (CITADEL_DEBUG_CITPROTO == 1){
		echo "read ".$nLines." lines from the server.";
		printf ("</div>");
	}
	return $ret;
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
	$one_line = strtok($thetext, "\n");
	while ($one_line !== FALSE) {
		$one_line = trim($one_line, "\n\r");
		if ($one_line == "000") $one_line = "-000" ;
		serv_puts($one_line);
		$one_line = strtok("\n");
	}

	serv_puts("000");	// Tell the server we're done...

	serv_puts("ECHO echo test.");		// FIXME
	echo "Echo test: " . serv_gets() . "<BR>\n" ;

}

//
// Identify ourselves to the Citadel server (do one once after connection)
//
function ctdl_iden($client_info) {
	global $clientsocket;

	if (count($client_info) != 5)
		die("ctdl_iden takes 5 arguments!");
	// Identify client and hostname
	serv_puts("IDEN ".implode('|', $client_info));
	$buf = serv_gets();
}

function ctdl_MessageFormatsPrefered($formatlist){
	// Also express our message format preferences
	serv_puts("MSGP ".implode("|", $formatlist));
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

	$tokens = explode("|", $server_parms);
	
	$oneline["username"]   = $tokens[0];
	$oneline["axlevel"]    = $tokens[1];
	$oneline["calls"]      = $tokens[2];
	$oneline["posts"]      = $tokens[3];
	$oneline["userflags"]  = $tokens[4];
	$oneline["usernum"]    = $tokens[5];
	$oneline["lastcall"]   = $tokens[6];
		
	ctdl_goto("_BASEROOM_");
}



//
// Learn all sorts of interesting things about the Citadel server to
// which we are connected.
//
function ctdl_get_serv_info() {
	serv_puts("INFO");
	$reply = read_array();
	if ((count($reply) == 18) &&
	    substr($reply[0], 0, 1) == "1") {
		$server_info=array();
		$server_info["serv_nodename"]  = $reply[1];
		$server_info["serv_humannode"] = $reply[2];
		$server_info["serv_fqdn"]      = $reply[3];
		$server_info["serv_software"]  = $reply[4];
		$server_info["serv_city"]      = $reply[6];
		$server_info["serv_sysadmin"]  = $reply[7];
		if (CITADEL_DEBUG_CITPROTO == 1)
		{
			echo "<pre>";
			print_r($server_info);
			echo "</pre>";
		}
		return $server_info;
	}
	else 
		die ("didn't understand the reply to the INFO command");

}


//
// Display a system banner.  (Returns completed HTML.)
// (One is probably temporary because it outputs more or less finalized
// markup.  For now it's just usable.)
//
function ctdl_mesg($msgname) {
	global $clientsocket;

	$msgtext = "<DIV ALIGN=CENTER>\n";

	serv_puts("MESG " . $msgname);
	$response = read_array();

	if (substr($response[0], 0, 1) == "1") {
		array_shift($response); // throw away the status code.
		$msgtext .= "<TT>" . implode( "</TT><BR>\n" ,$response);
	}
	else {
		$msgtext .= "<B><I>" . substr($response[0], 4) . "</I></B><BR>\n";
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
	
	$responses = read_array();
	foreach ($responses as $response) {
		$tokens = explode("|", $response);
		$oneline = array();

		$oneline["session"]      = $tokens[0];		
		$oneline["user"]         = $tokens[1];		
		$oneline["room"]         = $tokens[2];		
		$oneline["host"]         = $tokens[3];		
		$oneline["client"]       = $tokens[4];
		$oneline["idlesince"]    = $tokens[5];
		$oneline["lastcmd"]      = $tokens[6];
		$oneline["flags"]        = $tokens[7];
		$oneline["realname"]     = $tokens[8];
		$oneline["realroom"]     = $tokens[9];
		$oneline["realhostname"] = $tokens[10];
		$oneline["registered"]   = $tokens[11];

		// IGnore the rest of the fields for now.
		if (CITADEL_DEBUG_CITPROTO == 1)
		{
			echo "<pre>";
			print_r($oneline);
			echo "</pre>";

		}


		$num_lines = array_push($all_lines, $oneline);
	}

	return array($num_lines, $all_lines);

}


//
// Goto a room.
//
function ctdl_goto($to_where) {
	
	serv_puts("GOTO " . $to_where);
	$response = serv_gets();

	$results = explode ("|", $response);
	$status_room = array_shift($results);
	$status = substr($status_room, 0, 3);
	if (substr($status, 0, 1) == "2") {
		$room = substr($status_room, 4);
		array_unshift($results, $room);
		$room_state=array(
			"state"          => TRUE,
			"statereply"     => $status,
			"roomname"       => $results[ 0],
			"nunreadmsg"     => $results[ 1],
			"nmessages"      => $results[ 2],
			"rinfopresent"   => $results[ 3],
			"flags"          => $results[ 4],
			"msgidmax"       => $results[ 5],
			"msgidreadmax"   => $results[ 6],
			"ismailroom"     => $results[ 7],
			"isroomaide"     => $results[ 8],
			"nnewmessages"   => $results[ 9],
			"floorid"        => $results[10],
			"viewselected"   => $results[11],
			"defaultview"    => $results[12],
			"istrashcan"     => $results[13]);
			
		$_SESSION["room"] = $room;
		if (CITADEL_DEBUG_CITPROTO == 1)
		{
			echo "<pre>";
			print_r($room_state);
			echo "</pre>";

		}

		return $room_state;
	}

	else {
		return array("state" => FALSE, "statereply" => $status);
	}

}



//
// Fetch the list of known rooms.
//
function ctdl_knrooms() {
	global $clientsocket;

	serv_puts("LKRA");
	$results = read_array();

	if (substr($results[0], 0, 1) != "1") {
		return array(0, NULL);
	}
	array_shift($results);
	$all_lines = array();
	$num_lines = 0;

	foreach ($results as $result){
		$oneline = array();
		$tokens = explode("|",$result);

		$oneline["name"]   = $tokens[0];		
		$oneline["flags"]  = $tokens[1];		
		$oneline["floor"]  = $tokens[2];		
		$oneline["order"]  = $tokens[3];		
		$oneline["flags2"] = $tokens[4];		
		$oneline["access"] = $tokens[5];

		if ($oneline["access"] & 8) {
			$oneline["hasnewmsgs"] = TRUE;
		}
		else {
			$oneline["hasnewmsgs"] = FALSE;
		}

		if (CITADEL_DEBUG_CITPROTO == 1)
		{
			echo "<pre>";
			print_r($oneline);
			echo "</pre>";

		}
		$num_lines = array_push($all_lines, $oneline);
	}

	return array($num_lines, $all_lines);

}


//
// Fetch the list of messages in one room.
// Returns: count, response, message array
//
function ctdl_msgs($mode, $count) {
	global $clientsocket;

	serv_puts("MSGS " . $mode . "|" . $count);
	$responses = read_array();
	print_r($responses);

	$response = array_shift($responses);

	$num_msgs = count($responses);
	if (substr($response, 0, 1) != "1") {
		return array(0, substr($response, 4), NULL);
	}
	
	if (CITADEL_DEBUG_CITPROTO == 1)
	{
		printf("found ".$num_msgs." messages.");
	}
	return array($num_msgs, $response, $responses);
}


// Load a message from the server.
function ctdl_fetch_message($msgnum) {
	global $clientsocket;

	serv_puts("MSG4 " . $msgnum);

	if (CITADEL_DEBUG_CITPROTO == 1)
	    printf ("<div class='ctdldbgRead'>");
	$response = serv_gets(TRUE);

	if (substr($response, 0, 1) != "1") {
		return array(FALSE, substr($response, 4), NULL);
	}

	$fields = array();
	while (strcmp($buf = serv_gets(TRUE), "000")) {
		if (substr($buf, 0, 4) == "text") {
			if (CITADEL_DEBUG_CITPROTO == 1)
				printf ("</div>\n<h3>Message Body Follows</h3><div class='ctdldbgRead'>");
			// We're in the text body.  New loop here.
			$fields["text"] = ctdl_msg4_from_server();
			if (CITADEL_DEBUG_CITPROTO == 1)
				printf ("</div>");
			return array(TRUE, substr($response, 4), $fields);
		}
		else {
			$fields[substr($buf, 0, 4)] = substr($buf, 5);
		}
	}

	// Message terminated prematurely (no text body)
	return array(FALSE, substr($response, 4), $fields);
}

// Support function for ctdl_fetch_message(). This handles the text body
// portion of the message, converting various formats to HTML as
// appropriate.
function ctdl_msg4_from_server() {

	$txt = "";
	$msgformat = "text/plain";
	$in_body = FALSE;

	$previous_line = "";
	while (strcmp($buf = serv_gets(TRUE), "000")) {
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
