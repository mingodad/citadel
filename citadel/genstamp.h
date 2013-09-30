
long datestring(char *buf, size_t n, time_t xtime, int which_format);

enum {
	DATESTRING_RFC822,
	DATESTRING_IMAP
};
