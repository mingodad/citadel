/*
 * getutline.c: not-quite-compatible replacement for getutline(3)
 * by nathan bryant, feb 1999
 */

#include "sysdep.h"
#ifdef HAVE_UTMP_H
#include <stdio.h>
#include <sys/types.h>
#include <utmp.h>
#ifdef HAVE_PATHS_H
#include <paths.h>
#endif
#include <string.h>

struct utmp *getutline(struct utmp *ut)
{
  static struct utmp retval;
  FILE *utmp;

#ifdef UTMP_FILE
  if ((utmp = fopen(UTMP_FILE, "rb")) == NULL)
#else
  if ((utmp = fopen(_PATH_UTMP, "rb")) == NULL)
#endif
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
