<?PHP
#could be: "uncensored.citadel.org"
# or the path of the unix domain socket: /var/run/citadel/citadel.sock
define('CITADEL_HOSTNAME',"127.0.0.1");

#make it 0 to use unix domain sockets
define('CITADEL_TCP_PORTNO','504');

#do you want to see the server conversation for exploring the protocol?
define('CITADEL_DEBUG_CITPROTO',1);
?>