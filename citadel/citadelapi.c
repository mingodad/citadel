#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include "citadel.h"
#include "serv_info.h"
#include "ipc.h"
#include "citadelapi.h"
#include "support.h"


struct CtdlInternalList {
	struct CtdlInternalList *next;
	char data[256];
	};


struct CtdlServerHandle CtdlAppHandle;
struct CtdlServInfo CtdlAppServInfo;
struct CtdlRoomInfo CtdlCurrentRoom;
int CtdlErrno = 0;

void logoff(int exitcode) {
	exit(exitcode);
	}



/*
 * CtdlInternalNumParms()  -  discover number of parameters...
 */
int CtdlInternalNumParms(char *source)
{
	int a;
	int count = 1;

	for (a=0; a<strlen(source); ++a) 
		if (source[a]=='|') ++count;
	return(count);
	}

/*
 * CtdlInternalExtract()  -  extract a parameter from a series of "|" separated
 */
void CtdlInternalExtract(char *dest, char *source, int parmnum)
{
	char buf[256];
	int count = 0;
	int n;

	if (strlen(source)==0) {
		strcpy(dest,"");
		return;
		}

	n = CtdlInternalNumParms(source);

	if (parmnum >= n) {
		strcpy(dest,"");
		return;
		}
	strcpy(buf,source);
	if ( (parmnum == 0) && (n == 1) ) {
		strcpy(dest,buf);
		for (n=0; n<strlen(dest); ++n)
			if (dest[n]=='|') dest[n] = 0;
		return;
		}

	while (count++ < parmnum) do {
		strcpy(buf,&buf[1]);
		} while( (strlen(buf)>0) && (buf[0]!='|') );
	if (buf[0]=='|') strcpy(buf,&buf[1]);
	for (count = 0; count<strlen(buf); ++count)
		if (buf[count] == '|') buf[count] = 0;
	strcpy(dest,buf);
	}

/*
 * CtdlInternalExtractInt()  -  Extract an int parm w/o supplying a buffer
 */
int CtdlInternalExtractInt(char *source, int parmnum)
{
	char buf[256];
	
	CtdlInternalExtract(buf,source,parmnum);
	return(atoi(buf));
	}

/*
 * CtdlInternalExtractLong()  -  Extract an long parm w/o supplying a buffer
 */
long CtdlInternalExtractLong(char *source, long int parmnum)
{
	char buf[256];
	
	CtdlInternalExtract(buf,source,parmnum);
	return(atol(buf));
	}












/*
 * Programs linked against the Citadel server extension library need to
 * be called with the following arguments:
 * 0 - program name (as always)
 * 1 - server address (usually 127.0.0.1)
 * 2 - server port number
 * 3 - internal program secret
 * 4 - user name
 * 5 - user password
 * 6 - initial room
 * 7 - associated client session
 * 
 */

