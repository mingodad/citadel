/* 
 * $Id$
 *
 * A simple wrapper for the Citadel client.  This allows telnetd to call
 * Citadel without a system login.
 *
 */

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include "citadel.h"

void get_config(void);
struct config config;

int main (int argc, char **argv) {
	get_config();
	setuid(config.c_bbsuid);
	execlp("./citadel", "citadel", NULL);
	exit(errno);
}
