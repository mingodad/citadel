<?
/***********************************************
* File      :   maildir.php
* Project   :   Z-Push
* Descr     :   This backend is based on
*				'BackendDiff' which handles the
*				intricacies of generating
*				differentials from static
*				snapshots. This means that the
*				implementation here needs no
*				state information, and can simply
*				return the current state of the
*				messages. The diffbackend will
*				then compare the current state
*				to the known last state of the PDA
*				and generate change increments
*				from that.
*
* Created   :   01.10.2007
*
* © Zarafa Deutschland GmbH, www.zarafaserver.de
* This file is distributed under GPL v2.
* Consult LICENSE file for details
************************************************/

include_once('diffbackend.php');

// The is an improved version of mimeDecode from PEAR that correctly
// handles charsets and charset conversion
include_once('mimeDecode.php');

include_once('ctdlprotocol.php');

include_once('ctdlsession.php');


class BackendCitadel extends BackendDiff 
{
    /* Called to logon a user. These are the three authentication strings that you must
     * specify in ActiveSync on the PDA. Normally you would do some kind of password
     * check here. Alternatively, you could ignore the password here and have Apache
     * do authentication via mod_auth_*
     */
    function Logon($username, $domain, $password) {
	    debugLog ("Logging in.\n");
	    establish_citadel_session();
	    $usr = explode ('\\', $username);
///	    debugLog(print_r($usr, true));
	    debugLog($password);
	    if (count ($usr) == 2)
		    $username = $usr[1];
	    $ret = login_existing_user($username, $password);
	    if ($ret[0] != TRUE)
		    echo $ret[1];
	    return $ret[0];
    }

    /* Called directly after the logon. This specifies the client's protocol version
     * and device id. The device ID can be used for various things, including saving
     * per-device state information.
     * The $user parameter here is normally equal to the $username parameter from the
     * Logon() call. In theory though, you could log on a 'foo', and then sync the emails
     * of user 'bar'. The $user here is the username specified in the request URL, while the
     * $username in the Logon() call is the username which was sent as a part of the HTTP 
     * authentication.
     */    
    function Setup($user, $devid, $protocolversion) {
	    debugLog ("Setup\n");
        $this->_user = $user;
        $this->_devid = $devid;
        $this->_protocolversion = $protocolversion;
        return true;
    }
    
    /* Sends a message which is passed as rfc822. You basically can do two things
     * 1) Send the message to an SMTP server as-is
     * 2) Parse the message yourself, and send it some other way
     * It is up to you whether you want to put the message in the sent items folder. If you
     * want it in 'sent items', then the next sync on the 'sent items' folder should return
     * the new message as any other new message in a folder.
     */
    function SendMessage($rfc822) {
	    debugLog("SendMessage\n");
        // Unimplemented
        return true;
    }
    
    /* Should return a wastebasket folder if there is one. This is used when deleting
     * items; if this function returns a valid folder ID, then all deletes are handled
     * as moves and are sent to your backend as a move. If it returns FALSE, then deletes
     * are always handled as real deletes and will be sent to your importer as a DELETE
     */
    function GetWasteBasket() {
	    debugLog("GetWasteBasket");
        return "Trash";
    }
    
