<?PHP

	include "ctdlheader.php";
	bbs_page_header();

	serv_puts("ENT0 ");
	$response = serv_gets();

	if (substr($response, 0, 1) != '2') {
		echo htmlspecialchars(substr($response, 3)) . '<br>' ;
	}
	else {
		echo	'<center>' .
			'<form ENCTYPE="multipart/form-data" ' .
			'METHOD="POST" ACTION="postmsg.php" NAME="postmsg">' .
			'<input TYPE="submit" NAME="sc" VALUE="Save message">'.
			'<input TYPE="submit" NAME="sc" VALUE="Cancel">' .
			'<br>' .
			'<textarea NAME="msgtext" wrap=soft' .
			' ROWS=25 COLS=80 WIDTH=80></textarea><br> '.
			'</form>' .
			'</center> '
		;
	}

	bbs_page_footer();
?>
