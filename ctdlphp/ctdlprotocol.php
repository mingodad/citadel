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
// Learn all sorts of interesting things about the Citadel server to
// which we are connected.
//
function ctdl_get_serv_info() {
	global $serv_humannode;
	global $serv_software;

	serv_puts("INFO");
	serv_gets($buf);
	if (substr($buf, 0, 1) == "1") {
		$i = 0;
		do {
			$buf = serv_gets();
			if ($i == 2) $serv_humannode = $buf;
			if ($i == 4) $serv_software = $buf;
			$i = $i + 1;
		} while ($buf != "000");
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
			if ($buf != "000") {
				echo "<TT>", $buf, "</TT><BR>\n" ;
			}
		} while ($buf != "000");
		echo "</DIV>\n";
	}
	else {
		echo "<B><I>", substr($response, 4), "</I></B><BR>\n";
	}
}

?>