    /* Should return a list (array) of messages, each entry being an associative array
     * with the same entries as StatMessage(). This function should return stable information; ie
     * if nothing has changed, the items in the array must be exactly the same. The order of
     * the items within the array is not important though.
     *
     * The cutoffdate is a date in the past, representing the date since which items should be shown.
     * This cutoffdate is determined by the user's setting of getting 'Last 3 days' of e-mail, etc. If
     * you ignore the cutoffdate, the user will not be able to select their own cutoffdate, but all
     * will work OK apart from that.
     */
    function GetMessageList($folderid, $cutoffdate) {
	    debugLog("GetMessageList $folderid $cutoffdate");
///	    $this->moveNewToCur();

	    ctdl_goto ($folderid);
        
#        if($folderid != "root")
#           return false;
            
        // return stats of all messages in a dir. We can do this faster than
        // just calling statMessage() on each message; We still need fstat()
        // information though, so listing 10000 messages is going to be
        // rather slow (depending on filesystem, etc)
        
        // we also have to filter by the specified cutoffdate so only the 
        // last X days are retrieved. Normally, this would mean that we'd
        // have to open each message, get the Received: header, and check
        // whether that is in the filter range. Because this is much too slow, we
        // are depending on the creation date of the message instead, which should
        // normally be just about the same, unless you just did some kind of import.
        
	    $message = ctdl_msgs("","");
	    debugLog(print_r($message, true), true);
	    $messages = array();
	    
	    if ($message[0] > 0) for ($i=0; $i < $message[0]; $i ++)
	    {
		    $thismessage["id"] = $message[2][$i];
		    $thismessage["flags"] = 0;
		    $thismessage["flags"] |= 1; // 'seen' aka 'read' is the only flag we want to know about
		    array_push($messages, $thismessage);
		    
	    }
	    return $messages;
//        $messages = array();
//        $dirname = $this->getPath();
//        
//        $dir = opendir($dirname);
//        
//        if(!$dir)
//            return false;
//        
//        while($entry = readdir($dir)) {
//            if($entry{0} == ".")
//                continue;
//                
//            $message = array();
//            
//            $stat = stat("$dirname/$entry");
//
//            if($stat["mtime"] < $cutoffdate) {
//                // message is out of range for curoffdate, ignore it
//                continue;
//            }
//                            
//            $message["mod"] = $stat["mtime"];
//            
//            $matches = array();
//            
//            // Flags according to http://cr.yp.to/proto/maildir.html (pretty authoritative - qmail author's website)
//            if(!preg_match("/([^:]+):2,([PRSTDF]*)/",$entry,$matches))
//                continue;
//            $message["id"] = $matches[1];
//            $message["flags"] = 0;
//            
//            if(strpos($matches[2],"S") !== false) {
//                $message["flags"] |= 1; // 'seen' aka 'read' is the only flag we want to know about
//            }
//            
//            array_push($messages, $message);
//        }
//        
//        return $messages;
    }
    
    /* This function is analogous to GetMessageList. In simple implementations like this one,
     * you probably just return one folder.
     */
    function GetFolderList() {
	    $folders = array();
	    debugLog("GetFolderList");
	    $ret = ctdl_knrooms(); /// TODO: should we just get the rooms with new messages in them? No.
	    if ($ret[0])
	    {
		    $fldr = $ret[1];
		    foreach ($fldr as $folder)
		    {      // hide contacts and calendar here... TODO: do we realy need to?
			    if (($folder['name'] != 'Calendar') && ($folder['name'] != 'Contacts'))
			    {
				    $folders[] = array("id"     => $folder['name'], 
						       "parent" => $folder['floor'], 
						       "mod"    => "Inbox");
				    
			    }
		    }
		    return $folders;
	    }
	    else return false;
	    
        
///        $inbox = array();
///        $inbox["id"] = "root";
///        $inbox["parent"] = "0";
///        $inbox["mod"] = "Inbox";
///        
///        $folders[]=$inbox;
///        
///        $sub = array();
///        $sub["id"] = "sub";
///        $sub["parent"] = "root";
///        $sub["mod"] = "Sub";
///        
/////        $folders[]=$sub;
///        
///        return $folders;
    }
    
    /* GetFolder should return an actual SyncFolder object with all the properties set. Folders
     * are pretty simple really, having only a type, a name, a parent and a server ID. 
     */
    function GetFolder($id) {
	    debugLog("GetFolder $id");
	    $ret = ctdl_goto ($id);
//	    debugLog(print_r($ret, true));
	    $box = new SyncFolder();
	    $box->serverid = $id;
	    $box->parentid = $ret['floorid'];
	    $box->displayname = $ret['roomname'];
	    switch ($ret['defaultview'])
	    {
	    case VIEW_BBS:
		    $box->type = SYNC_FOLDER_TYPE_OTHER;
		    break;
	    case VIEW_MAILBOX:
		    $box->type = SYNC_FOLDER_TYPE_INBOX;
		    break;
	    case VIEW_ADDRESSBOOK:
		    $box->type = SYNC_FOLDER_TYPE_OTHER;
		    break;
	    case VIEW_CALENDAR:
		    $box->type = SYNC_FOLDER_TYPE_OTHER;
		    break;
	    case VIEW_TASKS:
		    $box->type = SYNC_FOLDER_TYPE_OTHER;
		    break;
	    case VIEW_NOTES:
		    $box->type = SYNC_FOLDER_TYPE_OTHER;
		    break;
	    }
	    return $box;
//        if($id == "root") {
//            $inbox = new SyncFolder();
//            
//            $inbox->serverid = $id;
//            $inbox->parentid = "0"; // Root
//            $inbox->displayname = "Inbox";
//            $inbox->type = SYNC_FOLDER_TYPE_INBOX;
//            
//            return $inbox;
//        } else if($id = "sub") {
//            $inbox = new SyncFolder();
//            $inbox->serverid = $id;
//            $inbox->parentid = "root";
//            $inbox->displayname = "Sub";
//            $inbox->type = SYNC_FOLDER_TYPE_OTHER;
//            
//            return $inbox;
//        } else {
//            return false;
//        }
    }
    
