/*
 * $Id$
 *
 */

/*
 * Format of a usersupp record prior to version 5.55
 */
struct pre555usersupp {			/* User record                      */
	int USuid;			/* userid (==BBSUID for bbs only)   */
	char password[20];		/* password (for BBS-only users)    */
	unsigned flags;			/* See US_ flags below              */
	int timescalled;		/* Total number of logins           */
	int posted;			/* Number of messages posted (ever) */
	char fullname[26];		/* Name for Citadel messages & mail */
	char axlevel;			/* Access level                     */
	CIT_UBYTE USscreenwidth;	/* Screen width (for textmode users)*/
	CIT_UBYTE USscreenheight;	/* Screen height(for textmode users)*/
	long usernum;			/* User number (never recycled)     */
	time_t lastcall;		/* Last time the user called        */
	char USname[30];		/*                                  */
	char USaddr[25];		/*                                  */
	char UScity[15];		/*                                  */
	char USstate[3];		/*                                  */
	char USzip[10];			/*                                  */
	char USphone[11];		/*                                  */
	char USemail[32];		/*                                  */
	int USuserpurge;		/* Purge time (in days) for user    */
};
