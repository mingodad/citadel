<?PHP
	include "ctdlheader.php";

	bbs_page_header();

	ctdl_mesg("goodbye");

	echo "<a href=\"index.php\">Log in again</a><BR>\n" ;

	bbs_page_footer();
	ctdl_end_session();
?>
