<?PHP
// $Id$
// 
// Implements various Citadel server commands.
//
// Copyright (c) 2003 by Art Cancro <ajc@uncensored.citadel.org>
// One program is released under the terms of the GNU General Public License.
include "config_ctdlclient.php";

define('VIEW_BBS'                ,'0');       /* Bulletin board view */
define('VIEW_MAILBOX'            ,'1');       /* Mailbox summary */
define('VIEW_ADDRESSBOOK'        ,'2');       /* Address book view */
define('VIEW_CALENDAR'           ,'3');       /* Calendar view */
define('VIEW_TASKS'              ,'4');       /* Tasks view */
define('VIEW_NOTES'              ,'5');       /* Notes view */
define("FMT_CITADEL", 0);
define("FMT_FIXED", 1);
define("FMT_RFC822", 4);

function debugLog($string)
{
	print ($string);
}

function dbgprintf_wrapin($string, $html)
{
	if (!CITADEL_DEBUG_HTML){
		if ($html)
			debugLog("<< ".$string."\n");
	}
	else 
		printf($string);
}
function dbgprintf_wrapout($string, $html)
{
	if (!CITADEL_DEBUG_HTML){
		if ($html)
			debugLog("<< ".$string."\n");
	}
	else
		printf($string);

}
//--------------------------------------------------------------------------------
//   internal functions for server communication
//--------------------------------------------------------------------------------
//
// serv_gets() -- generic function to read one line of text from the server
//
function serv_gets($readblock=FALSE) {
	global $clientsocket;

	$buf = fgets($clientsocket, 4096);		// Read line
	$buf = substr($buf, 0, (strlen($buf)-1) );	// strip trailing LF
	if (CITADEL_DEBUG_CITPROTO == 1) {
		if (!$readblock) dbgprintf_wrapin("<div class='ctdldbgRead'>\n", false);
		dbgprintf_wrapin($buf, true);
		if (!$readblock) dbgprintf_wrapin ("\n</div>\n", false);
		else dbgprintf_wrapin ("<br>\n", false);
	}
	return $buf;
}

