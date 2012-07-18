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
#define	SMTP_RETRY_INTERVAL	300	/* 5 minutes */
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
#define USERDRAFTROOM		"Drafts"
#define USERTRASHROOM		"Trash"
#define PAGELOGROOM		"Sent/Received Pages"
#define SYSCONFIGROOM		"Local System Configuration"
#define SMTP_SPOOLOUT_ROOM	"__CitadelSMTPspoolout__"
#define FNBL_QUEUE_ROOM		"__CitadelFNBLqueue__"
#define PAGER_QUEUE_ROOM	"__CitadelPagerQueue__"
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
 * The size of per-thread stacks.  If set too low, citserver will randomly crash.
 */
#define THREADSTACKSIZE		0x100000

/*
 * How many messages may the full text indexer scan before flushing its
 * tables to disk?
 */
#define FT_MAX_CACHE		2500
