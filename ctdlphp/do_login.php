<?PHP

// $Id$
//
// The login form displayed in login.php submits to this page.
// 
// Copyright (c) 2003 by Art Cancro <ajc@uncensored.citadel.org>
// This program is released under the terms of the GNU General Public License.
//

include "ctdlheader.php";

bbs_page_header();

if ($_POST["action"] == "Login") {
	list($retval, $msg) =
		login_existing_user($_POST["user"], $_POST["pass"]);
}
else if ($_POST["action"] == "New User") {
	list($retval, $msg) =
		create_new_user($_POST["user"], $_POST["pass"]);
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
	echo "<A HREF=\"welcome.php\">Logged in.  Click to continue.</a><BR>";
	echo "<meta http-equiv=\"refresh\" content=\"0;url=welcome.php\">\n";
}

bbs_page_footer();

?>