int
main(int argc, char *argv[])
{
	int a;
	char buf[256];

	/* We're really not interested in stdio */
	close(0);
	close(1);
	close(2);

	/* Bail out if someone tries to run this thing manually */
	if (argc < 3) exit(1);

	/* Zeroing out the server handle neatly sets the values of
	 * CtdlAppHandle to sane default values.  This also holds true
	 * for the CtdlCurrentRoom.
	 */
	bzero(&CtdlAppHandle, sizeof(struct CtdlServerHandle));
	bzero(&CtdlCurrentRoom, sizeof(struct CtdlRoomInfo));

	/* Now parse the command-line arguments fed to us by the server */
	for (a=0; a<argc; ++a) switch(a) {
		case 1:	strcpy(CtdlAppHandle.ServerAddress, argv[a]);
			break;
		case 2:	CtdlAppHandle.ServerPort = atoi(argv[a]);
			break;
		case 3:	strcpy(CtdlAppHandle.ipgmSecret, argv[a]);
			break;
		case 4:	strcpy(CtdlAppHandle.UserName, argv[a]);
			break;
		case 5:	strcpy(CtdlAppHandle.Password, argv[a]);
			break;
		case 6:	strcpy(CtdlAppHandle.InitialRoom, argv[a]);
			break;
		case 7:	CtdlAppHandle.AssocClientSession = atoi(argv[a]);
			break;
		}

	/* Connect to the server */
	argc = 3;
	attach_to_server(argc, argv);
	serv_gets(buf);
	if (buf[0] != '2') exit(1);

	/* Set up the server environment to our liking */

	CtdlInternalGetServInfo(&CtdlAppServInfo);

	sprintf(buf, "IDEN 0|5|006|CitadelAPI Client");
	serv_puts(buf);
	serv_gets(buf);

	if (strlen(CtdlAppHandle.ipgmSecret) > 0) {
		sprintf(buf, "IPGM %s", CtdlAppHandle.ipgmSecret);
		serv_puts(buf);
		serv_gets(buf);
		}

	if (strlen(CtdlAppHandle.UserName) > 0) {
		sprintf(buf, "USER %s", CtdlAppHandle.UserName);
		serv_puts(buf);
		serv_gets(buf);
		sprintf(buf, "PASS %s", CtdlAppHandle.Password);
		serv_puts(buf);
		serv_gets(buf);
		}

	if (CtdlGotoRoom(CtdlAppHandle.InitialRoom) != 0) {
		CtdlGotoRoom("_BASEROOM_");
		}

	/* Now do the loop. */
	CtdlMain();

	/* Clean up gracefully and exit. */
	serv_puts("QUIT");
	exit(0);
	}


int CtdlGetLastError(void) {
	return CtdlErrno;
	}


int CtdlSendExpressMessage(char *ToUser, char *MsgText) {
	char buf[256];

	if (strlen(ToUser) + strlen(MsgText) > 248) {
		CtdlErrno = ERROR + TOO_BIG;
		return CtdlErrno;
		}

	sprintf(buf, "SEXP %s|%s", ToUser, MsgText);
	serv_puts(buf);
	serv_gets(buf);
	
	CtdlErrno = atoi(buf);
	if (CtdlErrno == OK) CtdlErrno = 0;
	return CtdlErrno;
	}



int CtdlInternalGetUserParam(char *ParamBuf, int ParamNum, char *WhichUser) {
	char buf[256];

	sprintf(buf, "AGUP %s", WhichUser);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0] != '2') return(atoi(buf));
	CtdlInternalExtract(ParamBuf, &buf[4], ParamNum);
	return(0);
	}


int CtdlInternalSetUserParam(char *ParamBuf, int ParamNum, char *WhichUser) {
	char buf[256];
	char params[8][256];
	int a;

	sprintf(buf, "AGUP %s", WhichUser);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0] != '2') return(atoi(buf));
	for (a=0; a<8; ++a) {
		CtdlInternalExtract(&params[a][0], &buf[4], a);
		}
	strcpy(&params[ParamNum][0], ParamBuf);
	strcpy(buf, "ASUP ");
	for (a=0; a<8; ++a) {
		strcat(buf, &params[a][0]);
		strcat(buf, "|");
		}
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0] != '2') return(atoi(buf));
	return(0);
	}

/*
 0 - User name
 1 - Password
 2 - Flags (see citadel.h)
 3 - Times called
 4 - Messages posted
 5 - Access level
 6 - User number
 7 - Timestamp of last call
 */

int CtdlGetUserPassword(char *buf, char *WhichUser) {
	CtdlErrno = CtdlInternalGetUserParam(buf, 1, WhichUser);
	return(CtdlErrno);
	}

int CtdlSetUserPassword(char *buf, char *WhichUser) {
	CtdlErrno = CtdlInternalSetUserParam(buf, 1, WhichUser);
	return(CtdlErrno);
	}

unsigned int CtdlGetUserFlags(char *WhichUser) {
	char buf[256];
	CtdlErrno = CtdlInternalGetUserParam(buf, 2, WhichUser);
	return((CtdlErrno == 0) ? atoi(buf) : (-1));
	}

int CtdlSetUserFlags(unsigned int NewFlags, char *WhichUser) {
	char buf[256];
	sprintf(buf, "%u", NewFlags);
	CtdlErrno = CtdlInternalGetUserParam(buf, 2, WhichUser);
	return(CtdlErrno);
	}

