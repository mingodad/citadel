The totally incomplete guide to Citadel internals
-----------------------------------------------------
-----------------------------------------------------

Citadel has evolved quite a bit since its early days, and the data structures
have evolved with it.  This document provides a rough overview of how the
system works internally.  For details you're going to have to dig through the
code, but this'll get you started. 


DATABASE TABLES
---------------
As you probably already know by now, Citadel uses a group of tables stored
with a record manager (usually Berkeley DB).  Since we're using a record
manager rather than a relational database, all record structures are managed
by Citadel.  Here are some of the tables we keep on disk:


USER RECORDS
------------
This table contains all user records.  It's indexed by
user name (translated to lower case for indexing purposes).  The records in
this file look something like this:

struct ctdluser {                       /* User record                      */
        int version;                    /* Cit vers. which created this rec */
        uid_t uid;                      /* Associate with a unix account?   */
        char password[32];              /* password (for Citadel-only users)*/
        unsigned flags;                 /* See US_ flags below              */
        long timescalled;               /* Total number of logins           */
        long posted;                    /* Number of messages posted (ever) */
        CIT_UBYTE axlevel;              /* Access level                     */
        long usernum;                   /* User number (never recycled)     */
        time_t lastcall;                /* Last time the user called        */
        int USuserpurge;                /* Purge time (in days) for user    */
        char fullname[64];              /* Name for Citadel messages & mail */
};
 
 Most fields here should be fairly self-explanatory.  The ones that might
deserve some attention are:

 - *uid* -- if uid is not the same as the *unix uid* Citadel is running as, then the
   account is assumed to belong to the user on the underlying Unix system with
   that uid.  This allows us to require the user's OS password instead of having
   a separate Citadel password.
 
 - *usernum* -- these are assigned sequentially, and **NEVER REUSED**. This is
  important because it allows us to use this number in other data structures
  without having to worry about users being added/removed later on, as you'll
  see later in this document.
 
 
ROOM RECORDS
------------
These are room records.  There is a room record for every room on the
system, public or private or mailbox.  It's indexed by room name (also in
lower case for easy indexing) and it contains records which look like this:

    struct ctdlroom {
        char QRname[ROOMNAMELEN];       /* Name of room                     */
        char QRpasswd[10];              /* Only valid if it's a private rm  */
        long QRroomaide;                /* User number of room aide         */
        long QRhighest;                 /* Highest message NUMBER in room   */
        time_t QRgen;                   /* Generation number of room        */
        unsigned QRflags;               /* See flag values below            */
        char QRdirname[15];             /* Directory name, if applicable    */
        long QRinfo;                    /* Info file update relative to msgs*/
        char QRfloor;                   /* Which floor this room is on      */
        time_t QRmtime;                 /* Date/time of last post           */
        struct ExpirePolicy QRep;       /* Message expiration policy        */
        long QRnumber;                  /* Globally unique room number      */
        char QRorder;                   /* Sort key for room listing order  */
        unsigned QRflags2;              /* Additional flags                 */
        int QRdefaultview;              /* How to display the contents      */
    };

Again, mostly self-explanatory.  Here are the interesting ones:
 
