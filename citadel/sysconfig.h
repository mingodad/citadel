/*
 * $Id$
 *
 */

/****************************************************************************/
/*                  YOUR SYSTEM CONFIGURATION                               */
/* Set all the values in this file appropriately BEFORE compiling any of the*/
/* C programs. If you are upgrading from an older version of Citadel, it */
/* is vitally important that the #defines which are labelled "structure size*/
/* variables" are EXACTLY the same as they were in your old system,         */
/* otherwise your files will be munged beyond repair.                       */
/****************************************************************************/

/* $Id$ */

/*
 * NOTE: this file is for client software tuning, not customization.  For
 * making changes to the behavior of the client, you want to edit citadel.rc,
 * not this file.
 */

/*
 * If you want to keep a transcript of all multiuser chats that go across
 * your system, define CHATLOG to the filename to be saved to.  Otherwise,
 * set CHATLOG to "/dev/null".
 */
#define CHATLOG		"/dev/null"

/* 
 * S_KEEPALIVE is a watchdog timer.  It is used to send "keep
 * alive" messages to the server to prevent the server from assuming the
 * client is dead and terminating the session.  30 seconds is the recommended
 * value; I can't think of any good reason to change it.
 */
#define S_KEEPALIVE	30

/*
 * Logging level to use if none is specified on the command line.
 * Note that this will suppress messages before they even get to syslog().
 */
#define DEFAULT_VERBOSITY	7



/*
 * NLI is the string that shows up in a <W>ho's online listing for sessions
 * that are active, but for which no user has yet authenticated.
 */
#define NLI	"(not logged in)"

/*
 * Maximum number of floors on the system.
 * WARNING!  *Never* change this value once your system is up; THINGS WILL DIE!
 * Also, do not set it higher than 127.
 */
#define MAXFLOORS	16

/*
 * Standard buffer size for string datatypes.  DO NOT CHANGE!  Not only does
 * there exist a minimum buffer size for certain protocols (such as IMAP), but
 * fixed-length buffers are now stored in some of the data structures on disk,
 * so if you change the buffer size you'll fux0r your database.
 */
#define SIZ		4096

/*
 * If the body of a message is beyond this size, it will be stored in
 * a separate table.
 */
#define BIGMSG		1024

/*
 * SMTP delivery retry rules (all values are in seconds)
 *
 * If delivery of a message via SMTP is unsuccessful, Citadel will try again
 * after SMTP_RETRY_INTERVAL seconds.  This interval will double after each
 * unsuccessful delivery, up to a maximum of SMTP_RETRY_MAX seconds.  If no
 * successful delivery has been accomplished after SMTP_GIVE_UP seconds, the
 * message will be returned to its sender.
 */
#define	SMTP_RETRY_INTERVAL	900	/* 15 minutes */
#define SMTP_RETRY_MAX		43200	/* 12 hours */
#define SMTP_GIVE_UP		432000	/* 5 days */

/*
 * Who bounced messages appear to be from
 */
#define BOUNCESOURCE		"Citadel Mail Delivery Subsystem"

/*
 * This variable defines the amount of network spool data that may be carried
 * in one server transfer command.  For some reason, some networks get hung
 * up on larger packet sizes.  We don't know why.  In any case, never set the
 * packet size higher than 4096 or your server sessions will crash.
 */
#define IGNET_PACKET_SIZE	4000

/*
 * The names of rooms which are automatically created by the system
 */
#define BASEROOM		"Lobby"
#define MAILROOM		"Mail"
#define SENTITEMS		"Sent Items"
#define AIDEROOM		"Aide"
#define USERCONFIGROOM		"My Citadel Config"
#define USERCALENDARROOM	"Calendar"
#define USERTASKSROOM		"Tasks"
#define USERCONTACTSROOM	"Contacts"
#define USERNOTESROOM		"Notes"
#define USERTRASHROOM		"Trash"
#define PAGELOGROOM		"Sent/Received Pages"
#define SYSCONFIGROOM		"Local System Configuration"
#define SMTP_SPOOLOUT_ROOM	"__CitadelSMTPspoolout__"
#define FNBL_QUEUE_ROOM		"__CitadelFNBLqueue__"
/*
 * Where we keep messages containing the vCards that source our directory.  It
 * makes no sense to change this, because you'd have to change it on every
 * system on the network.  That would be stupid.
 */
#define ADDRESS_BOOK_ROOM	"Global Address Book"


/*
 * How long (in seconds) to retain message entries in the use table
 */
#define USETABLE_RETAIN		604800L		/* 7 days */

/*
 * Pathnames for cryptographic goodness
 */
/*
#define	CTDL_CRYPTO_DIR		"./keys"
#define CTDL_KEY_PATH		CTDL_CRYPTO_DIR "/citadel.key"
#define CTDL_CSR_PATH		CTDL_CRYPTO_DIR "/citadel.csr"
#define CTDL_CER_PATH		CTDL_CRYPTO_DIR "/citadel.cer"
*/
#define THREADSTACKSIZE		1048576

/*
 * How many messages may the full text indexer scan before flushing its
 * tables to disk?
 */
#define FT_MAX_CACHE		2500
#
