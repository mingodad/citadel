<?PHP

include "ctdlsession.php";
include "ctdlprotocol.php";

function bbs_page_header() {
	global $session;

	if(strcmp('4.3.0', phpversion()) > 0) {
		die("This program requires PHP 4.3.0 or newer.");
	}

	establish_citadel_session();

	echo <<<LITERAL

<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
<html>
<head>
	<meta http-equiv="Content-Type" content="text/html; charset=iso-8859-1">
	<meta name="Description" content="Citadel BBS">
	<meta name="Keywords" content="citadel,bbs">
	<meta name="MSSmartTagsPreventParsing" content="TRUE">
	<title>
LITERAL;

	if ($_SESSION["serv_humannode"]) {
		echo $_SESSION["serv_humannode"] ;
	}
	else {
		echo "BBS powered by Citadel" ;
	}

	echo <<<LITERAL
</title>
</head>

<body
	text="#000000"
	bgcolor="#FFFFFF"
	link="#0000FF"
	vlink="#990066"
	alink="#DD0000"
>

LITERAL;

	echo "Your session ID is ", $session, "<BR>\n";
	echo "<A HREF=\"logout.php\">Log out</A><HR>";
	// flush();
	// test_for_echo();
}


function bbs_page_footer() {
	echo "<HR>";
	echo "Powered by Citadel.  And a few cups of coffee.<BR>\n";
	echo "</BODY></HTML>\n";
}

?>