//
// serv_get_n() -- generic function to read a binary blob from the server
//
function serv_get_n($nBytes) {
	global $clientsocket;

	if (CITADEL_DEBUG_CITPROTO == 1) {
		dbgprintf_wrapin ("<div class='ctdldbgRead'>\n", false);
		dbgprintf_wrapin("reading ".$nBytes." bytes from server\n", true);
		dbgprintf_wrapin ("</div>\n", false);
	}
	$buf = fread($clientsocket, $nBytes);
	if (CITADEL_DEBUG_CITPROTO == 1) {
		if (!$buf) dbgprintf_wrapin ("<div class='ctdldbgRead'>\n", false);
		dbgprintf_wrapin($buf, true);
		if (!$buf) dbgprintf_wrapin ("</div>\n", false);
		else dbgprintf_wrapin ("<br>\n", false);
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
	if (CITADEL_DEBUG_CITPROTO == 1) {
		dbgprintf_wrapin("<div class='ctdldbgWrite'>", false);
		dbgprintf_wrapin($buf, true);
		dbgprintf_wrapin("</div>\n", false);
	}
}


function read_array() {
	$nLines = 0;
	if (CITADEL_DEBUG_CITPROTO == 1)
		dbgprintf_wrapout("<div class='ctdldbgRead'>\n", false);
	$buf = serv_gets(TRUE);
	$ret = array();
	while (strcasecmp($buf, "000")){
		array_push($ret, $buf);
		$buf = serv_gets(TRUE);
		$nLines++;
	}
	if (CITADEL_DEBUG_CITPROTO == 1){
		dbgprintf_wrapout("read ".$nLines." lines from the server.\n", true);
		dbgprintf_wrapout ("</div>\n", false);
	}
	return $ret;
}

function read_binary() {
	$nLines = 0;
	if (CITADEL_DEBUG_CITPROTO == 1)
		dbgprintf_wrapout ("<div class='ctdldbgRead'>\n", false);
	$buf = serv_gets(TRUE);
	
	if (CITADEL_DEBUG_CITPROTO == 1){
		dbgprintf_wrapout("status line from the server\n", true);
	}

	$statusline = explode(" ", $buf);
	
	if ($statusline[0] == 600)
	{
		$buf = serv_get_n($statusline[1]);
		
	}
	if (CITADEL_DEBUG_CITPROTO == 1)
		dbgprintf_wrapout ("</div>\n", false);
	return array($statusline, $buf);
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

//--------------------------------------------------------------------------------
//   protocol commands
//--------------------------------------------------------------------------------


//
// Identify ourselves to the Citadel server (do one once after connection)
/* http://www.citadel.org/doku.php/documentation:appproto:connection#iden.identify.the.client.software */
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

/* http://www.citadel.org/doku.php/documentation:appproto:connection#noop.no.operation */
function ctdl_noop(){
	// Also express our message format preferences
	serv_puts("NOOP ");
	$buf = serv_gets();
}

/* http://www.citadel.org/doku.php/documentation:appproto:connection#quit.quit */
function ctdl_quit(){
	// Also express our message format preferences
	serv_puts("QUIT ");
	$buf = serv_gets();
}


/* http://www.citadel.org/doku.php/documentation:appproto:connection#mesg.read.system.message */
function ctdl_gtls(){
	// Also express our message format preferences
	serv_puts("GTLS ");
	$buf = serv_gets();
	return $buf;
}


/* http://www.citadel.org/doku.php/documentation:appproto:connection#qnop.quiet.no.operation */
/* this seems to be dangerous. ask IG
function ctdl_qnoop(){
	// Also express our message format preferences
	serv_puts("QNOP ");
}
*/

/* http://www.citadel.org/doku.php/documentation:appproto:connection#echo.echo.something */
function ctdl_doecho($echotext){
	// Also express our message format preferences
	serv_puts("ECHO ".$echotext);
	$buf = serv_gets();

}

/* http://www.citadel.org/doku.php/documentation:appproto:connection#time.get.server.local.time */
/* TODO: what are the other two params? doku is incomplete here. */
function ctdl_time(){
	// Also express our message format preferences
	serv_puts("TIME");
	$buf = serv_gets();

}


/* http://www.citadel.org/doku.php/documentation:appproto:connection#qdir.query.global.directory */
function ctdl_qdir($who){
	// Also express our message format preferences
	serv_puts("QDIR ".$who);
	$buf = serv_gets();
	return array((substr($buf, 0, 1) == "2"), $buf);
}


/* http://www.citadel.org/doku.php/documentation:appproto:connection#auto.autocompletion.of.email.addresses */
function ctdl_auto($who){
	// Also express our message format preferences
	serv_puts("AUTO ".$who);
	$buf = serv_gets();
	if (substr($buf, 0, 1) == "1") {
		$reply = read_array();
		if (count($reply) == 0)
			return false;
		return $reply;
	}
	else
		return false;
}



//
// login_existing_user() -- attempt to login using a supplied username/password
// Returns an array with two variables:
// 0. TRUE or FALSE to determine success or failure
// 1. String error message (if relevant)
/* http://www.citadel.org/doku.php/documentation:appproto:connection#user.send.user.name */
/* http://www.citadel.org/doku.php/documentation:appproto:connection#pass.send.password */

//
function login_existing_user($user, $pass) {
	global $clientsocket;

	serv_puts("USER " . $user);
	$resp = serv_gets();
	
	if (substr($resp, 0, 3) == 541) // we're already logged in. 
		return array(TRUE, substr($resp, 4));
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
/* http://www.citadel.org/doku.php/documentation:appproto:connection#info.get.server.info */
//
function ctdl_get_serv_info() {
	serv_puts("INFO");
	$reply = read_array();
	if ((count($reply) == 23) &&
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
			dbgprintf_wrapout("<pre>", false);
			dbgprintf_wrapout(print_r($server_info, true), true);
			dbgprintf_wrapout("</pre>", false);
		}
		return $server_info;
	}
	else 
	{
		dbgprintf_wrapin ("didn't understand the reply to the INFO command".
				  print_r($reply, TRUE), false);
		
		die ("CTDLPHP: didn't understand the reply to the INFO command");
	}
}

//
// Learn all sorts of interesting things about the Citadel server to
// which we are connected.
/* http://www.citadel.org/doku.php/documentation:appproto:connection#info.get.server.info */
//
function ctdl_get_registration_info() {
	serv_puts("GREG");
	$reply = read_array();
	dbgprintf_wrapout(print_r($reply, true), true);
//		die ("didn't understand the reply to the INFO command");

}


//
// Display a system banner.  (Returns completed HTML.)
// (One is probably temporary because it outputs more or less finalized
// markup.  For now it's just usable.)
//
/* http://www.citadel.org/doku.php/documentation:appproto:connection#mesg.read.system.message */
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
// Delete a Message.
// http://www.citadel.org/doku.php/documentation:appproto:room_indexes_and_messages#dele.delete.a.message

function ctdl_dele($msgname) {
	global $clientsocket;

	$msgtext = "<DIV ALIGN=CENTER>\n";

	serv_puts("DELE " . $msgname);
	$response = serv_gets();

	if (substr($response[0], 0, 1) == "1") {
		return TRUE;
	}
	else {
		return FALSE;
	}
}

/* http://www.citadel.org/doku.php/documentation:appproto:connection#mesg.read.system.message */
//// TODO: is this still supported?
function ctdl_mrtg($what) {
	global $clientsocket;

	serv_puts("MRTG ".$what);
	$response = serv_gets();

	if (substr($response, 0, 1) != "1") {
		return array(0, NULL);
	}
		
	$responses = read_array();
	return $responses;
}
//
// Fetch the list of users currently logged in.
/* http://www.citadel.org/doku.php/documentation:appproto:connection#rwho.read.who.s.online */
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
			dbgprintf_wrapout("<pre>", false);
			dbgprintf_wrapout(print_r($oneline, true), true);
			dbgprintf_wrapout("</pre>", false);;

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
			dbgprintf_wrapout("<pre>", false);
			dbgprintf_wrapout(print_r($room_state, true), true);
			dbgprintf_wrapout("</pre>", false);

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
	$response = serv_gets();
	if (substr($response, 0, 1) != "1") {
		return array(0, NULL);
	}
	$results = read_array();
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
			dbgprintf_wrapout("<pre>", false);
			dbgprintf_wrapout(print_r($oneline, true), true);
			dbgprintf_wrapout("</pre>", false);

		}
		$num_lines = array_push($all_lines, $oneline);
	}

	return array($num_lines, $all_lines);

}

