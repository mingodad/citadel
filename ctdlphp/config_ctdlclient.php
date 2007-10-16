<?PHP

// Please override these defaults in config_ctdlclient_local.php, not here.

#do you want to see the server conversation for exploring the protocol?
define('CITADEL_DEBUG_PROXY', FALSE);
#switch this if you're using php5
define('SOCKET_PREFIX', "unix://");
#define('SOCKET_PREFIX', "");
include "config_ctdlclient_local.php";

// Examples:
// 
// On the same host:
// unix:///var/run/citadel/citadel.sock
//
// On a different host (or via loopback):
// tcp://citserver.example.com
// tcp://127.0.0.1

if (!defined('CITADEL_HOSTNAME')) {
	define('CITADEL_HOSTNAME',"tcp://127.0.0.1");
}

// make it 0 to use unix domain sockets
if (!defined('CITADEL_TCP_PORTNO')) {
	define('CITADEL_TCP_PORTNO','504');
}

// do you want to see the server conversation for exploring the protocol?
if (!defined('CITADEL_DEBUG_CITPROTO')) {
	define('CITADEL_DEBUG_CITPROTO',0);
}
if (!defined('CITADEL_DEBUG_PROXY')) {
	define('CITADEL_DEBUG_PROXY', FALSE);
}

if (!defined('CITADEL_DEBUG_HTML')) {
	define('CITADEL_DEBUG_HTML', FALSE);
}
?>