int CtdlGetUserTimesCalled(char *WhichUser) {
	char buf[256];
	CtdlErrno = CtdlInternalGetUserParam(buf, 3, WhichUser);
	return((CtdlErrno == 0) ? atoi(buf) : (-1));
	}

int CtdlSetUserTimesCalled(int NewValue, char *WhichUser) {
	char buf[256];
	sprintf(buf, "%d", NewValue);
	CtdlErrno = CtdlInternalGetUserParam(buf, 3, WhichUser);
	return(CtdlErrno);
	}

int CtdlGetUserMessagesPosted(char *WhichUser) {
	char buf[256];
	CtdlErrno = CtdlInternalGetUserParam(buf, 4, WhichUser);
	return((CtdlErrno == 0) ? atoi(buf) : (-1));
	}

int CtdlSetUserMessagesPosted(int NewValue, char *WhichUser) {
	char buf[256];
	sprintf(buf, "%d", NewValue);
	CtdlErrno = CtdlInternalGetUserParam(buf, 4, WhichUser);
	return(CtdlErrno);
	}

int CtdlGetUserAccessLevel(char *WhichUser) {
	char buf[256];
	CtdlErrno = CtdlInternalGetUserParam(buf, 5, WhichUser);
	return((CtdlErrno == 0) ? atoi(buf) : (-1));
	}

int CtdlSetUserAccessLevel(int NewValue, char *WhichUser) {
	char buf[256];

	if ( (NewValue < 0) || (NewValue > 6) ) {
		return(ERROR + ILLEGAL_VALUE);
		}

	sprintf(buf, "%d", NewValue);
	CtdlErrno = CtdlInternalGetUserParam(buf, 5, WhichUser);
	return(CtdlErrno);
	}

long CtdlGetUserNumber(char *WhichUser) {
	char buf[256];
	CtdlErrno = CtdlInternalGetUserParam(buf, 6, WhichUser);
	return((CtdlErrno == 0) ? atol(buf) : (-1L));
	}

int CtdlSetUserNumber(long NewValue, char *WhichUser) {
	char buf[256];
	sprintf(buf, "%ld", NewValue);
	CtdlErrno = CtdlInternalGetUserParam(buf, 6, WhichUser);
	return(CtdlErrno);
	}

time_t CtdlGetUserLastCall(char *WhichUser) {
	char buf[256];
	CtdlErrno = CtdlInternalGetUserParam(buf, 7, WhichUser);
	return((CtdlErrno == 0) ? atol(buf) : (time_t)(-1L));
	}

int CtdlSetUserLastCall(time_t NewValue, char *WhichUser) {
	char buf[256];
	sprintf(buf, "%ld", NewValue);
	CtdlErrno = CtdlInternalGetUserParam(buf, 7, WhichUser);
	return(CtdlErrno);
	}

int CtdlForEachUser(int (*CallBack)(char *EachUser))  {
	struct CtdlInternalList *TheList = NULL;
	struct CtdlInternalList *ptr;
	char buf[256];

	serv_puts("LIST");
	serv_gets(buf);
	if (buf[0] != '1') return(-1);

	while (serv_gets(buf), strcmp(buf, "000")) {
		ptr = (struct CtdlInternalList *)
			malloc(sizeof (struct CtdlInternalList));
		if (ptr != NULL) {
			CtdlInternalExtract(ptr->data, buf, 0);
			ptr->next = TheList;
			TheList = ptr;
			}
		}

	while (TheList != NULL) {
		(*CallBack)(TheList->data);
		ptr = TheList->next;
		free(TheList);
		TheList = ptr;
		}

	return(0);
	}



/*
 * Goto a different room
 */
int CtdlGotoRoom(char *RoomName) {
	char buf[256];

	sprintf(buf, "GOTO %s", RoomName);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0] != '2') {
		CtdlErrno = atoi(buf);
		return(CtdlErrno);
		}
	extract(CtdlCurrentRoom.RoomName, &buf[4], 0);
	return 0;
	}
