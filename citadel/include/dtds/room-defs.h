/*
 * Room records.
 */

STRING_BUF(QRname,ROOMNAMELEN);	/* Name of room                     */
STRING_BUF(QRpasswd,10);		/* Only valid if it's a private rm  */
LONG(QRroomaide);		/* User number of room aide         */
LONG(QRhighest);			/* Highest message NUMBER in room   */
TIME(QRgen);			/* Generation number of room        */
UNSIGNED(QRflags);		/* See flag values below            */
STRING_BUF(QRdirname,15);		/* Directory name, if applicable    */
LONG(QRinfo);			/* Info file update relative to msgs*/
CHAR(QRfloor);			/* Which floor this room is on      */
TIME(QRmtime);			/* Date/time of last post           */
SUBSTRUCT(struct ExpirePolicy QRep);	/* Message expiration policy        */
SUBSTRUCT_ELEMENT(INTEGER(QRep.expire_mode));
SUBSTRUCT_ELEMENT(INTEGER(QRep.expire_value));
LONG(QRnumber);			/* Globally unique room number      */
CHAR(QRorder);			/* Sort key for room listing order  */
UNSIGNED(QRflags2);		/* Additional flags                 */
INTEGER(QRdefaultview);		/* How to display the contents      */