    /* Return folder stats. This means you must return an associative array with the
     * following properties:
     * "id" => The server ID that will be used to identify the folder. It must be unique, and not too long
     *         How long exactly is not known, but try keeping it under 20 chars or so. It must be a string.
     * "parent" => The server ID of the parent of the folder. Same restrictions as 'id' apply.
     * "mod" => This is the modification signature. It is any arbitrary string which is constant as long as
     *          the folder has not changed. In practice this means that 'mod' can be equal to the folder name
     *          as this is the only thing that ever changes in folders. (the type is normally constant)
     */
    function StatFolder($id) {
debugLog("Statfolder $id");
        $folder = $this->GetFolder($id);
        
        $stat = array();
        $stat["id"] = $id;
        $stat["parent"] = $folder->parentid;
        $stat["mod"] = $folder->displayname;
        
        return $stat;
    }

    /* Should return attachment data for the specified attachment. The passed attachment identifier is
     * the exact string that is returned in the 'AttName' property of an SyncAttachment. So, you should
     * encode any information you need to find the attachment in that 'attname' property.
     */    
    function GetAttachmentData($attname) {
debugLog("GetAttachmentData");
        list($id, $part) = explode(":", $attname);
        
        $fn = $this->findMessage($id);
        
        // Parse e-mail
        $rfc822 = file_get_contents($this->getPath() . "/$fn");
        
        $message = Mail_mimeDecode::decode(array('decode_headers' => true, 'decode_bodies' => true, 'include_bodies' => true, 'input' => $rfc822, 'crlf' => "\n", 'charset' => 'utf-8'));
        return $message->parts[$part]->body;
    }

    /* StatMessage should return message stats, analogous to the folder stats (StatFolder). Entries are:
     * 'id' 	=> Server unique identifier for the message. Again, try to keep this short (under 20 chars)
     * 'flags' 	=> simply '0' for unread, '1' for read
     * 'mod'	=> modification signature. As soon as this signature changes, the item is assumed to be completely
     *             changed, and will be sent to the PDA as a whole. Normally you can use something like the modification
     *             time for this field, which will change as soon as the contents have changed.
     */
     
    function StatMessage($folderid, $id) {
	    debugLog("StatMessage $folderid $id");
	    return array ("id" => "$id", "flags" => 0, "mod", "12345");
//
//        $dirname = $this->getPath();
//        $fn = $this->findMessage($id);
//        if(!$fn)
//            return false;
//
//        $stat = stat("$dirname/$fn");
//
//        $entry = array();
//        $entry["id"] = $id;
//        $entry["flags"] = 0;
//
//        if(strpos($fn,"S"))
//            $entry["flags"] |= 1;
//        $entry["mod"] = $stat["mtime"];
//                
//        return $entry;
    }
    
