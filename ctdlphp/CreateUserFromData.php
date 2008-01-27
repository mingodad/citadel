<?PHP

define('CITADEL_DEBUG_HTML', FALSE);

if ($_SERVER["argc"] < 3)
	die("need at least two params Call me CreateUserFromData.php testuser opensesame.\n");


$user = $_SERVER["argv"][1];
$password = $_SERVER["argv"][1];
echo "----------------------- Creating User $user -------------------\n";
$vcardname = $user."/vcard.txt";
if (! is_array(stat($vcardname)))
	die("\nCouldn't find vcard in $vcardname\n");
	
$configname= $user."/config.txt";
if (!is_array(stat($configname)))
	die("\nCouldn't find users config in $configname\n");


$vcardfh = fopen($vcardname, "r");
$vcard = fread($vcardfh, filesize($vcardname));
fclose($vcardfh);

$configfh = fopen($configname, "r");
$config = fread($configfh, filesize($configname));
fclose($configfh);


include "ctdlsession.php";
include "ctdlprotocol.php";
include "ctdlelements.php";

//define(CITADEL_DEBUG_HTML, FALSE);
establish_citadel_session();
create_new_user($user, $password);

ctdl_goto("My Citadel Config");
list($num_msgs, $response, $msgs) = ctdl_msgs("", "");

$Webcit_Preferences = array();
$Webcit_PrefMsgid = 0;
if ($num_msgs > 0) foreach ($msgs as $msgnum) {
	print_r($msgnum);

	// Fetch the message from the server
	list($ok, $response, $fields) = ctdl_fetch_message($msgnum);

	// Bail out gracefully if the message isn't there.
	if (!$ok) {
		echo "Error: " . $response . "\n";
		return false;
	}
	if (isset($fields['part']))
	{
		$parts = explode('|', $fields['part']);
		print_r($parts);
		if ($parts[4]=="text/x-vcard")
			list($size, $OldVcard) = download_attachment($msgnum, $result[2]);
		else
			ctdl_dele($msgnum);
	}
	else if ($fields['subj'] == "__ WebCit Preferences __")
	{
		$Webcit_PrefMsgid = $msgnum;
		$Webcit_Preferences = $fields;
	} 
}

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
$entarray=array();
$entarray['post']=1;
$entarray['format_type']=FMT_RFC822;
enter_message_0($entarray, "text/x-vcard; charset=UTF-8", $vcard);


$Webcit_Preferences=$config;
"
iconbar|ib_displayas=0,ib_logo=0,ib_summary=1,ib_inbox=1,ib_calendar=1,ib_contacts=1,ib_notes=1,ib
_tasks=1,ib_rooms=1,ib_users=2,ib_chat=1,ib_advanced=1,ib_logoff=0,ib_citadel=1
roomlistview|rooms
emptyfloors|yes
weekstart|0
use_sig|no
startpage|dotskip&room=_BASEROOM_
signature|
daystart|8
floordiv_expanded|
current_iconbar|0
calhourformat|24
dayend|17
";

if ($Webcit_PrefMsgid != '0')
{
	ctdl_dele($Webcit_PrefMsgid);
	
}
$entarray=array();
$entarray['post']=1;
$entarray['format_type']=FMT_FIXED;
$entarray['subject'] = "__ WebCit Preferences __";
$entarray['anon_flag'] = '0';
enter_message_0($entarray,
		"", 
		"somevar|somevalue\r\n".$Webcit_Preferences);

ctdl_end_session();

?>