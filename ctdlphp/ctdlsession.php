<?PHP

//
// ctdlsession.php
//
// This gets called from within the header functions.  It establishes or
// connects to a PHP session, and then connects to Citadel if necessary.
//

function establish_citadel_session() {

	global $session, $clientsocket;

	// echo "Calling session_start()<BR>\n";
	// flush();
	session_start();
	$session = "CtdlSession." . session_id();

	// See if there's a Citadel connection proxy open for this session.
	// The name of the socket is identical to the name of the
	// session, and it's found in the /tmp directory.

	$sockname = "/tmp/" . $session . ".socket" ;

	// echo "Connecting to ", $sockname, "...<BR>\n";
	// flush();
	$clientsocket = fsockopen($sockname, 0, $errno, $errstr, 5);
	if (!$clientsocket) {
		//echo "Socket not present.  Firing up a new proxy.<BR>\n";
		//flush();

		// It ain't there, dude.  Open up the proxy. (C version)
		//$cmd = "./sessionproxy " . $sockname ;
		//exec($cmd);

		// It ain't there, dude.  Open up the proxy.  (PHP version)
		$cmd = "./sessionproxy.php " . $sockname .
			" </dev/null >/dev/null 2>&1 " .
			" 3>&1 4>&1 5>&1 6>&1 7>&1 8>&1 & " ;
		exec($cmd);
		sleep(2);

		// Ok, now try again.
		// echo "Connecting to ", $sockname, "...<BR>\n";
		// flush();
		$clientsocket = fsockopen($sockname, 0, $errno, $errstr, 5);
	}

	if ($clientsocket) {
		/*
		echo "Connected.  Performing echo tests.<BR>\n";
		flush();
		$cmd = "ECHO test echo string upon connection\n";
		fwrite($clientsocket, $cmd, strlen($cmd));
		$response = fgets($clientsocket, 4096);
		echo "Response is: ", $response, "<BR>\n";
		flush();

		$cmd = "ECHO second test for echo\n";
		fwrite($clientsocket, $cmd, strlen($cmd));
		$response = fgets($clientsocket, 4096);
		echo "Response is: ", $response, "<BR>\n";
		flush();
		*/
	}
	else {
		echo "ERROR: no Citadel socket!<BR>\n";
		flush();
	}
	
}


function ctdl_end_session() {
	global $clientsocket, $session;

	session_destroy();
	unset($clientsocket);
	unset($session); 

	echo "Session destroyed.<BR>\n";
	
}

?>
