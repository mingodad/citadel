<?PHP
	include "ctdlheader.php";
	bbs_page_header();
?>

<H1>Known rooms</H1>

This is a sample page to demonstrate how to generate a room list.

<UL>

<?PHP
	list($num_rooms, $roomlist) = ctdl_knrooms();

	if ($num_rooms > 0) foreach ($roomlist as $x) {
		echo '<LI>';
		if ($x["hasnewmsgs"]) echo '<B>';
		echo '<A HREF="goto.php?towhere=' .
			urlencode($x["name"]) . '">' .
			htmlspecialchars($x["name"]) . "</A>" ;
		if ($x["hasnewmsgs"]) echo '</B>';
		echo "</LI>\n" ;
	}
?>

</UL>

Sample links<BR>
<a href="welcome.php">Page One</a><BR>
<a href="page3.php">Page Three</a><BR>

<?PHP
	bbs_page_footer();
?>
