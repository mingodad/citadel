<?PHP
	include "ctdlheader.php";
	bbs_page_header();

	ctdl_goto($_REQUEST["towhere"]);

	echo "You are now in: ", htmlspecialchars($_SESSION["room"]) , "<BR>\n";
?>

<a href="welcome.php">Page One</a><BR>
<a href="page3.php">Page Three</a><BR>

<?PHP
	bbs_page_footer();
?>
