/*
 * Main stuff for libcitadel
 *
 * Copyright (c) 1987-2013 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
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

ConstStr RoomNetCfgStrs[maxRoomNetCfg] = {
	{HKEY(strof(subpending))},
	{HKEY(strof(unsubpending))},
	{HKEY(strof(lastsent))}, /* Server internal use only */
	{HKEY(strof(ignet_push_share))},
	{HKEY(strof(listrecp))},
	{HKEY(strof(digestrecp))},
	{HKEY(strof(pop3client))},
	{HKEY(strof(rssclient))},
	{HKEY(strof(participate))}
// No, not one of..	{HKEY(strof(maxRoomNetCfg))}
};



extern int EnableSplice;
extern int BaseStrBufSize;
extern int ZLibCompressionRatio;

char *libcitadel_version_string(void) {
	return "libcitadel(unnumbered)";
}

int libcitadel_version_number(void) {
	return LIBCITADEL_VERSION_NUMBER;
}

void StartLibCitadel(size_t basesize)
{
	const char *envvar;

	BaseStrBufSize = basesize;
	envvar = getenv("LIBCITADEL_ENABLE_SPLICE");
	if (envvar != NULL)
		EnableSplice = atol(envvar);

	envvar = getenv("LIBCITADEL_ZLIB_LEVEL");
	if (envvar != NULL)
		ZLibCompressionRatio = atol(envvar);
}

void ShutDownLibCitadel(void)
{
	ShutDownLibCitadelMime();
	WildFireShutdown();
	xdg_mime_shutdown();
}
