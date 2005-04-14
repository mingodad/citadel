/*
 * $Id$
 *
 * a setuid helper program for machines which use shadow passwords
 * by Nathan Bryant, March 1999
 *
 */

#include <pwd.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/types.h>

#include "auth.h"
#include "config.h"
#include "citadel.h"

int main(void)
{
  uid_t uid;
  struct passwd *pw;
  char buf[SIZ];

  get_config();
  uid = getuid();

  if (uid != CTDLUID && uid)
    {
      pw = getpwuid(uid);
      openlog("chkpwd", LOG_CONS, LOG_AUTH);
      syslog(LOG_WARNING, "invoked by %s (uid %u); possible breakin/probe "
	     "attempt", pw != NULL ? pw->pw_name : "?", uid);
      return 1;
    }

  if (fgets(buf, sizeof buf, stdin) == NULL)
    return 1;

  strtok(buf, "\n");
  uid = atoi(buf);

  if (fgets(buf, sizeof buf, stdin) == NULL)
    return 1;

  strtok(buf, "\n");

  if (validpw(uid, buf))
    return 0;

  return 1;
}
