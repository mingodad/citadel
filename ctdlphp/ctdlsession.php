<?PHP
// $Id$
//
// This gets called from within the header functions.  It establishes or
// connects to a PHP session, and then connects to Citadel if necessary.
//
// Web designers: please make changes in ctdlheader.php, not here.
//
// Copyright (c) 2003 by Art Cancro <ajc@uncensored.citadel.org>
// This program is released under the terms of the GNU General Public License.


function establish_citadel_session() {

	global $session, $clientsocket;

	if (strcmp('4.3.0', phpversion()) > 0) {
		die("This program requires PHP 4.3.0 or newer.");
	}


	session_start();

	if (isset($_SESSION["ctdlsession"])) {
		$session = $_SESSION["ctdlsession"];
	}
	else {
		$session = "CtdlSession." . time() . rand(1000,9999) ;
		$_SESSION["ctdlsession"] = $session;
	}

	// See if there's a Citadel connection proxy open for this session.
	// The name of the socket is identical to the name of the
	// session, and it's found in the /tmp directory.

	$sockname = "/tmp/" . $session . ".socket" ;
	$errno = 0; 
	$errstr = "";
	if (is_array(stat($sockname)))
		$clientsocket = fsockopen(SOCKET_PREFIX.$sockname, 0, $errno, $errstr, 5);
	else
		$clientsocket = false;
//// TODO: if we get connection refused...
	echo "$socketname - $errno - $errstr";
	
	if (!$clientsocket) {
		// It ain't there, dude.  Open up the proxy. (C version)
		//$cmd = "./sessionproxy " . $sockname ;
		//exec($cmd);

		// It ain't there, dude.  Open up the proxy.  (PHP version)
		if (CITADEL_DEBUG_PROXY){
			$stdout = '>>/tmp/sessionproxyout.txt ';
		}
		else{
			$stdout = '>/dev/null ';
		}

		$cmd = "./sessionproxy.php " . $sockname .
			" </dev/null ".$stdout."2>&1 " .
			" 3>&1 4>&1 5>&1 6>&1 7>&1 8>&1 & " ;
		exec($cmd);
		sleep(1);

		// Keep attempting connections 10 times per second up to 100 times
		$attempts = 0;
		while (!$clientsocket) {
			usleep(100);
			if (is_array(stat($sockname)))
				$clientsocket = fsockopen(SOCKET_PREFIX.$sockname, 0, $errno, $errstr, 5);
			else 
				$clientsocket = false;
			$attempts += 1;
			if ($attempts > 100) {
				echo "ERROR: unable to start connection proxy. ";
				echo "Please contact your system administrator.<BR>\n";
				flush();
				exit(1);
			}
		}

		// At this point we have a good connection to Citadel.
		$identity=array(
			"DevelNr" => '0',
			"ClientID" => '8',
			"VersionNumber" => '001',
			"ClientInfoString" => 'PHP web client|',
			"Remote Address" => $_SERVER['REMOTE_ADDR'] );

		ctdl_iden($identity);	// Identify client
		ctdl_MessageFormatsPrefered(array("text/html","text/plain"));
		if (isset($_SESSION["username"])) {
			login_existing_user(
				$_SESSION["username"],
				$_SESSION["password"]
			);
		}

		if (isset($_SESSION["room"])) {
			ctdl_goto($_SESSION["room"]);
		}
		else {
			ctdl_goto("_BASEROOM_");
		}
	}

	if (!isset($_SESSION["serv_humannode"])) {
		$server_info = ctdl_get_serv_info();
		// print_r($server_info);
		$keys = array_keys($server_info);
		foreach ($keys as $key)
			$_SESSION[$key] = $server_info[$key];
	}

	// If the user is trying to call up any page other than
	// login.php logout.php do_login.php,
	// and the session is not logged in, redirect to login.php
	//
	if (isset($_SESSION["logged_in"]) && ($_SESSION["logged_in"] != 1)) {
		$filename = basename(getenv('SCRIPT_NAME'));
		if (	(strcmp($filename, "login.php"))
		   &&	(strcmp($filename, "logout.php"))
		   &&	(strcmp($filename, "do_login.php"))
		) {
			header("Location: login.php");
			exit(0);
		}
	}

	
}


//
// Clear out both our Citadel session and our PHP session.  We're done.
//
function ctdl_end_session() {
	global $clientsocket, $session;

	// Tell the Citadel server to terminate our connection.
	// (The extra newlines force it to see that the Citadel session
	// ended, and the proxy will quit.)
	//
	fwrite($clientsocket, "QUIT\n\n\n\n\n\n\n\n\n\n\n");
	$response = fgets($clientsocket, 4096);		// IGnore response
	fclose($clientsocket);
	unset($clientsocket);

	// Now clear our PHP session.
	$_SESSION = array();
	session_write_close();
}

?>
