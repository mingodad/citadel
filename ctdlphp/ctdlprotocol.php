<?PHP

function serv_gets() {
	global $clientsocket;

	$buf = fgets($clientsocket, 4096);		// Read line
	$buf = substr($buf, 0, (strlen($buf)-1) );	// strip trailing LF
	return $buf;
}

function serv_puts($buf) {
	global $clientsocket;
	
	fwrite($clientsocket, $buf . "\n", (strlen($buf)+1) );
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
