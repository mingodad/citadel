#!/usr/bin/php -q

<?php

// $Id$
//
// This is the session proxy that binds a unix domain socket to a Citadel
// server connection.  We need one of these for each session because PHP does
// not have a way to bind a session to a persistent socket.
//
// Copyright (c) 2003 by Art Cancro <ajc@uncensored.citadel.org>
// This program is released under the terms of the GNU General Public License.
//


// sock_gets() -- reads one line of text from a socket
// 
function sock_gets($sock) {
	$buf = socket_read($sock, 4096, PHP_NORMAL_READ);
	if ($buf == false) return false;

	if (preg_match("'\n$'s", $buf)) {
		$buf = substr($buf, 0, strpos($buf, "\n"));
	}

	return $buf;
}





// *** Start of main program ***

error_reporting(E_ALL);

/* Allow the script to hang around waiting for connections. */
set_time_limit(0);

/* Turn on implicit output flushing so we see what we're getting
 * as it comes in.
 */
ob_implicit_flush();

if ($argc != 2) {
	echo "usage: ", $argv[0], " sockname\n";
	exit(1);
}

$sockname = $argv[1];

if ($sockname == "/tmp/") {
	echo "Invalid socket name.\n";
	exit(1);
}

// If there's a dead socket already there, remove it.
system("/bin/rm -f " . $sockname);

$sock = socket_create(AF_UNIX, SOCK_STREAM, 0);
if ($sock < 0) {
	echo "socket_create() failed: ", socket_strerror($sock), "\n";
	system("/bin/rm -f " . $sockname);
	exit(2);
}

$ret = socket_bind($sock, $sockname);
if ($ret < 0) {
	echo "socket_bind() failed: ", socket_strerror($ret), "\n";
	system("/bin/rm -f " . $sockname);
	exit(3);
}

$ret = socket_listen($sock, 5);
if ($ret < 0) {
	echo "socket_listen() failed: ", socket_strerror($ret), "\n";
	system("/bin/rm -f " . $sockname);
	exit(4);
}

// We need to get a connection to the Citadel server going now.

$ctdlsock = fsockopen("uncensored.citadel.org", 504, $errno, $errstr, 30);
// $ctdlsock = fsockopen("/appl/citadel/citadel.socket", 0, $errno, $errstr, 30);
if (!$ctdlsock) {
	socket_close ($sock);
	system("/bin/rm -f " . $sockname);
	exit(5);
}

// Read the greeting from the Citadel server.
if (!$buf = fgets($ctdlsock, 4096)) {
	socket_close ($sock);
	system("/bin/rm -f " . $sockname);
	exit(6);
}

// Make sure the server is allowing logins.
if (substr($buf, 0, 1) != "2") {
	socket_close ($sock);
	system("/bin/rm -f " . $sockname);
	exit(7);
}

do {
	$msgsock = socket_accept($sock);
	if ($msgsock >= 0) do {
		$buf = sock_gets($msgsock);
		if ($buf !== false) {
			if (!fwrite($ctdlsock, $buf . "\n")) {
				fclose($ctdlsock);
				socket_close($sock);
				system("/bin/rm -f " . $sockname);
				exit(8);
			}
			$talkback = fgets($ctdlsock, 4096);
			if (!$talkback) {
				fclose($ctdlsock);
				socket_close($sock);
				system("/bin/rm -f " . $sockname);
				exit(9);
			}
        		socket_write($msgsock, $talkback, strlen($talkback));

			if (substr($talkback, 0, 1) == "1") do {
				$buf = fgets($ctdlsock, 4096);
        			socket_write($msgsock, $buf, strlen($buf));
			} while ($buf != "000\n");

		}
	} while($buf !== false);

	socket_close ($msgsock);

} while (true);

socket_close($sock);
fclose($ctdlsock);
system("/bin/rm -f " . $sockname);
exit(0);

?>
