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
			echo $msg, "<BR>\n";
	}
	if ($_POST["action"] == "New User") {
		list($retval, $msg) =
			create_new_user($_POST["user"], $_POST["pass"]);
			echo $msg, "<BR>\n";
	}
	else {
		echo "uuuuhhhhhhhh....<BR>\n";
	}

?>


<a href="page2.php">Page Two</a><BR>
<a href="page3.php">Page Three</a><BR>

<?PHP
	bbs_page_footer();
?>

