<?PHP
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

