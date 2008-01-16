/*
 * $Id$
 *
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <ctype.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include "libcitadel.h"

char *libcitadel_version_string(void) {
	return "$Id$";
}

int libcitadel_version_number(void) {
	return LIBCITADEL_VERSION_NUMBER;
}
