/*
 * $id: $
 *
 * Convert RDF style datestamps (2005-09-17T06:18:00+00:00) into time_t
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <ctype.h>
#include <time.h>
#include <string.h>

#include "rdf_parsedate.h"



time_t rdf_parsedate(char *p)
{
	struct tm tm;

	if (!p) return 0L;
	if (strlen(p) < 10) return 0L;

	memset(&tm, 0, sizeof tm);

	/* We only extract the date.  Time is not needed
	 * because we don't need that much granularity.
	 */
	tm.tm_year = atoi(&p[0]) - 1900;
	tm.tm_mon = atoi(&p[5]);
	tm.tm_mday = atoi(&p[8]);

	return mktime(&tm);
}
