<?PHP

	include "ctdlheader.php";
	bbs_page_header();

	serv_puts("ENT0 1");
	$response = serv_gets();

	if (substr($response, 0, 1) != '4') {
		echo htmlspecialchars(substr($response, 3)) . '<br>' ;
	}
	else {
		echo 'Sending: ' . $_REQUEST["msgtext"] . '<BR>' ;
		flush();
		serv_puts($_REQUEST["msgtext"]);
		echo 'Sending: 000<BR>' ;
		flush();
		serv_puts("000");
	}

	echo "Message has been posted.<BR>\n" ;

	bbs_page_footer();
?>
