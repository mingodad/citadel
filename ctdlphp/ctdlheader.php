<?PHP

// $Id$
//
// Header and footer code to be included on every page.  Not only does it
// contain some common markup, but it also calls some code glue that holds
// the session together.
//
// Copyright (c) 2003 by Art Cancro <ajc@uncensored.citadel.org>
// This program is released under the terms of the GNU General Public License.

include "ctdlsession.php";
include "ctdlprotocol.php";

function bbs_page_header() {
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

<div align=right>
<A HREF="logout.php">Log out</A>
</div>
<HR>

LITERAL;

}


function bbs_page_footer() {
	echo "<HR>";
	echo "Powered by Citadel.  And a few cups of coffee.<BR>\n";
	echo "</BODY></HTML>\n";
}

?>
