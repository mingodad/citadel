/*
 * $Id$
 */
/**
 * \defgroup MiscRout Miscellaneous routines 
 * \ingroup tools
 */

/*@{*/
#include "webcit.h"
#include "webserver.h"


typedef unsigned char byte; /**< byte data type */

#define FALSE 0 /**< no. */
#define TRUE 1  /**< yes. */

static byte dtable[256];	/**< base64 encode / decode table */

/**
 * \brief sanitize strncopy.
 * \param dest destination string
 * \param src source string
 * \param n length of source to copy 
 * \return result string
 */
char *safestrncpy(char *dest, const char *src, size_t n)
{
	if (dest == NULL || src == NULL) {
		abort();
	}
	strncpy(dest, src, n);
	dest[n - 1] = 0;
	return dest;
}



/**
 * \brief discover number of parameters/tokens in a string
 * \param source string to inspect
 * \param tok seperation token
 * \return number of tokenized parts found
 */
int num_tokens(char *source, char tok)
{
	int count = 1;
	char *ptr = source;

	if (source == NULL) {
		return (0);
	}

	while (*ptr) {
		if (*ptr++ == tok) {
			++count;
		}
	}
	
	return (count);
}

/**
 * brief a string tokenizer
 * \param dest destination string 
 * \param source the string to grab tokens from
 * \param parmnum the n'th token to grab
 * \param separator the tokenizer string
 * \param maxlen the length of dest
 */
void extract_token(char *dest, const char *source, int parmnum, char separator, int maxlen)
{
	char *d;		/* dest */
	const char *s;		/* source */
	int count = 0;
	int len = 0;

	dest[0] = 0;

	/* Locate desired parameter */
	s = source;
	while (count < parmnum) {
		/* End of string, bail! */
		if (!*s) {
			s = NULL;
			break;
		}
		if (*s == separator) {
			count++;
		}
		s++;
	}
	if (!s) return;		/* Parameter not found */

	for (d = dest; *s && *s != separator && ++len<maxlen; s++, d++) {
		*d = *s;
	}
	*d = 0;
}



/**
 * \brief a tokenizer that kills, maims, and destroys
 * \param source the string to process
 * \param parmnum which token to kill
 * \param separator the tokenizer string
 */
void remove_token(char *source, int parmnum, char separator)
{
	int i;
	int len, slen;
	int curr_parm;
	int start, end;

	len = 0;
	curr_parm = 0;
	start = (-1);
	end = (-1);

	slen = strlen(source);
	if (slen == 0) {
		return;
	}

	for (i = 0; 
	     ( (i < slen)  && (end == -1) ); 
	     ++i) {
		if ((start < 0) && (curr_parm == parmnum)) {
			start = i;
		}

		if ((end < 0) && (curr_parm == (parmnum + 1))) {
			end = i;
		}

		if (source[i] == separator) {
			++curr_parm;
		}
	}

	if (end < 0)
		end = slen;

	memmove(&source[start], &source[end], slen - end + 1);
}




/**
 * \brief extract an int parm w/o supplying a buffer
 * \param source the string to locate the int in
 * \param parmnum the n'th token to grab the int from
 * \return the integer
 */
int extract_int(const char *source, int parmnum)
{
	char buf[32];
	
	extract_token(buf, source, parmnum, '|', sizeof buf);
	return(atoi(buf));
}

/**
 * \brief extract an long parm w/o supplying a buffer
 * \param source string to examine
 * \param parmnum n'th token to search long in
 * \return the found long value
 */
long extract_long(const char *source, int parmnum)
{
	char buf[32];
	
	extract_token(buf, source, parmnum, '|', sizeof buf);
	return(atol(buf));
}






/**
 * \brief check for the presence of a character within a string (returns count)
 * \param st the string to examine
 * \param ch the char to search
 * \return the position inside of st
 */
int haschar(char *st,char ch)
{
	int a, b, len;
	b = 0;
	len = strlen(st);
	for (a = 0; a < len; ++a)
		if (st[a] == ch)
			++b;
	return (b);
}