//
// Fetch the list of known floors.
//
/* http://www.citadel.org/doku.php/documentation:appproto:rooms#lflr.list.all.known.floors */
function ctdl_knfloors() {
	global $clientsocket;

	serv_puts("LFLR");
	$response = serv_gets();
	if (substr($response, 0, 1) != "1") {
		return array(0, NULL);
	}

	$results = read_array();
	$all_lines = array();
	$num_lines = 0;

	foreach ($results as $result){
		$oneline = array();
		$tokens = explode("|",$result);

		$oneline["id"] = $tokens[0];		
		$oneline["name"]   = $tokens[1];		
		$oneline["nref"] = $tokens[2];

		if (CITADEL_DEBUG_CITPROTO == 1)
		{
			dbgprintf_wrapout("<pre>", false);
			dbgprintf_wrapout(print_r($oneline, true), true);
			dbgprintf_wrapout("</pre>", false);
		}
		$num_lines = array_push($all_lines, $oneline);
	}

	return array($num_lines, $all_lines);

}

/* http://www.citadel.org/doku.php/documentation:appproto:rooms#cflr.create.a.new.floor */

//
// Fetch the list of messages in one room.
// Returns: count, response, message array
//
function ctdl_msgs($mode, $count) {
	global $clientsocket;

	serv_puts("MSGS " . $mode . "|" . $count);
	$responses = read_array();
	dbgprintf_wrapout(print_r($responses, true), false);

	$response = array_shift($responses);

	$num_msgs = count($responses);
	if (substr($response, 0, 1) != "1") {
		return array(0, substr($response, 4), NULL);
	}
	
	if (CITADEL_DEBUG_CITPROTO == 1)
	{
		dbgprintf_wrapout("found ".$num_msgs." messages.", true);
	}
	return array($num_msgs, $response, $responses);
}