    /* GetMessage should return the actual SyncXXX object type. You may or may not use the '$folderid' parent folder
     * identifier here.
     * Note that mixing item types is illegal and will be blocked by the engine; ie returning an Email object in a 
     * Tasks folder will not do anything. The SyncXXX objects should be filled with as much information as possible, 
     * but at least the subject, body, to, from, etc.
     */
    function GetMessage($folderid, $id) {
	    debugLog("GetMessge $folderid $id");
#        if($folderid != 'root')
#            return false;
            
//        $fn = $this->findMessage($id);

        // Get flags, etc
        $stat = $this->StatMessage($folderid, $id);
        
        // Parse e-mail
        $rfc822 = $this->findMessage($id);
#file_get_contents($this->getPath() . "/" . $fn);
        debugLog("-------------------".print_r($rfc822, true));
	$params = array('decode_headers' => true, 'decode_bodies' => true, 'include_bodies' => true,  'crlf' => "\r\n", 'charset' => 'utf-8');
	$decoder = new Mail_mimeDecode($rfc822);
        $message = $decoder->decode();

	debugLog(print_r($message, true));
        $output = new SyncMail();

        $output->body = str_replace("\n", "\r\n", $this->getBody($message));
        $output->bodysize = strlen($output->body);
        $output->bodytruncated = 0;
        $output->datereceived = $this->parseReceivedDate($message->headers["received"][0]);
        $output->displayto = $message->headers["to"];
        $output->importance = $message->headers["x-priority"];
        $output->messageclass = "IPM.Note";
        $output->subject = $message->headers["subject"];
        $output->read = $stat["flags"];
        $output->to = $message->headers["to"];
        $output->cc = $message->headers["cc"];
        $output->from = $message->headers["from"];
        $output->reply_to = isset($message->headers["reply-to"]) ? $message->headers["reply-to"] : null;

        // Attachments are only searched in the top-level part
        $n = 0;
        if(isset($message->parts)) {
            foreach($message->parts as $part) {
                if($part->ctype_primary == "application") {
                    $attachment = new SyncAttachment();
                    $attachment->attsize = strlen($part->body);
                    
                    if(isset($part->d_parameters['filename']))
                        $attname = $part->d_parameters['filename'];
                    else if(isset($part->ctype_parameters['name']))
                        $attname = $part->ctype_parameters['name'];
                    else if(isset($part->headers['content-description']))
                        $attname = $part->headers['content-description'];
                    else $attname = "unknown attachment";
                    
                    $attachment->displayname = $attname;
                    $attachment->attname = $id . ":" . $n;
                    $attachment->attmethod = 1;
                    $attachment->attoid = isset($part->headers['content-id']) ? $part->headers['content-id'] : "";
                    
                    array_push($output->attachments, $attachment);
                }
                $n++;
            }
        }
        
        return $output;
    }
    
    /* This function is called when the user has requested to delete (really delete) a message. Usually
     * this means just unlinking the file its in or somesuch. After this call has succeeded, a call to
     * GetMessageList() should no longer list the message. If it does, the message will be re-sent to the PDA
     * as it will be seen as a 'new' item. This means that if you don't implement this function, you will
     * be able to delete messages on the PDA, but as soon as you sync, you'll get the item back
     */
    function DeleteMessage($folderid, $id) {
	    debugLog("DeleteMessage");
        if($folderid != 'root')
            return false;
            
        $fn = $this->findMessage($id);

        if(!$fn)
            return true; // success because message has been deleted already

        
        if(!unlink($this->getPath() . "/$fn")) {
            return true; // success - message may have been deleted in the mean time (since findMessage)
        }

        return true;
    }
    
    /* This should change the 'read' flag of a message on disk. The $flags
     * parameter can only be '1' (read) or '0' (unread). After a call to
     * SetReadFlag(), GetMessageList() should return the message with the
     * new 'flags' but should not modify the 'mod' parameter. If you do
     * change 'mod', simply setting the message to 'read' on the PDA will trigger
     * a full resync of the item from the server
     */
    function SetReadFlag($folderid, $id, $flags) {
	    debugLog("SetReadFlag");
        if($folderid != 'root')
            return false;
            
        $fn = $this->findMessage($id);
        
        if(!$fn)
            return true; // message may have been deleted
        
        if(!preg_match("/([^:]+):2,([PRSTDF]*)/",$fn,$matches))
            return false;

        // remove 'seen' (S) flag            
        if(!$flags) {
            $newflags = str_replace("S","",$matches[2]);
        } else {
            // make sure we don't double add the 'S' flag
            $newflags = str_replace("S","",$matches[2]) . "S";
        }
        
        $newfn = $matches[1] . ":2," . $newflags;
        // rename if required
        if($fn != $newfn) 
            rename($this->getPath() ."/$fn", $this->getPath() . "/$newfn");
        
        return true;
    }
    
