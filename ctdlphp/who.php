<?PHP
	include "ctdlheader.php";
	bbs_page_header();


	echo "<TABLE border=1>";
	echo "<TR>";
	echo "<TD><B>User</B></TD>";
	echo "<TD><B>Room</B></TD>";
	echo "<TD><B>Host</B></TD>";
	echo "</TR>";

	list($num_users, $wholist) = ctdl_rwho();

	if ($num_users > 0) foreach ($wholist as $x) {
		echo "<TR>";
		echo "<TD>", htmlspecialchars($x["user"]), "</TD>";
		echo "<TD>", htmlspecialchars($x["room"]), "</TD>";
		echo "<TD>", htmlspecialchars($x["host"]), "</TD>";
		echo "</TR>\n";
	}

	echo "</TABLE>\n";

?>

<a href="welcome.php">Page One</a><BR>
<a href="page3.php">Page Three</a><BR>

<?PHP
	bbs_page_footer();
?>
