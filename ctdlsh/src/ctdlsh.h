/*
 * main header file for ctdlsh
 *
 * Copyright (c) 2009-2013 by the citadel.org team
 * This program is open source software, cheerfully made available to
 * you under the terms of the GNU General Public License version 3.
 */

#include <config.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <pwd.h>
#include <time.h>
#include <errno.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <readline/readline.h>

/*
 * Set to the location of Citadel
 * FIXME this needs to be configurable
 */
#ifndef CTDLDIR
#define CTDLDIR	"/usr/local/citadel"
#endif

typedef int ctdlsh_cmdfunc_t(int, char *);

enum ctdlsh_cmdfunc_return_values {
	cmdret_ok,
	cmdret_exit,
	cmdret_error
};

int cmd_help(int, char *);
int cmd_quit(int, char *);
int cmd_datetime(int, char *);
int cmd_passwd(int, char *);
int cmd_shutdown(int, char *);
int cmd_who(int, char *);
int cmd_export(int, char *);