/** 
 * \brief Utility function to "readline" from memory
 * \param start Location in memory from which we are reading.
 * \param buf the buffer to place the string in.
 * \param maxlen Size of string buffer
 * \return Pointer to the source memory right after we stopped reading.
 */
char *memreadline(char *start, char *buf, int maxlen)
{
	char ch;
	char *ptr;
	int len = 0;		/**< tally our own length to avoid strlen() delays */

	ptr = start;
	memset(buf, 0, maxlen);

	while (1) {
		ch = *ptr++;
		if ((len + 1 < (maxlen)) && (ch != 13) && (ch != 10)) {
			buf[len++] = ch;
			buf[len] = 0;
		}
		if ((ch == 10) || (ch == 0)) {
			return ptr;
		}
	}
}



/**
 * \brief searches for a  paternn within asearch string
 * \param search the string to search 
 * \param patn the pattern to find in string
 * \returns position in string
 */
int pattern2(char *search, char *patn)
{
	int a;
	int len, plen;
	len = strlen (search);
	plen = strlen (patn);
	for (a = 0; a < len; ++a) {
		if (!strncasecmp(&search[a], patn, plen))
			return (a);
	}
	return (-1);
}


/**
 * \brief Strip leading and trailing spaces from a string; with premeasured and adjusted length.
 * \param buf the string to modify
 * \param len length of the string. 
 */
void stripltlen(char *buf, int *len)
{
	int delta = 0;
	if (*len == 0) return;
	while ((*len > delta) && (isspace(buf[delta]))){
		delta ++;
	}
	memmove (buf, &buf[delta], *len - delta + 1);
	(*len) -=delta;

	if (*len == 0) return;
	while (isspace(buf[(*len) - 1])){
		buf[--(*len)] = '\0';
	}
}

/**
 * \brief Strip leading and trailing spaces from a string
 * \param buf the string to modify
 */
void striplt(char *buf)
{
	int len;
	len = strlen(buf);
	stripltlen(buf, &len);
}


/**
 * \brief Determine whether the specified message number is contained within the
 * specified set.
 *
 * \param mset Message set string
 * \param msgnum Message number we are looking for
 *
 * \return Nonzero if the specified message number is in the specified message set string.
 */
int is_msg_in_mset(char *mset, long msgnum) {
	int num_sets;
	int s;
	char setstr[SIZ], lostr[SIZ], histr[SIZ];	/* was 1024 */
	long lo, hi;

	/*
	 * Now set it for all specified messages.
	 */
	num_sets = num_tokens(mset, ',');
	for (s=0; s<num_sets; ++s) {
		extract_token(setstr, mset, s, ',', sizeof setstr);

		extract_token(lostr, setstr, 0, ':', sizeof lostr);
		if (num_tokens(setstr, ':') >= 2) {
			extract_token(histr, setstr, 1, ':', sizeof histr);
			if (!strcmp(histr, "*")) {
				snprintf(histr, sizeof histr, "%ld", LONG_MAX);
			}
		} 
		else {
			strcpy(histr, lostr);
		}
		lo = atol(lostr);
		hi = atol(histr);

		if ((msgnum >= lo) && (msgnum <= hi)) return(1);
	}

	return(0);
}



/**
 * \brief Strip a boundarized substring out of a string
 * (for example, remove
 * parentheses and anything inside them).
 *
 * This improved version can strip out *multiple* boundarized substrings.
 * \param str the string to process
 * \param leftboundary the boundary character on the left side of the target string 
 * \param rightboundary the boundary character on the right side of the target string
 */
void stripout(char *str, char leftboundary, char rightboundary)
{
	int a;
	int lb = (-1);
	int rb = (-1);
	int len = strlen(str);

	do {
		lb = (-1);
		rb = (-1);

		for (a = 0; a < len; ++a) {
			if (str[a] == leftboundary)
				lb = a;
			if (str[a] == rightboundary)
				rb = a;
		}

		if ((lb > 0) && (rb > lb)) {
			memmove(&str[lb - 1], &str[rb + 1], len - rb);
			len -= (rb - lb + 2);
		}

	} while ((lb > 0) && (rb > lb));

}