*QRnumber* is a globally unique room ID, while QRgen is the "generation number"
of the room (it's actually a timestamp).  The two combined produce a unique
value which identifies the room.  The reason for two separate fields will be
explained below when we discuss the visit table.  For now just remember that
*QRnumber* remains the same for the duration of the room's existence, and QRgen
is timestamped once during room creation but may be restamped later on when
certain circumstances exist.

FLOORTAB
--------
Floors.  This is so simplistic it's not worth going into detail about, except
to note that we keep a reference count of the number of rooms on each floor.
 
MSGLISTS
--------
Each record in this table consists of a bunch of message  numbers
which represent the contents of a room.  A message can exist in more than one
room (for example, a mail message with multiple recipients -- 'single instance
store').  This table is never, ever traversed in its entirety.  When you do
any type of read operation, it fetches the msglist for the room you're in
(using the room's ID as the index key) and then you can go ahead and read
those messages one by one.

Each room is basically just a list of message numbers.  Each time
we enter a new message in a room, its message number is appended to the end
of the list.  If an old message is to be expired, we must delete it from the
message base.  Reading a room is just a matter of looking up the messages
one by one and sending them to the client for display, printing, or whatever.
 

VISIT
-----
This is the tough one.  Put on your thinking cap and grab a fresh cup of
coffee before attempting to grok the visit table.
 
This table contains records which establish the relationship between users
and rooms.  Its index is a hash of the user and room combination in question.
When looking for such a relationship, the record in this table can tell the
server things like "this user has zapped this room," "this user has access to
this private room," etc.  It's also where we keep track of which messages
the user has marked as "old" and which are "new" (which are not necessarily
contiguous; contrast with older Citadel implementations which simply kept a
"last read" pointer).
 

Here's what the records look like:
 
    struct visit {
        long v_roomnum;
        long v_roomgen;
        long v_usernum;
        long v_lastseen;
        unsigned int v_flags;
        char v_seen[SIZ];
        int v_view;
    };

    #define V_FORGET        1       /* User has zapped this room        */
    #define V_LOCKOUT       2       /* User is locked out of this room  */
    #define V_ACCESS        4       /* Access is granted to this room   */
 
This table is indexed by a concatenation of the first three fields.  Whenever
we want to learn the relationship between a user and a room, we feed that
data to a function which looks up the corresponding record.  The record is
designed in such a way that an "all zeroes" record (which is what you get if
the record isn't found) represents the default relationship.
 
With this data, we now know which private rooms we're allowed to visit: if
the *V_ACCESS* bit is set, the room is one which the user knows, and it may
appear in his/her known rooms list.  Conversely, we also know which rooms the
user has zapped: if the *V_FORGET* flag is set, we relegate the room to the
zapped list and don't bring it up during new message searches.  It's also
worth noting that the *V_LOCKOUT* flag works in a similar way to administratively
lock users out of rooms.
 
Implementing the "cause all users to forget room" command, then, becomes very
simple: we simply change the generation number of the room by putting a new
timestamp in the *QRgen* field.  This causes all relevant visit records to
become irrelevant, because they appear to point to a different room.  At the
same time, we don't lose the messages in the room, because the msglists table
is indexed by the room number (*QRnumber*), which never changes.
 
*v_seen* contains a string which represents the set of messages in this room
which the user has read (marked as 'seen' or 'old').  It follows the same
syntax used by IMAP and NNTP.  When we search for new messages, we simply
return any messages that are in the room that are **not** represented by this
set.  Naturally, when we do want to mark more messages as seen (or unmark
them), we change this string.  Citadel BBS client implementations are naive
and think linearly in terms of "everything is old up to this point," but IMAP
clients want to have more granularity.


DIRECTORY
---------
This table simply maps Internet e-mail addresses to Citadel network addresses
for quick lookup.  It is generated from data in the Global Address Book room.

USETABLE
--------
This table keeps track of message ID's of messages arriving over a network,
to prevent duplicates from being posted if someone misconfigures the network
and a loop is created.  This table goes unused on a non-networked Citadel.

THE MESSAGE STORE
-----------------
This is where all message text is stored.  It's indexed by message number:
give it a number, get back a message.  Messages are numbered sequentially, and
the message numbers are never reused.
 
We also keep a "metadata" record for each message.  This record is also stored
in the msgmain table, using the index (0 - msgnum).  We keep in the metadata
record, among other things, a reference count for each message.  Since a
message may exist in more than one room, it's important to keep this reference
count up to date, and to delete the message from disk when the reference count
reaches zero.
 
#Here's the format for the message itself:

 - Each message begins with an 0xFF 'start of message' byte.
 
 - The next byte denotes whether this is an anonymous message.  The codes
   available are *MES_NORMAL*, *MES_ANON*, or *MES_AN2* (defined in citadel.h).
 
 - The third byte is a "message type" code.  The following codes are defined:
  - 0 - "Traditional" Citadel format.  Message is to be displayed "formatted."
  - 1 - Plain pre-formatted ASCII text (otherwise known as text/plain)
  - 4 - MIME formatted message.  The text of the message which follows is
        expected to begin with a "Content-type:" header.
 
 - After these three opening bytes, the remainder of
   the message consists of a sequence of character strings.  Each string
   begins with a type byte indicating the meaning of the string and is
   ended with a null.  All strings are printable ASCII: in particular,
   all numbers are in ASCII rather than binary.  This is for simplicity,
   both in implementing the system and in implementing other code to
   work with the system.  For instance, a database driven off Citadel archives
   can do wildcard matching without worrying about unpacking binary data such
   as message ID's first.  To provide later downward compatability
   all software should be written to IGNORE fields not currently defined.


#The type bytes currently defined are:


| BYTE  |       Enum        | NW   | Mnemonic       |  Enum / Comments
|-------|-------------------|------|----------------|---------------------------------------------------------
| A     |    eAuthor        | from | Author         |  *eAuthor*
|       |                   |      |                |  Name of originator of message.
| B     |    eBig\_message  |      | Big message    |  *eBig\_message*
|       |                   |      |                |  This is a flag which indicates that the message is
|       |                   |      |                |  big, and Citadel is storing the body in a separate
|       |                   |      |                |  record.  You will never see this field because the
|       |                   |      |                |  internal API handles it.
| C     |    eRemoteRoom    |      | RemoteRoom     |  *eRemoteRoom*
|       |                   |      |                |  when sent via Citadel Networking, this is the room
|       |                   |      |                |  its going to be put on the remote site.
| D     |    eDestination   |      | Destination    |  *eDestination*
|       |                   |      |                |  Contains name of the system this message should
|       |                   |      |                |  be sent to, for mail routing (private mail only).
| E     |    eExclusiveID   | exti | Exclusive ID   |  *eExclusiveID*
|       |                   |      |                |  A persistent alphanumeric Message ID used for
|       |                   |      |                |  network replication.  When a message arrives that
|       |                   |      |                |  contains an Exclusive ID, any existing messages which
|       |                   |      |                |  contain the same Exclusive ID and are *older* than this
|       |                   |      |                |  message should be deleted.  If there exist any messages
|       |                   |      |                |  with the same Exclusive ID that are *newer*, then this
|       |                   |      |                |  message should be dropped.
| F     |    erFc822Addr    | rfca | rFc822 address |  *erFc822Addr*
|       |                   |      |                |  For Internet mail, this is the delivery address of the
|       |                   |      |                |  message author.
| H     |    eHumanNode     | hnod | Human node name|  *eHumanNode*
|       |                   |      |                |  Human-readable name of system message originated on.
| I     |    emessageId     | msgn | Message ID     |  *emessageId*
|       |                   |      |                |  An RFC822-compatible message ID for this message.
| J     |    eJournal       | jrnl | Journal        |  *eJournal*
|       |                   |      |                |  The presence of this field indicates that the message
|       |                   |      |                |  is disqualified from being journaled, perhaps because
|       |                   |      |                |  it is itself a journalized message and we wish to
|       |                   |      |                |  avoid double journaling.
| K     |    eReplyTo       | rep2 | Reply-To       |  *eReplyTo*
|       |                   |      |                |  the Reply-To header for mailinglist outbound messages
| L     |    eListID        | list | List-ID        |  *eListID*
|       |                   |      |                |  Mailing list identification, as per RFC 2919
| M     |    eMesageText    | text | Message Text   |  *eMesageText*
|       |                   |      |                |  Normal ASCII, newlines seperated by CR's or LF's,
|       |                   |      |                |  null terminated as always.
| N     |    eNodeName      | node | Nodename       |  *eNodeName*
|       |                   |      |                |  Contains node name of system message originated on.
| O     |    eOriginalRoom  | room | Room           |  *eOriginalRoom* - Room of origin.
| P     |    eMessagePath   | path | Path           |  *eMessagePath*
|       |                   |      |                |  Complete path of message, as in the UseNet news
|       |                   |      |                |  standard.  A user should be able to send Internet mail
|       |                   |      |                |  to this path. (Note that your system name will not be
|       |                   |      |                |  tacked onto this until you're sending the message to
|       |                   |      |                |  someone else)
| R     |    eRecipient     | rcpt | Recipient      |  *eRecipient* - Only present in Mail messages.
| S     |    eSpecialField  | spec | Special field  |  *eSpecialField*
|       |                   |      |                |  Only meaningful for messages being spooled over a
|       |                   |      |                |  network.  Usually means that the message isn't really
|       |                   |      |                |  a message, but rather some other network function:
|       |                   |      |                |  -> "S" followed by "FILE" (followed by a null, of
|       |                   |      |                |     course) means that the message text is actually an
|       |                   |      |                |     IGnet/Open file transfer.  (OBSOLETE)
|       |                   |      |                |  -> "S" followed by "CANCEL" means that this message
|       |                   |      |                |     should be deleted from the local message base once
|       |                   |      |                |     it has been replicated to all network systems.
| T     |    eTimestamp     | time | date/Time      |  *eTimestamp*
|       |                   |      |                |  Unix timestamp containing the creation date/time of
|       |                   |      |                |  the message.
| U     |    eMsgSubject    | subj | sUbject        |  *eMsgSubject* - Optional.
|       |                   |      |                |  Developers may choose whether they wish to
|       |                   |      |                |  generate or display subject fields.
| V     |    eenVelopeTo    | nvto | enVelope-to    |  *eenVelopeTo*
|       |                   |      |                |  The recipient specified in incoming SMTP messages.
| W     |    eWeferences    | wefw | Wefewences     |  *eWeferences*
|       |                   |      |                |  Previous message ID's for conversation threading.  When
|       |                   |      |                |  converting from RFC822 we use References: if present, or
|       |                   |      |                |  In-Reply-To: otherwise.
|       |                   |      |                |  (Who in extnotify spool messages which don't need to know
|       |                   |      |                |  other message ids)
| Y     |    eCarbonCopY    | cccc | carbon copY    |  *eCarbonCopY*
|       |                   |      |                |  Optional, and only in Mail messages.
| %     |    eHeaderOnly    | nhdr | oNlyHeader     |  we will just be sending headers. for the Wire protocol only.
| %     |    eFormatType    | type | type           |  type of citadel message: (Wire protocol only)
|       |                   |      |                |     FMT\_CITADEL     0   Citadel vari-format (proprietary) 
|       |                   |      |                |     FMT\_FIXED       1   Fixed format (proprietary)
|       |                   |      |                |     FMT\_RFC822      4   Standard (headers are in M field)
| %     |    eMessagePart   | part | emessagePart   |  *eMessagePart* is the id of this part in the mime hierachy
| %     |	 eSubFolder     | suff | eSubFolder     |  descend into a mime sub container
| %     | 	 ePevious       | pref | ePevious       |  exit a mime sub container
| 0     |    eErrorMsg      |      | Error          |  *eErrorMsg*
|       |                   |      |                |  This field is typically never found in a message on
|       |                   |      |                |  disk or in transit.  Message scanning modules are
|       |                   |      |                |  expected to fill in this field when rejecting a message
|       |                   |      |                |  with an explanation as to what happened (virus found,
|       |                   |      |                |  message looks like spam, etc.)
| 1     |    eSuppressIdx   |      | suppress index |  *eSuppressIdx*
|       |                   |      |                |  The presence of this field indicates that the message is
|       |                   |      |                |  disqualified from being added to the full text index.
| 2     |    eExtnotify     |      | extnotify      |  *eExtnotify* - Used internally by the serv_extnotify module.
| 3     |    eVltMsgNum     |      | msgnum         |  *eVltMsgNum*
|       |                   |      |                |  Used internally to pass the local message number in the
|       |                   |      |                |  database to after-save hooks.  Discarded afterwards.

EXAMPLE
-------
Let *<FF>* be a *0xFF* byte, and *<0>* be a null *(0x00)* byte.  Then a message
which prints as...

    Apr 12, 1988 23:16 From Test User In Network Test> @lifesys (Life Central)
    Have a nice day!

might be stored as...

    <FF><40><0>I12345<0>Pneighbor!lifesys!test_user<0>T576918988<0>    (continued)
    -----------|Mesg ID#|--Message Path---------------|--Date------
    
    AThe Test User<0>ONetwork Test<0>Nlifesys<0>HLife Central<0>MHave a nice day!<0>
    |-----Author-----|-Room name-----|-nodename-|Human Name-|--Message text-----

Weird things can happen if fields are missing, especially if you use the
networker.  But basically, the date, author, room, and nodename may be in any
order.  But the leading fields and the message text must remain in the same
place.  The H field looks better when it is placed immediately after the N
field.


EUID (EXCLUSIVE MESSAGE ID'S)
-----------------------------
This is where the groupware magic happens.  Any message in any room may have
a field called the Exclusive message *ID*, or *EUID*.  We keep an index in the
table *CDB_EUIDINDEX* which knows the message number of any item that has an
*EUID*.  This allows us to do two things:
 
 - If a subsequent message arrives with the same *EUID*, it automatically
   *deletes* the existing one, because the new one is considered a replacement
   for the existing one.
 - If we know the *EUID* of the item we're looking for, we can fetch it by *EUID*
   and get the most up-to-date version, even if it's been updated several times.

This functionality is made more useful by server-side hooks.  For example,
when we save a vCard to an address book room, or an iCalendar item to a
calendar room, our server modules detect this condition, and automatically set
the *EUID* of the message to the *UUID* of the *vCard* or *iCalendar* item.
Therefore when you save an updated version of an address book entry or
a calendar item, the old one is automatically deleted.

NETWORKING (REPLICATION)
------------------------
Citadel nodes network by sharing one or more rooms. Any Citadel node
can choose to share messages with any other Citadel node, through the sending
of spool files.  The sending system takes all messages it hasn't sent yet, and
spools them to the recieving system, which posts them in the rooms.

The *EUID* discussion above is extremely relevant, because *EUID* is carried over
the network as well, and the replacement rules are followed over the network
as well.  Therefore, when a message containing an *EUID* is saved in a networked
room, it replaces any existing message with the same *EUID* *on every node in
the network*.

Complexities arise primarily from the possibility of densely connected
networks: one does not wish to accumulate multiple copies of a given
message, which can easily happen.  Nor does one want to see old messages
percolating indefinitely through the system.

This problem is handled by keeping track of the path a message has taken over
the network, like the UseNet news system does.  When a system sends out a
message, it adds its own name to the bang-path in the *<P>* field of the
message.  If no path field is present, it generates one.
   
With the path present, all the networker has to do to assure that it doesn't
send another system a message it's already received is check the <P>ath field
for that system's name somewhere in the bang path.  If it's present, the system
has already seen the message, so we don't send it.

We also keep a small database, called the "use table," containing the ID's of
all messages we've seen recently.  If the same message arrives a second or
subsequent time, we will find its ID in the use table, indicating that we
already have a copy of that message.  It will therefore be discarded.

The above discussion should make the function of the fields reasonably clear:

 o  Travelling messages need to carry original message-id, system of origin,
    date of origin, author, and path with them, to keep reproduction and
    cycling under control.

(Uncoincidentally) the format used to transmit messages for networking
purposes is precisely that used on disk, serialized.  The current
distribution includes serv_network.c, which is basically a database replicator;
please see network.txt on its operation and functionality (if any).

PORTABILITY ISSUES
------------------
Citadel is 64-bit clean, architecture-independent, and Year 2000
compliant.  The software should compile on any POSIX compliant system with
a full pthreads implementation and TCP/IP support.  In the future we may
try to port it to non-POSIX systems as well.

On the client side, it's also POSIX compliant.  The client even seems to
build ok on non-POSIX systems with porting libraries (such as Cygwin).

SUPPORTING PRIVATE MAIL
-----------------------
Can one have an elegant kludge?  This must come pretty close.

Private mail is sent and recieved in the *Mail>* room, which otherwise
behaves pretty much as any other room.        To make this work, we have a
separate Mail> room for each user behind the scenes.  The actual room name
in the database looks like *"0000001234.Mail"* (where *'1234'* is the user
number) and it's flagged with the *QR_MAILBOX* flag.  The user number is
stripped off by the server before the name is presented to the client.  This
provides the ability to give each user a separate namespace for mailboxes
and personal rooms.

This requires a little fiddling to get things just right. For example,
*make_message()* has to be kludged to ask for the name of the recipient
of the message whenever a message is entered in *Mail>*. But basically
it works pretty well, keeping the code and user interface simple and
regular.

PASSWORDS AND NAME VALIDATION
-----------------------------
This has changed a couple of times over the course of Citadel's history.  At
this point it's very simple, again due to the fact that record managers are
used for everything.    The user file (user) is indexed using the user's
name, converted to all lower-case.  Searching for a user, then, is easy.  We
just lowercase the name we're looking for and query the database.  If no
match is found, it is assumed that the user does not exist.

This makes it difficult to forge messages from an existing user.  (Fine
point: nonprinting characters are converted to printing characters, and
leading, trailing, and double blanks are deleted.)
