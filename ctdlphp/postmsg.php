<?PHP

	include "ctdlheader.php";
	bbs_page_header();

	serv_puts("ENT0 1||0|4|");
	$response = serv_gets();

	if (substr($response, 0, 1) != '4') {
		echo htmlspecialchars(substr($response, 3)) . '<br>' ;
	}
	else {
		serv_puts("Content-type: text/html");
		serv_puts("");
		text_to_server(stripslashes($_REQUEST["msgtext"]), TRUE);
	}

	echo "Message has been posted.<BR>\n" ;

	bbs_page_footer();
?>