    /* This function is called when a message has been changed on the PDA. You should parse the new
     * message here and save the changes to disk. The return value must be whatever would be returned
     * from StatMessage() after the message has been saved. This means that both the 'flags' and the 'mod'
     * properties of the StatMessage() item may change via ChangeMessage().
     * Note that this function will never be called on E-mail items as you can't change e-mail items, you
     * can only set them as 'read'.
     */
    function ChangeMessage($folderid, $id, $message) {
	    debugLog("ChangeMessage");
        return false;
    }
    
    /* This function is called when the user moves an item on the PDA. You should do whatever is needed
     * to move the message on disk. After this call, StatMessage() and GetMessageList() should show the items
     * to have a new parent. This means that it will disappear from GetMessageList() will not return the item
     * at all on the source folder, and the destination folder will show the new message
     */
    function MoveMessage($folderid, $id, $newfolderid) {
	    debugLog("MoveMessage");
        return false;
    }

    // ----------------------------------------
    // maildir-specific internals
    
    function findMessage($id) {
	    debugLog("findMessage $id");
        // We could use 'this->_folderid' for path info but we currently
        // only support a single INBOX. We also have to use a glob '*'
        // because we don't know the flags of the message we're looking for.

	    $msg = ctdl_fetch_message_rfc822($id);
	    if ($msg[0])
		    return $msg[1];
	    else
		    return false;
//        $dirname = $this->getPath();
//        $dir = opendir($dirname);
//        
//        while($entry = readdir($dir)) {
//            if(strpos($entry,$id) === 0)
//                return $entry;
//        }
//        return false; // not found
    }
    
    /* Parse the message and return only the plaintext body
     */
    function getBody($message) {
	    debugLog("getBody -> $message <-");
        $body = "";
        $htmlbody = "";
        
        $this->getBodyRecursive($message, "plain", $body);
        
        if(!isset($body) || $body === "") {
            $this->getBodyRecursive($message, "html", $body);
            // HTML conversion goes here
        }
        
        return $body;
    }
    
    // Get all parts in the message with specified type and concatenate them together, unless the
    // Content-Disposition is 'attachment', in which case the text is apparently an attachment
    function getBodyRecursive($message, $subtype, &$body) {
	    debugLog("GetBodyRecursive $subtype".print_r($message, true));
        if(strcasecmp($message->ctype_primary,"text")==0 && strcasecmp($message->ctype_secondary,$subtype)==0 && isset($message->body))
            $body .= $message->body;
        
        if(strcasecmp($message->ctype_primary,"multipart")==0) {
            foreach($message->parts as $part) {
                if(!isset($part->disposition) || strcasecmp($part->disposition,"attachment"))  {
                    $this->getBodyRecursive($part, $subtype, $body);
                }
            }
        }
    }

    function parseReceivedDate($received) {
	    debugLog("parseRecivedDate");
	    $pos = strpos($received, ";");
        if(!$pos)
            return false;
            
        $datestr = substr($received, $pos+1);
        $datestr = ltrim($datestr);
        
        return strtotime($datestr);
    }
    
    /* moves everything in Maildir/new/* to Maildir/cur/
     */
    function moveNewToCur() {
	    debugLog("moveNewToCur");
        $newdirname = MAILDIR_BASE . "/" . $this->_user . "/" . MAILDIR_SUBDIR . "/new";
        
        $newdir = opendir($newdirname);
        
        while($newentry = readdir($newdir)) {
            if($newentry{0} == ".")
                continue;
                
            // link/unlink == move. This is the way to move the message according to cr.yp.to
            link($newdirname . "/" . $newentry, $this->getPath() . "/" . $newentry . ":2,");
            unlink($newdirname . "/" . $newentry);
        }
    }
    
    /* The path we're working on
     */
    function getPath() {
	    debugLog("GetPath");
        return MAILDIR_BASE . "/" . $this->_user . "/" . MAILDIR_SUBDIR . "/cur";
    }
};


?>