/* User record                      */

INTEGER(version);			/* Cit vers. which created this rec */
UID_T(uid);			/* Associate with a unix account?   */
STRING_BUF(password,32);		/* password (for Citadel-only users)*/
UNSIGNED(flags);			/* See US_ flags below           TODO: is this really the same?   */ 
LONG(timescalled);		/* Total number of logins           */
LONG(posted);			/* Number of messages posted (ever) */
UINT8(axlevel);		/* Access level                     */
LONG(usernum);			/* User number (never recycled)     */
TIME(lastcall);		/* Last time the user called        */
INTEGER(USuserpurge);		/* Purge time (in days) for user    */
STRING_BUF(fullname,64);		/* Name for Citadel messages & mail */
UINT8(USscreenwidth);	/* Screen width (for textmode users)*/
UINT8(USscreenheight);	/* Screen height(for textmode users)*/