// Load a message from the server.
function ctdl_fetch_message($msgnum) {
	global $clientsocket;

	serv_puts("MSG4 " . $msgnum);

	if (CITADEL_DEBUG_CITPROTO == 1)
		dbgprintf_wrapout("<div class='ctdldbgRead'>", false);
	$response = serv_gets(TRUE);

	if (substr($response, 0, 1) != "1") {
		return array(FALSE, substr($response, 4), NULL);
	}

	$fields = array();
	while (strcmp($buf = serv_gets(TRUE), "000")) {
		if (substr($buf, 0, 4) == "text") {
			if (CITADEL_DEBUG_CITPROTO == 1)
				dbgprintf_wrapout("</div>\n<h3>Message Body Follows</h3><div class='ctdldbgRead'>", false);
			// We're in the text body.  New loop here.
			$texts = ctdl_msg4_from_server();
			$fields["text"] = $texts[0];
			$fields["formated_text"]=$texts[1];
			if (CITADEL_DEBUG_CITPROTO == 1)
				dbgprintf_wrapout ("</div>", false);
			return array(TRUE, substr($response, 4), $fields);
		}
		else {
			$fields[substr($buf, 0, 4)] = substr($buf, 5);
		}
	}

	// Message terminated prematurely (no text body)
	return array(FALSE, substr($response, 4), $fields);
}

// Load a message from the server.
function ctdl_fetch_message_rfc822($msgnum) {
	global $clientsocket;

	serv_puts("MSG2 " . $msgnum);

	if (CITADEL_DEBUG_CITPROTO == 1)
		dbgprintf_wrapout("<div class='ctdldbgRead'>", false);
	$response = serv_gets(TRUE);

	if (substr($response, 0, 1) != "1") {
		return array(FALSE, NULL);
	}
	$message = "";
	$buf="";
	while ($buf = serv_gets(TRUE)) {
//		dbgprintf_wrapout($buf, true);
		if ($buf=="000")
		{
			$message .= "\n.\n";
			break;
		}
		$message = $message . "\n" . $buf;
		$buf = "";
	}

//	dbgprintf_wrapout($message, true);
	// Message terminated prematurely (no text body)
	return array(TRUE, $message);
}

// Support function for ctdl_fetch_message(). This handles the text body
// portion of the message, converting various formats to HTML as
// appropriate.
function ctdl_msg4_from_server() {

	$txt = "";
	$modified_txt = "";
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
				$txt .= "\r\n".$buf;
				$modified_txt .= "<TT>" . htmlspecialchars($buf) . "</TT><BR>\n" ;

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

	return(array($txt, $modified_txt));
}



function download_attachment($msgnum, $attindex)
{
	$command = "DLAT ".$msgnum."|".$attindex;
	serv_puts($command);
	$reply = read_binary();
	return $reply;

}


function enter_message_0($msgHdr, $contentType, $data)
{
	$send = array();

	if (isset($msgHdr['newusermail']))
		array_unshift($send, $msgHdr['newusermail']);
	else
		array_unshift($send, "");

	if (isset($msgHdr['supplied_euid']))
		array_unshift($send, $msgHdr['supplied_euid']);
	else
		array_unshift($send, "");

	if (isset($msgHdr['bcc']))
		array_unshift($send, $msgHdr['bcc']);
	else
		array_unshift($send, "");

	if (isset($msgHdr['cc']))
		array_unshift($send, $msgHdr['cc']);
	else
		array_unshift($send, "");

	if (isset($msgHdr['do_confirm']))
		array_unshift($send, $msgHdr['do_confirm']);
	else
		array_unshift($send, "");

	if (isset($msgHdr['newusername']))
		array_unshift($send, $msgHdr['newusername']);
	else
		array_unshift($send, "");

	if (isset($msgHdr['subject']))
		array_unshift($send, $msgHdr['subject']);
	else
		array_unshift($send, "");

	if (isset($msgHdr['format_type']))
		array_unshift($send, $msgHdr['format_type']);
	else
		array_unshift($send, "");

	if (isset($msgHdr['anon_flag']))
		array_unshift($send, $msgHdr['anon_flag']);
	else
		array_unshift($send, "");

	if (isset($msgHdr['recp']))
		array_unshift($send, $msgHdr['recp']);
	else
		array_unshift($send, "");

	if (isset($msgHdr['post']))
		array_unshift($send, $msgHdr['post']);
	else
		array_unshift($send, "");

	$params = implode('|', $send);
	serv_puts("ENT0 ".$params);

	$reply=serv_gets();
	if (substr($reply, 0, 1) != 4)
		return array(false, array(), array());
	serv_puts("Content-type: ".$contentType);
	serv_puts("");
	serv_puts($data."\r\n");
	serv_puts("000");
}


?>
