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
#include "xdgmime/xdgmime.h"
#include "libcitadellocal.h"

extern int BaseStrBufSize;
char *libcitadel_version_string(void) {
	return "$Id$";
}

int libcitadel_version_number(void) {
	return LIBCITADEL_VERSION_NUMBER;
}

void StartLibCitadel(size_t basesize)
{
	BaseStrBufSize = basesize;
}

void ShutDownLibCitadel(void)
{
	ShutDownLibCitadelMime();
	WildFireShutdown();
	xdg_mime_shutdown();
}
