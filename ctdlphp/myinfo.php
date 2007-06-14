<?PHP
$vcard="";
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
list($num_msgs, $response, $msgs) = ctdl_msgs("", "");

echo "num_msgs: " . $num_msgs . "<BR>\n" ;
echo "response: " . htmlspecialchars($response) . "<BR>\n" ;

if ($num_msgs > 0) foreach ($msgs as $msgnum) {
	print_r($msgnum);
	$result = get_message_partlist($msgnum);
	if (is_array($result) &&
	    ($result[4]=="text/x-vcard"))
	{
		list($size, $vcard) = download_attachment($msgnum, $result[2]);
	}
}

echo "</TABLE>\n <pre>\n".$vcard."</pre>";

echo "putting it back in!";

$vcard = 
"begin:vcard
n:Surename;CName;mitte;Mrs;IV
title:master
fn:CName Surename
org:citadel.org
adr:blarg;Road To nowhere ;10;Metropolis;NRW;12345;Country
tel;home:888888888
tel;work:99999999999
email;internet:user@samplecitadel.org
email;internet:me@samplecitadel.org
email;internet:myself@samplecitadel.org
email;internet:i@samplecitadel.org
FBURL;PREF:http://samplecitadel.org/Cname_Lastname.vfb
UID:Citadel vCard: personal card for Cname Lastname at samplecitadel.org
end:vcard
";
preg_replace('/Sir/', 'Mrs',$vcard);
echo "Now its: <pre>\n".$vcard."</pre>";
$entearray=array();
$entarray['post']=1;
$entarray['format_type']=FMT_RFC822;
enter_message_0($entarray, "text/x-vcard; charset=UTF-8", $vcard);
?>

<BR>Sample links<BR>
<a href="welcome.php">Page One</a><BR>
<a href="page3.php">Page Three</a><BR>

<?PHP
	bbs_page_footer();
?>