/**
 * \brief Replacement for sleep() that uses select() in order to avoid SIGALRM
 * \param seconds how many seconds should we sleep?
 */
void sleeeeeeeeeep(int seconds)
{
	struct timeval tv;

	tv.tv_sec = seconds;
	tv.tv_usec = 0;
	select(0, NULL, NULL, NULL, &tv);
}



/**
 * \brief encode a string into base64 to for example tunnel it through mail transport
 * CtdlDecodeBase64() and CtdlEncodeBase64() are adaptations of code by
 * John Walker, copied over from the Citadel server.
 * \param dest encrypted string
 * \param source the string to encrypt
 * \param sourcelen the length of the source data (may contain string terminators)
 */

void CtdlEncodeBase64(char *dest, const char *source, size_t sourcelen, int linebreaks)
{
	int i, hiteof = FALSE;
	int spos = 0;
	int dpos = 0;
	int thisline = 0;

	/**  Fill dtable with character encodings.  */

	for (i = 0; i < 26; i++) {
		dtable[i] = 'A' + i;
		dtable[26 + i] = 'a' + i;
	}
	for (i = 0; i < 10; i++) {
		dtable[52 + i] = '0' + i;
	}
	dtable[62] = '+';
	dtable[63] = '/';

	while (!hiteof) {
		byte igroup[3], ogroup[4];
		int c, n;

		igroup[0] = igroup[1] = igroup[2] = 0;
		for (n = 0; n < 3; n++) {
			if (spos >= sourcelen) {
				hiteof = TRUE;
				break;
			}
			c = source[spos++];
			igroup[n] = (byte) c;
		}
		if (n > 0) {
			ogroup[0] = dtable[igroup[0] >> 2];
			ogroup[1] =
			    dtable[((igroup[0] & 3) << 4) |
				   (igroup[1] >> 4)];
			ogroup[2] =
			    dtable[((igroup[1] & 0xF) << 2) |
				   (igroup[2] >> 6)];
			ogroup[3] = dtable[igroup[2] & 0x3F];

			/**
			 * Replace characters in output stream with "=" pad
			 * characters if fewer than three characters were
			 * read from the end of the input stream. 
			 */

			if (n < 3) {
				ogroup[3] = '=';
				if (n < 2) {
					ogroup[2] = '=';
				}
			}
			for (i = 0; i < 4; i++) {
				dest[dpos++] = ogroup[i];
				dest[dpos] = 0;
			}
			thisline += 4;
			if ( (linebreaks) && (thisline > 70) ) {
				dest[dpos++] = '\r';
				dest[dpos++] = '\n';
				dest[dpos] = 0;
				thisline = 0;
			}
		}
	}
	if ( (linebreaks) && (thisline > 70) ) {
		dest[dpos++] = '\r';
		dest[dpos++] = '\n';
		dest[dpos] = 0;
		thisline = 0;
	}
}


/**
 * \brief Convert base64-encoded to binary.  
 * It will stop after reading 'length' bytes.
 *
 * \param dest The destination buffer 
 * \param source The base64 data to be decoded.
 * \param length The number of bytes to decode.
 * \return The actual length of the decoded data.
 */
