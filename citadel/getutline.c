/*
 * getutline.c: not-quite-compatible replacement for getutline(3)
 * by nathan bryant, feb 1999
 *
 * $Id$
 */

#include "sysdep.h"
#ifdef HAVE_UTMP_H
#include <stdio.h>
#include <utmp.h>
#include <paths.h>
#include <string.h>

struct utmp *getutline(struct utmp *ut)
{
  static struct utmp retval;
  FILE *utmp;

  if ((utmp = fopen(_PATH_UTMP, "rb")) == NULL)
    return NULL;

  do
    if (!fread(&retval, sizeof retval, 1, utmp))
      {
	fclose(utmp);
	return NULL;
      }
  while (strcmp(ut->ut_line, retval.ut_line));

  fclose(utmp);
  return &retval;
}
#endif /* HAVE_UTMP_H */
