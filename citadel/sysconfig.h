/****************************************************************************/
/*                  YOUR SYSTEM CONFIGURATION                               */
/* Set all the values in this file appropriately BEFORE compiling any of the*/
/* C programs. If you are upgrading from an older version of Citadel/UX, it */
/* is vitally important that the #defines which are labelled "structure size*/
/* variables" are EXACTLY the same as they were in your old system,         */
/* otherwise your files will be munged beyond repair.                       */
/****************************************************************************/

/* $Id$ */

/* NOTE THAT THIS FILE IS MUCH, MUCH SMALLER THAN IT USED TO BE.
 * That's because the setup program now creates a citadel.config file with
 * all of the settings that don't really need to be in a header file.
 * You can now run setup whenever you want, and change lots of parameters
 * without having to recompile the whole system!
 */

/*
 * If you want to keep a transcript of all multiuser chats that go across
 * your system, define CHATLOG to the filename to be saved to.  Otherwise,
 * set CHATLOG to "/dev/null".
 */
#define CHATLOG		"chat.log"

/*
 * SLEEPING refers to the watchdog timer.  If a user sits idle without typing
 * anything for this number of seconds, the session will automatically be
 * logged out.  Set it to zero to disable this feature.
 * Note: the watchdog timer only functions when the parent is 1 (init) - in
 * other words, only if Citadel is the login shell. 
 */
#define SLEEPING	180

/* 
 * S_KEEPALIVE is also a watchdog timer, except it is used to send "keep
 * alive" messages to the server to prevent the server from assuming the
 * client is dead and terminating the session.  30 seconds is the recommended
 * value; I can't think of any good reason to change it.
 */
#define S_KEEPALIVE	30

/*
 * This is the command that gets executed when a user hits <E>nter message:
 * presses the <E>nter message key.  The possible values are:
 *   46 - .<E>nter message with <E>ditor
 *   4  - .<E>nter <M>essage
 *   36 - .<E>nter message with <A>scii
 * Normally, this value will be set to 4, to cause the <E>nter message
 * command to run Citadel's built-in editor.  However, if you have an external
 * editor installed, and you want to make it the default, set this to 46
 * to make it use your editor by default.
 */
#define DEFAULT_ENTRY		4


/*
 * Logging level to use if none is specified on the command line.
 */
#define DEFAULT_VERBOSITY	9


/*
 * HOUSEKEEPING_WAKEUP is the number of seconds which pass between each pass
 * of the housekeeping thread.  This normally happens once per minute and there
 * isn't any good reason to change it.
 */
#define HOUSEKEEPING_WAKEUP	60


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
 * SMTP delivery retry and give-up times
 */
#define	SMTP_RETRY_INTERVAL	900	/* retry sends every 15 minutes */
#define SMTP_GIVE_UP		432000	/* give up after 5 days */


/*
 * The names of rooms which are automatically created by the system
 */
#define BASEROOM		"Lobby"
#define MAILROOM		"Mail"
#define AIDEROOM		"Aide"
#define USERCONFIGROOM		"My Citadel Config"
#define PAGELOGROOM		"Sent/Received Pages"
#define SYSCONFIGROOM		"Local System Configuration"
#define SMTP_SPOOLOUT_ROOM	"__CitadelSMTPspoolout__"
