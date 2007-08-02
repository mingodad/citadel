#!/usr/bin/php -q

<?php
include "config_ctdlclient.php";
// $Id$
//
// This is the session proxy that binds a unix domain socket to a Citadel
// server connection.  We need one of these for each session because PHP does
// not have a way to bind a session to a persistent socket.
//
// Web designers: don't touch this module.  It's not included in your web pages
// and therefore you don't need to be here.
//
// Copyright (c) 2003 by Art Cancro <ajc@uncensored.citadel.org>
// This program is released under the terms of the GNU General Public License.

global $msgsock;

// sock_gets() -- reads one line of text from a socket
// 
if (CITADEL_DEBUG_PROXY)
{
	define_syslog_variables();
	openlog("sessionproxylog", LOG_PID | LOG_PERROR, LOG_LOCAL0);
}
function sock_gets($sock) {
	global $msgsock;
	socket_clear_error($msgsock);
	$buf = socket_read($sock, 4096, PHP_NORMAL_READ);
	if (CITADEL_DEBUG_PROXY)
	{
		syslog(LOG_DEBUG, "gets Read: ".$buf);
	}
	if (socket_last_error($sock)) return false;

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
if (!$sock) {
	echo "socket_create() failed: ", socket_strerror($sock), "\n";
	system("/bin/rm -f " . $sockname);
	exit(2);
}

$ret = socket_bind($sock, $sockname);
if (!$ret) {
	echo "socket_bind() failed: ", socket_strerror($ret), "\n";
	system("/bin/rm -f " . $sockname);
	exit(3);
}

$ret = socket_listen($sock, 5);
if (!$ret) {
	echo "socket_listen() failed: ", socket_strerror($ret), "\n";
	system("/bin/rm -f " . $sockname);
	exit(4);
}

// Set the permissions so someone else doesn't jump into our connection.
chmod($sockname, 0600);

// We need to get a connection to the Citadel server going now.

$ctdlsock = fsockopen(CITADEL_HOSTNAME, CITADEL_TCP_PORTNO, $errno, $errstr, 30);
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
	// Wait for connections, but time out after 15 minutes.
	// socket_select() is completely b0rken in PHP 4.1, which is why
	// this program requires PHP 4.3 or newer.
	//
	if (socket_select($readsock = array($sock),
			$writesock = NULL,
			$exceptsock = NULL,
			900, 0
	) == 0) {
		// Timing out.
		socket_close ($sock);
		system("/bin/rm -f " . $sockname);
		exit(8);
	}

	// Ok, there's a valid connection coming in.  Accept it.
	$msgsock = socket_accept($sock);
	if ($msgsock >= 0) do {
		$buf = sock_gets($msgsock);
		if ($buf !== false) {
//			fwrite($logfd, ">>");
//			fwride($logfd, $buf);
			if (!fwrite($ctdlsock, $buf . "\n")) {
				fclose($ctdlsock);
				socket_close($sock);
				system("/bin/rm -f " . $sockname);
				exit(9);
			}
			$talkback = fgets($ctdlsock, 4096);
			if (CITADEL_DEBUG_PROXY)
			{
				syslog(LOG_DEBUG, "talkback: ".$talkback);
			}
			if (!$talkback) {
				if (CITADEL_DEBUG_PROXY)
				{
					syslog(LOG_ERROR, "closing socket.");
				}
				fclose($ctdlsock);
				socket_close($sock);
				system("/bin/rm -f " . $sockname);
				exit(10);
			}
        		socket_write($msgsock, $talkback, strlen($talkback));

			// BINARY_FOLLOWS mode
			if (substr($talkback, 0, 1) == "6") {
				$bytes = intval(substr($talkback, 4));
				if (CITADEL_DEBUG_PROXY)
				{
					syslog(LOG_DEBUG, "reading ".$bytes." bytes from server");
				}
				$buf = fread($ctdlsock, $bytes);
				if (CITADEL_DEBUG_PROXY)
				{
					syslog(LOG_DEBUG, "Read: ".$buf);
				}
				socket_write($msgsock, $buf, $bytes);
			}

			// LISTING_FOLLOWS mode
			if (substr($talkback, 0, 1) == "1") do {
				$buf = fgets($ctdlsock, 4096);
				if (!$buf) {
					$buf = "000\n" ;
				}
				else {
        				socket_write($msgsock, $buf,
						strlen($buf));
				}
			} while ($buf != "000\n");

			// SEND_LISTING mode
			if (substr($talkback, 0, 1) == "4") do {
				socket_clear_error($msgsock);
				$buf = sock_gets($msgsock);
				if (socket_last_error($msgsock)) {
					$buf = "000" ;
				}
				if (!fwrite($ctdlsock, $buf . "\n")) {
					fclose($ctdlsock);
					socket_close($sock);
					system("/bin/rm -f " . $sockname);
					exit(11);
				}
			} while ($buf != "000");

		}
	} while($buf !== false);

	socket_close ($msgsock);

} while (true);

socket_close($sock);
fclose($ctdlsock);
system("/bin/rm -f " . $sockname);
exit(0);

?>
