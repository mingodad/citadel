<?PHP
	include "ctdlheader.php";
	bbs_page_header();

	list($num_msgs, $response, $msgs) = ctdl_msgs($_REQUEST["mode"],
							$_REQUEST["count"] );

	echo "num_msgs: " . $num_msgs . "<BR>\n" ;
	echo "response: " . htmlspecialchars($response) . "<BR>\n" ;

        if ($num_msgs > 0) foreach ($msgs as $msgnum) {
		display_message($msgnum);
	}

?>

<BR>
<a href="welcome.php">Page One</a><BR>
<a href="page3.php">Page Three</a><BR>

<?PHP
	bbs_page_footer();
?>
