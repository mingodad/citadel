#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include "citadel.h"
#include "server.h"
#include "proto.h"

int CtdlGetLastError() {
	return(CC->CtdlErrno);
	}

int CtdlInternalGetUser(char *UserName, struct usersupp *usbuf) {
	if ( (strlen(UserName)==0) || (!strcasecmp(UserName,CC->curr_user)) ) {
		if (!CC->logged_in) {
			CC->CtdlErrno = ERROR+NOT_LOGGED_IN;
			return(-1);
			}
		memcpy(usbuf, &CC->usersupp, sizeof(struct usersupp));
		return(0);
		}
	else {	
		if (getuser(usbuf, UserName) != 0) {
			CC->CtdlErrno = ERROR+NO_SUCH_USER;
			return(-1);
			}
		else {
			CC->CtdlErrno = 0;
			return(0);
			}
		}
	}

int CtdlGetUserAccessLevel(char *UserName) {
	struct usersupp usbuf;

	return( (CtdlInternalGetUser(UserName, &usbuf)==0)
		?	usbuf.axlevel
		:	(-1)
		);
	}

long CtdlGetUserNumber(char *UserName) {
	struct usersupp usbuf;

	return( (CtdlInternalGetUser(UserName, &usbuf)==0)
		?	usbuf.usernum
		:	(-1L)
		);
	}

time_t CtdlGetUserLastCall(char *UserName) {
	struct usersupp usbuf;

	return( (CtdlInternalGetUser(UserName, &usbuf)==0)
		?	usbuf.lastcall
		:	(-1L)
		);
	}
