/*
 * $Id$
 */

/**
 *  Replacements for snprintf() and vsnprintf()
 *
 * Use it only if you have the "spare" cycles needed to effectively
 * do every snprintf operation twice! Why is that? Because everything
 * is first vfprintf()'d to /dev/null to determine the number of bytes.
 * Perhaps a bit slow for demanding applications on slow machines,
 * no problem for a fast machine with some spare cycles.
 *
 * You don't have a /dev/null? Every Linux contains one for free!
 *
 * Because the format string is never even looked at, all current and
 * possible future printf-conversions should be handled just fine.
 *
 * Written July 1997 by Sten Gunterberg (gunterberg@ergon.ch)
 */

#include "webcit.h"
#include "webserver.h"

/**
 *  is it needed????
 *  fmt the formatstring?
 *  argp how many params?
 */
static int needed(const char *fmt, va_list argp)
{
	static FILE *sink = NULL;

	/**
	 * ok, there's a small race here that could result in the sink being
	 * opened more than once if we're threaded, but I'd rather ignore it than
	 * spend cycles synchronizing :-) */

	if (sink == NULL) {
		if ((sink = fopen("/dev/null", "w")) == NULL) {
			perror("/dev/null");
			exit(1);
		}
	}
	return vfprintf(sink, fmt, argp);
}

/**
 *  vsnprintf wrapper
 *  buf the output charbuffer
 *  max maximal size of the buffer
 *  fmt the formatstring (see man printf)
 *  argp the variable argument list 
 */
int vsnprintf(char *buf, size_t max, const char *fmt, va_list argp)
{
	char *p;
	int size;

	if ((p = malloc(needed(fmt, argp) + 1)) == NULL) {
		lprintf(1, "vsnprintf: malloc failed, aborting\n");
		abort();
	}
	if ((size = vsprintf(p, fmt, argp)) >= max)
		size = -1;

	strncpy(buf, p, max);
	buf[max - 1] = 0;
	free(p);
	return size;
}

/**
 *  snprintf wrapper
 *  buf the output charbuffer
 *  max maximal size of the buffer
 *  fmt the formatstring (see man printf)
 *  ... the variable argument list 
 */
int snprintf(char *buf, size_t max, const char *fmt,...)
{
	va_list argp;
	int bytes;

	va_start(argp, fmt);
	bytes = vsnprintf(buf, max, fmt, argp);
	va_end(argp);

	return bytes;
}


