<?PHP

function test_for_echo() {

	global $clientsocket, $session;

	$command = "ECHO Video vertigo ... test for echo.\n";

	echo "Trying echo ... session name is ", $session, "<BR>\n";
	flush();
	echo "Writing...<BR>\n";
	flush();
	fwrite($clientsocket, $command, strlen($command));
	echo "Reading...<BR>\n";
	flush();
	$response = fgets($clientsocket, 4096);
	echo $response, "<BR>";
	flush();
}

?>
