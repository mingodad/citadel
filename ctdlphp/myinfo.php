<?PHP

	include "ctdlheader.php";
	bbs_page_header();

	echo	"This is an example of the use of " .
	        "ctdl_get_registration_info() to display " .
		"your personal data.<BR><BR>\n" ;

	echo	"<TABLE border=1>" .
		"<TR>" .
		"<TD><B>User</B></TD>" .
		"<TD><B>Room</B></TD>" .
		"<TD><B>Host</B></TD>" .
		"</TR>" ;

        ctdl_goto("My Citadel Config");
	list($num_msgs, $response, $msgs) = ctdl_msgs($_REQUEST["mode"],
							$_REQUEST["count"] );

	echo "num_msgs: " . $num_msgs . "<BR>\n" ;
	echo "response: " . htmlspecialchars($response) . "<BR>\n" ;

        if ($num_msgs > 0) foreach ($msgs as $msgnum) {
		print_r($msgnum);
		$result = get_message_partlist($msgnum);
		if (is_array($result) &&
		    ($result[4]=="text/vcard"))
		{
			$vcard = downoad_attachment($msgnum, $result[2]);


		}
	}

///phpinfo();
///	list($num_users, $wholist) =
// ctdl_get_registration_info();
/*
	if ($num_users > 0) foreach ($wholist as $x) {
		echo "<TR>";
		echo "<TD>", htmlspecialchars($x["user"]), "</TD>";
		echo "<TD>", htmlspecialchars($x["room"]), "</TD>";
		echo "<TD>", htmlspecialchars($x["host"]), "</TD>";
		echo "</TR>\n";
	}
*/
	echo "</TABLE>\n";

?>

<BR>Sample links<BR>
<a href="welcome.php">Page One</a><BR>
<a href="page3.php">Page Three</a><BR>

<?PHP
	bbs_page_footer();
?>
