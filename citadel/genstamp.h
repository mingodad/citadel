/*
 * $Id$
 *
 */

void datestring(char *buf, time_t xtime, int which_format);

enum {
	DATESTRING_RFC822,
	DATESTRING_IMAP
};
