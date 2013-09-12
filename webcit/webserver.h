/*
 * Copyright (c) 1996-2013 by the citadel.org team
 *
 * This program is open source software.  You can redistribute it and/or
 * modify it under the terms of the GNU General Public License, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


extern char *static_dirs[PATH_MAX];          /**< Web representation */
extern int ndirs;
extern char socket_dir[PATH_MAX];

extern char *default_landing_page;

int ClientGetLine(ParsedHttpHdrs *Hdr, StrBuf *Target);
int client_read_to(ParsedHttpHdrs *Hdr, StrBuf *Target, int bytes, int timeout);
void wc_backtrace(long LogLevel);
void ShutDownWebcit(void);
void shutdown_ssl(void);


