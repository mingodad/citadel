<?PHP
	include "ctdlheader.php";
	bbs_page_header();

	ctdl_goto($_REQUEST["towhere"]);

	echo "You are now in: ", htmlspecialchars($_SESSION["room"]) , "<BR>\n";
?>

<a href="readmsgs.php?mode=new&count=0">Read new messages</a><BR>
<a href="readmsgs.php?mode=all&count=0">Read all messages</a><BR>
<a href="welcome.php">Page One</a><BR>
<a href="page3.php">Page Three</a><BR>

<?PHP
	bbs_page_footer();
?>