int CtdlDecodeBase64(char *dest, const char *source, size_t length)
{
	int i, c;
	int dpos = 0;
	int spos = 0;

	for (i = 0; i < 255; i++) {
		dtable[i] = 0x80;
	}
	for (i = 'A'; i <= 'Z'; i++) {
		dtable[i] = 0 + (i - 'A');
	}
	for (i = 'a'; i <= 'z'; i++) {
		dtable[i] = 26 + (i - 'a');
	}
	for (i = '0'; i <= '9'; i++) {
		dtable[i] = 52 + (i - '0');
	}
	dtable['+'] = 62;
	dtable['/'] = 63;
	dtable['='] = 0;

	/**CONSTANTCONDITION*/ while (TRUE) {
		byte a[4], b[4], o[3];

		for (i = 0; i < 4; i++) {
			if (spos >= length) {
				return (dpos);
			}
			c = source[spos++];

			if (c == 0) {
				if (i > 0) {
					return (dpos);
				}
				return (dpos);
			}
			if (dtable[c] & 0x80) {
				/** Ignoring errors: discard invalid character */
				i--;
				continue;
			}
			a[i] = (byte) c;
			b[i] = (byte) dtable[c];
		}
		o[0] = (b[0] << 2) | (b[1] >> 4);
		o[1] = (b[1] << 4) | (b[2] >> 2);
		o[2] = (b[2] << 6) | b[3];
		i = a[2] == '=' ? 1 : (a[3] == '=' ? 2 : 3);
		if (i >= 1)
			dest[dpos++] = o[0];
		if (i >= 2)
			dest[dpos++] = o[1];
		if (i >= 3)
			dest[dpos++] = o[2];
		dest[dpos] = 0;
		if (i < 3) {
			return (dpos);
		}
	}
}



/**
 * \brief Generate a new, globally unique UID parameter for a calendar etc. object
 *
 * \param buf String buffer into which our newly created UUID should be placed
 */
void generate_uuid(char *buf) {
	static int seq = 0;

	sprintf(buf, "%s-%lx-%lx-%x",
		serv_info.serv_nodename,
		(long)time(NULL),
		(long)getpid(),
		(seq++)
	);
}


/**
 * \brief Local replacement for controversial C library function that generates
 * names for temporary files.  Included to shut up compiler warnings.
 * \todo return a fd to the file instead of the name for security reasons
 * \param name the created filename
 * \param len the length of the filename
 */
void CtdlMakeTempFileName(char *name, int len) {
	int i = 0;

	while (i++, i < 100) {
		snprintf(name, len, "/tmp/ctdl.%04x.%04x",
			getpid(),
			rand()
		);
		if (!access(name, F_OK)) {
			return;
		}
	}
}



/*
 * \brief	case-insensitive substring search
 *
 *		This uses the Boyer-Moore search algorithm and is therefore quite fast.
 *		The code is roughly based on the strstr() replacement from 'tin' written
 *		by Urs Jannsen.
 *
 * \param	text	String to be searched
 * \param	pattern	String to search for
 */
char *bmstrcasestr(char *text, char *pattern) {

	register unsigned char *p, *t;
	register int i, j, *delta;
	register size_t p1;
	int deltaspace[256];
	size_t textlen;
	size_t patlen;

	textlen = strlen (text);
	patlen = strlen (pattern);

	/* algorithm fails if pattern is empty */
	if ((p1 = patlen) == 0)
		return (text);

	/* code below fails (whenever i is unsigned) if pattern too long */
	if (p1 > textlen)
		return (NULL);

	/* set up deltas */
	delta = deltaspace;
	for (i = 0; i <= 255; i++)
		delta[i] = p1;
	for (p = (unsigned char *) pattern, i = p1; --i > 0;)
		delta[tolower(*p++)] = i;

	/*
	 * From now on, we want patlen - 1.
	 * In the loop below, p points to the end of the pattern,
	 * t points to the end of the text to be tested against the
	 * pattern, and i counts the amount of text remaining, not
	 * including the part to be tested.
	 */
	p1--;
	p = (unsigned char *) pattern + p1;
	t = (unsigned char *) text + p1;
	i = textlen - patlen;
	while(1) {
		if (tolower(p[0]) == tolower(t[0])) {
			if (strncasecmp ((const char *)(p - p1), (const char *)(t - p1), p1) == 0) {
				return ((char *)t - p1);
			}
		}
		j = delta[tolower(t[0])];
		if (i < j)
			break;
		i -= j;
		t += j;
	}
	return (NULL);
}





/*@}*/
