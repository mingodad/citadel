<?PHP
	include "ctdlheader.php";

	bbs_page_header();

	echo <<<LITERAL

<h1>Goodbye</h1>

You are being logged out.

<a href="index.php">Log in again</a><BR>

LITERAL;

	bbs_page_footer();
	ctdl_end_session();

?>

