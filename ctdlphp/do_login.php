<?PHP

// $Id$
//
// The login form displayed in login.php submits to this page.
// 
// Copyright (c) 2003 by Art Cancro <ajc@uncensored.citadel.org>
// This program is released under the terms of the GNU General Public License.

include "ctdlheader.php";

bbs_page_header();

if ($_REQUEST["action"] == "Login") {
	list($retval, $msg) =
		login_existing_user($_REQUEST["user"], $_REQUEST["pass"]);
}
else if ($_REQUEST["action"] == "New User") {
	list($retval, $msg) =
		create_new_user($_REQUEST["user"], $_REQUEST["pass"]);
}
else {
	echo "uuuuhhhhhhhhh....<BR>\n" ;
}


if ($retval == FALSE) {
	echo "<DIV ALIGN=CENTER>", $msg, "<BR><BR>\n" ;
	echo "<a href=\"login.php\">Try again</A><BR>\n" ;
	echo "<a href=\"logout.php\">Log out</A><BR>\n" ;
}
else {
	echo "<A HREF=\"welcome.php\">Logged in.  ";
	echo "Click to continue if your browser does not refresh.</a><BR>";
	echo "<meta http-equiv=\"refresh\" content=\"15;url=welcome.php\">\n";
}

bbs_page_footer();

?>

