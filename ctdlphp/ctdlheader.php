<?PHP

include "ctdlsession.php";
include "ctdlprotocol.php";

function bbs_page_header() {

	global $session;

	establish_citadel_session();

	echo <<<CITADEL_HEADER_DATA

<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
<html>
<head>
	<meta http-equiv="Content-Type" content="text/html; charset=iso-8859-1">
	<meta name="Description" content="Citadel BBS">
	<meta name="Keywords" content="citadel,bbs">
	<meta name="MSSmartTagsPreventParsing" content="TRUE">
	<title>FIXME NAME BBS</title>
</head>

<body
	text="#000000"
	bgcolor="#FFFFFF"
	link="#0000FF"
	vlink="#990066"
	alink="#DD0000"
>

Citadel test PHP thing.<BR>
CITADEL_HEADER_DATA;

echo "Your session ID is ", $session, "<BR>\n";
echo "Dude, we're in ", `pwd`, "<BR>";
echo "<A HREF=\"logout.php\">Log out</A><HR>";
flush();

test_for_echo();

}


function bbs_page_footer() {
	global $clientsocket;

	echo "<HR>";

	if (!fclose($clientsocket)) {
		echo "Error closing client socket.<BR>\n";
	}

	echo "Copyright &copy; 2003 by The SCO Group.<BR>\n";
	echo "</BODY></HTML>\n";


}

?>
