<?PHP

	include "ctdlheader.php";
	bbs_page_header();

	echo	"This is an example of the use of ctdl_rwho() to display " .
		"the list of users currently logged in.<BR><BR>\n" ;

	echo	"<TABLE border=1>" .
		"<TR>" .
		"<TD><B>User</B></TD>" .
		"<TD><B>Room</B></TD>" .
		"<TD><B>Host</B></TD>" .
		"</TR>" ;

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

<BR>Sample links<BR>
<a href="welcome.php">Page One</a><BR>
<a href="page3.php">Page Three</a><BR>

<?PHP
	bbs_page_footer();
?>
