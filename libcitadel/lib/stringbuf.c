#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/select.h>
#include <fcntl.h>
#define SHOW_ME_VAPPEND_PRINTF
#include <stdarg.h>
#include "libcitadel.h"


struct StrBuf {
	char *buf;
	long BufSize;
	long BufUsed;
	int ConstBuf;
};


inline const char *ChrPtr(const StrBuf *Str)
{
	if (Str == NULL)
		return "";
	return Str->buf;
}

inline int StrLength(const StrBuf *Str)
{
	return (Str != NULL) ? Str->BufUsed : 0;
}

StrBuf* NewStrBuf(void)
{
	StrBuf *NewBuf;

	NewBuf = (StrBuf*) malloc(sizeof(StrBuf));
	NewBuf->buf = (char*) malloc(SIZ);
	NewBuf->buf[0] = '\0';
	NewBuf->BufSize = SIZ;
	NewBuf->BufUsed = 0;
	NewBuf->ConstBuf = 0;
	return NewBuf;
}

StrBuf* NewStrBufDup(const StrBuf *CopyMe)
{
	StrBuf *NewBuf;
	
	if (CopyMe == NULL)
		return NewStrBuf();

	NewBuf = (StrBuf*) malloc(sizeof(StrBuf));
	NewBuf->buf = (char*) malloc(CopyMe->BufSize);
	memcpy(NewBuf->buf, CopyMe->buf, CopyMe->BufUsed + 1);
	NewBuf->BufUsed = CopyMe->BufUsed;
	NewBuf->BufSize = CopyMe->BufSize;
	NewBuf->ConstBuf = 0;
	return NewBuf;
}

StrBuf* NewStrBufPlain(const char* ptr, int nChars)
{
	StrBuf *NewBuf;
	size_t Siz = SIZ;
	size_t CopySize;

	NewBuf = (StrBuf*) malloc(sizeof(StrBuf));
	if (nChars < 0)
		CopySize = strlen((ptr != NULL)?ptr:"");
	else
		CopySize = nChars;

	while (Siz <= CopySize)
		Siz *= 2;

	NewBuf->buf = (char*) malloc(Siz);
	NewBuf->BufSize = Siz;
	if (ptr != NULL) {
		memcpy(NewBuf->buf, ptr, CopySize);
		NewBuf->buf[CopySize] = '\0';
		NewBuf->BufUsed = CopySize;
	}
	else {
		NewBuf->buf[0] = '\0';
		NewBuf->BufUsed = 0;
	}
	NewBuf->ConstBuf = 0;
	return NewBuf;
}


StrBuf* _NewConstStrBuf(const char* StringConstant, size_t SizeOfStrConstant)
{
	StrBuf *NewBuf;

	NewBuf = (StrBuf*) malloc(sizeof(StrBuf));
	NewBuf->buf = (char*) StringConstant;
	NewBuf->BufSize = SizeOfStrConstant;
	NewBuf->BufUsed = SizeOfStrConstant;
	NewBuf->ConstBuf = 1;
	return NewBuf;
}


static int IncreaseBuf(StrBuf *Buf, int KeepOriginal, int DestSize)
{
	char *NewBuf;
	size_t NewSize = Buf->BufSize * 2;

	if (Buf->ConstBuf)
		return -1;
		
	if (DestSize > 0)
		while (NewSize < DestSize)
			NewSize *= 2;

	NewBuf= (char*) malloc(NewSize);
	if (KeepOriginal)
	{
		memcpy(NewBuf, Buf->buf, Buf->BufUsed);
	}
	else
	{
		NewBuf[0] = '\0';
		Buf->BufUsed = 0;
	}
	free (Buf->buf);
	Buf->buf = NewBuf;
	Buf->BufSize *= 2;
	return Buf->BufSize;
}

int FlushStrBuf(StrBuf *buf)
{
	if (buf->ConstBuf)
		return -1;       
	buf->buf[0] ='\0';
	buf->BufUsed = 0;
	return 0;
}

void FreeStrBuf (StrBuf **FreeMe)
{
	if (*FreeMe == NULL)
		return;
	if (!(*FreeMe)->ConstBuf) 
		free((*FreeMe)->buf);
	free(*FreeMe);
	*FreeMe = NULL;
}

void HFreeStrBuf (void *VFreeMe)
{
	StrBuf *FreeMe = (StrBuf*)VFreeMe;
	if (FreeMe == NULL)
		return;
	if (!FreeMe->ConstBuf) 
		free(FreeMe->buf);
	free(FreeMe);
}

long StrTol(const StrBuf *Buf)
{
	if(Buf->BufUsed > 0)
		return atol(Buf->buf);
	else
		return 0;
}

int StrToi(const StrBuf *Buf)
{
	if(Buf->BufUsed > 0)
		return atoi(Buf->buf);
	else
		return 0;
}

int StrBufPlain(StrBuf *Buf, const char* ptr, int nChars)
{
	size_t Siz = Buf->BufSize;
	size_t CopySize;

	if (nChars < 0)
		CopySize = strlen(ptr);
	else
		CopySize = nChars;

	while (Siz <= CopySize)
		Siz *= 2;

	if (Siz != Buf->BufSize)
		IncreaseBuf(Buf, 0, Siz);
	memcpy(Buf->buf, ptr, CopySize);
	Buf->buf[CopySize] = '\0';
	Buf->BufUsed = CopySize;
	Buf->ConstBuf = 0;
	return CopySize;
}

void StrBufAppendBuf(StrBuf *Buf, const StrBuf *AppendBuf, size_t Offset)
{
	if ((AppendBuf == NULL) || (Buf == NULL))
		return;

	if (Buf->BufSize - Offset < AppendBuf->BufUsed + Buf->BufUsed)
		IncreaseBuf(Buf, 
			    (Buf->BufUsed > 0), 
			    AppendBuf->BufUsed + Buf->BufUsed);

	memcpy(Buf->buf + Buf->BufUsed, 
	       AppendBuf->buf + Offset, 
	       AppendBuf->BufUsed - Offset);
	Buf->BufUsed += AppendBuf->BufUsed - Offset;
	Buf->buf[Buf->BufUsed] = '\0';
}


void StrBufAppendBufPlain(StrBuf *Buf, const char *AppendBuf, long AppendSize, size_t Offset)
{
	long aps;

	if ((AppendBuf == NULL) || (Buf == NULL))
		return;

	if (AppendSize < 0 )
		aps = strlen(AppendBuf + Offset);
	else
		aps = AppendSize - Offset;

	if (Buf->BufSize < Buf->BufUsed + aps)
		IncreaseBuf(Buf, (Buf->BufUsed > 0), Buf->BufUsed + aps);

	memcpy(Buf->buf + Buf->BufUsed, 
	       AppendBuf + Offset, 
	       aps);
	Buf->BufUsed += aps;
	Buf->buf[Buf->BufUsed] = '\0';
}


inline int StrBufNum_tokens(const StrBuf *source, char tok)
{
	return num_tokens(source->buf, tok);
}


int StrBufSub(StrBuf *dest, const StrBuf *Source, size_t Offset, size_t nChars)
{
	size_t NCharsRemain;
	if (Offset > Source->BufUsed)
	{
		FlushStrBuf(dest);
		return 0;
	}
	if (Offset + nChars < Source->BufUsed)
	{
		if (nChars > dest->BufSize)
			IncreaseBuf(dest, 0, nChars + 1);
		memcpy(dest->buf, Source->buf + Offset, nChars);
		dest->BufUsed = nChars;
		dest->buf[dest->BufUsed] = '\0';
		return nChars;
	}
	NCharsRemain = Source->BufUsed - Offset;
	if (NCharsRemain > dest->BufSize)
		IncreaseBuf(dest, 0, NCharsRemain + 1);
	memcpy(dest->buf, Source->buf + Offset, NCharsRemain);
	dest->BufUsed = NCharsRemain;
	dest->buf[dest->BufUsed] = '\0';
	return NCharsRemain;
}


void StrBufVAppendPrintf(StrBuf *Buf, const char *format, va_list ap)
{
	size_t nWritten = Buf->BufSize + 1;
	size_t Offset = Buf->BufUsed;
	size_t newused = Offset + nWritten;
	
	while (newused >= Buf->BufSize) {
		nWritten = vsnprintf(Buf->buf + Offset, 
				     Buf->BufSize - Offset, 
				     format, ap);
		newused = Offset + nWritten;
		if (newused >= Buf->BufSize)
			IncreaseBuf(Buf, 1, 0);
		else
			Buf->BufUsed = Offset + nWritten ;

	}
}

void StrBufPrintf(StrBuf *Buf, const char *format, ...)
{
	size_t nWritten = Buf->BufSize + 1;
	va_list arg_ptr;
	
	while (nWritten >= Buf->BufSize) {
		va_start(arg_ptr, format);
		nWritten = vsnprintf(Buf->buf, Buf->BufSize, format, arg_ptr);
		va_end(arg_ptr);
		Buf->BufUsed = nWritten ;
		if (nWritten >= Buf->BufSize)
			IncreaseBuf(Buf, 0, 0);
	}
}


/**
 * \brief a string tokenizer
 * \param dest Destination StringBuffer
 * \param Source StringBuffer to read into
 * \param separator tokenizer param
 * \returns -1 if not found, else length of token.
 */
int StrBufExtract_token(StrBuf *dest, const StrBuf *Source, int parmnum, char separator)
{
	const char *s, *e;		//* source * /
	int len = 0;			//* running total length of extracted string * /
	int current_token = 0;		//* token currently being processed * /

	if ((Source == NULL) || (Source->BufUsed ==0)) {
		return(-1);
	}
	s = Source->buf;
	e = s + Source->BufUsed;
	if (dest == NULL) {
		return(-1);
	}

	//cit_backtrace();
	//lprintf (CTDL_DEBUG, "test >: n: %d sep: %c source: %s \n willi \n", parmnum, separator, source);
	dest->buf[0] = '\0';
	dest->BufUsed = 0;

	while ((s<e) && !IsEmptyStr(s)) {
		if (*s == separator) {
			++current_token;
		}
		if (len >= dest->BufSize)
			if (!IncreaseBuf(dest, 1, -1))
				break;
		if ( (current_token == parmnum) && 
		     (*s != separator)) {
			dest->buf[len] = *s;
			++len;
		}
		else if (current_token > parmnum) {
			break;
		}
		++s;
	}
	
	dest->buf[len] = '\0';
	dest->BufUsed = len;
		
	if (current_token < parmnum) {
		//lprintf (CTDL_DEBUG,"test <!: %s\n", dest);
		return(-1);
	}
	//lprintf (CTDL_DEBUG,"test <: %d; %s\n", len, dest);
	return(len);
}


/*
 * extract_int()  -  extract an int parm w/o supplying a buffer
 */
int StrBufExtract_int(const StrBuf* Source, int parmnum, char separator)
{
	StrBuf tmp;
	char buf[64];
	
	tmp.buf = buf;
	buf[0] = '\0';
	tmp.BufSize = 64;
	tmp.BufUsed = 0;
	if (StrBufExtract_token(&tmp, Source, parmnum, separator) > 0)
		return(atoi(buf));
	else
		return 0;
}

/*
 * extract_long()  -  extract an long parm w/o supplying a buffer
 */
long StrBufExtract_long(const StrBuf* Source, int parmnum, char separator)
{
	StrBuf tmp;
	char buf[64];
	
	tmp.buf = buf;
	buf[0] = '\0';
	tmp.BufSize = 64;
	tmp.BufUsed = 0;
	if (StrBufExtract_token(&tmp, Source, parmnum, separator) > 0)
		return(atoi(buf));
	else
		return 0;
}


/*
 * extract_unsigned_long() - extract an unsigned long parm
 */
unsigned long StrBufExtract_unsigned_long(const StrBuf* Source, int parmnum, char separator)
{
	StrBuf tmp;
	char buf[64];
	
	tmp.buf = buf;
	buf[0] = '\0';
	tmp.BufSize = 64;
	tmp.BufUsed = 0;
	if (StrBufExtract_token(&tmp, Source, parmnum, separator) > 0)
		return(atoi(buf));
	else 
		return 0;
}



/**
 * \brief Input binary data from socket
 * \param buf the buffer to get the input to
 * \param bytes the maximal number of bytes to read
 */
int StrBufTCP_read_line(StrBuf *buf, int *fd, int append, const char **Error)
{
	int len, rlen, slen;

	if (!append)
		FlushStrBuf(buf);

	slen = len = buf->BufUsed;
	while (1) {
		rlen = read(*fd, &buf->buf[len], 1);
		if (rlen < 1) {
			*Error = strerror(errno);
			
			close(*fd);
			*fd = -1;
			
			return -1;
		}
		if (buf->buf[len] == '\n')
			break;
		if (buf->buf[len] != '\r')
			len ++;
		if (!(len < buf->BufSize)) {
			buf->BufUsed = len;
			buf->buf[len+1] = '\0';
			IncreaseBuf(buf, 1, -1);
		}
	}
	buf->BufUsed = len;
	buf->buf[len] = '\0';
	return len - slen;
}

/**
 * \brief Input binary data from socket
 * \param buf the buffer to get the input to
 * \param bytes the maximal number of bytes to read
 */
int StrBufReadBLOB(StrBuf *Buf, int *fd, int append, long nBytes, const char **Error)
{
        fd_set wset;
        int fdflags;
	int len, rlen, slen;
	int nRead = 0;
	char *ptr;

	if ((Buf == NULL) || (*fd == -1))
		return -1;
	if (!append)
		FlushStrBuf(Buf);
	if (Buf->BufUsed + nBytes > Buf->BufSize)
		IncreaseBuf(Buf, 1, Buf->BufUsed + nBytes);

	ptr = Buf->buf + Buf->BufUsed;

	slen = len = Buf->BufUsed;

	fdflags = fcntl(*fd, F_GETFL);

	while (nRead < nBytes) {
               if ((fdflags & O_NONBLOCK) == O_NONBLOCK) {
                        FD_ZERO(&wset);
                        FD_SET(*fd, &wset);
                        if (select(*fd + 1, NULL, &wset, NULL, NULL) == -1) {
				*Error = strerror(errno);
                                return -1;
                        }
                }

                if ((rlen = read(*fd, 
				 ptr,
				 nBytes - nRead)) == -1) {
			close(*fd);
			*fd = -1;
			*Error = strerror(errno);
                        return rlen;
                }
		nRead += rlen;
		Buf->BufUsed += rlen;
	}
	Buf->buf[Buf->BufUsed] = '\0';
	return nRead;
}

void StrBufCutLeft(StrBuf *Buf, int nChars)
{
	if (nChars >= Buf->BufUsed) {
		FlushStrBuf(Buf);
		return;
	}
	memmove(Buf->buf, Buf->buf + nChars, Buf->BufUsed - nChars);
	Buf->BufUsed -= nChars;
	Buf->buf[Buf->BufUsed] = '\0';
}

void StrBufCutRight(StrBuf *Buf, int nChars)
{
	if (nChars >= Buf->BufUsed) {
		FlushStrBuf(Buf);
		return;
	}
	Buf->BufUsed -= nChars;
	Buf->buf[Buf->BufUsed] = '\0';
}


/*
 * string conversion function
 */
void StrBufEUid_unescapize(StrBuf *target, const StrBuf *source) 
{
	int a, b, len;
	char hex[3];

	if (target != NULL)
		FlushStrBuf(target);

	if (source == NULL ||target == NULL)
	{
		return;
	}

	len = source->BufUsed;
	for (a = 0; a < len; ++a) {
		if (target->BufUsed >= target->BufSize)
			IncreaseBuf(target, 1, -1);

		if (source->buf[a] == '=') {
			hex[0] = source->buf[a + 1];
			hex[1] = source->buf[a + 2];
			hex[2] = 0;
			b = 0;
			sscanf(hex, "%02x", &b);
			target->buf[target->BufUsed] = b;
			target->buf[++target->BufUsed] = 0;
			a += 2;
		}
		else {
			target->buf[target->BufUsed] = source->buf[a];
			target->buf[++target->BufUsed] = 0;
		}
	}
}


/*
 * string conversion function
 */
void StrBufEUid_escapize(StrBuf *target, const StrBuf *source) 
{
	int i, len;

	if (target != NULL)
		FlushStrBuf(target);

	if (source == NULL ||target == NULL)
	{
		return;
	}

	len = source->BufUsed;
	for (i=0; i<len; ++i) {
		if (target->BufUsed + 4 >= target->BufSize)
			IncreaseBuf(target, 1, -1);
		if ( (isalnum(source->buf[i])) || 
		     (source->buf[i]=='-') || 
		     (source->buf[i]=='_') ) {
			target->buf[target->BufUsed++] = source->buf[i];
		}
		else {
			sprintf(&target->buf[target->BufUsed], 
				"=%02X", 
				(0xFF &source->buf[i]));
			target->BufUsed += 3;
		}
	}
	target->buf[target->BufUsed + 1] = '\0';
}

/*
 * \brief uses the same calling syntax as compress2(), but it
 * creates a stream compatible with HTTP "Content-encoding: gzip"
 */
#ifdef HAVE_ZLIB
#define DEF_MEM_LEVEL 8 /*< memlevel??? */
#define OS_CODE 0x03	/*< unix */
int ZEXPORT compress_gzip(Bytef * dest,         /*< compressed buffer*/
			  size_t * destLen,     /*< length of the compresed data */
			  const Bytef * source, /*< source to encode */
			  uLong sourceLen,      /*< length of source to encode */
			  int level)            /*< compression level */
{
	const int gz_magic[2] = { 0x1f, 0x8b };	/* gzip magic header */

	/* write gzip header */
	snprintf((char *) dest, *destLen, 
		 "%c%c%c%c%c%c%c%c%c%c",
		 gz_magic[0], gz_magic[1], Z_DEFLATED,
		 0 /*flags */ , 0, 0, 0, 0 /*time */ , 0 /* xflags */ ,
		 OS_CODE);

	/* normal deflate */
	z_stream stream;
	int err;
	stream.next_in = (Bytef *) source;
	stream.avail_in = (uInt) sourceLen;
	stream.next_out = dest + 10L;	// after header
	stream.avail_out = (uInt) * destLen;
	if ((uLong) stream.avail_out != *destLen)
		return Z_BUF_ERROR;

	stream.zalloc = (alloc_func) 0;
	stream.zfree = (free_func) 0;
	stream.opaque = (voidpf) 0;

	err = deflateInit2(&stream, level, Z_DEFLATED, -MAX_WBITS,
			   DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY);
	if (err != Z_OK)
		return err;

	err = deflate(&stream, Z_FINISH);
	if (err != Z_STREAM_END) {
		deflateEnd(&stream);
		return err == Z_OK ? Z_BUF_ERROR : err;
	}
	*destLen = stream.total_out + 10L;

	/* write CRC and Length */
	uLong crc = crc32(0L, source, sourceLen);
	int n;
	for (n = 0; n < 4; ++n, ++*destLen) {
		dest[*destLen] = (int) (crc & 0xff);
		crc >>= 8;
	}
	uLong len = stream.total_in;
	for (n = 0; n < 4; ++n, ++*destLen) {
		dest[*destLen] = (int) (len & 0xff);
		len >>= 8;
	}
	err = deflateEnd(&stream);
	return err;
}
#endif


/**
 * Attention! If you feed this a Const String, you must maintain the uncompressed buffer yourself!
 */
int CompressBuffer(StrBuf *Buf)
{
#ifdef HAVE_ZLIB
	char *compressed_data = NULL;
	size_t compressed_len, bufsize;
	
	bufsize = compressed_len = ((Buf->BufUsed * 101) / 100) + 100;
	compressed_data = malloc(compressed_len);
	
	if (compress_gzip((Bytef *) compressed_data,
			  &compressed_len,
			  (Bytef *) Buf->buf,
			  (uLongf) Buf->BufUsed, Z_BEST_SPEED) == Z_OK) {
		if (!ConstBuf)
			free(Buf->buf);
		Buf->buf = compressed_data;
		Buf->BufUsed = compressed_len;
		Buf->BufSize = bufsize;
		return 1;
	} else {
		free(compressed_data);
	}
#endif	/* HAVE_ZLIB */
	return 0;
}

int StrBufDecodeBase64(StrBuf *Buf)
{
	char *xferbuf;
	size_t siz;
	if (Buf == NULL) return -1;

	xferbuf = (char*) malloc(Buf->BufSize);
	siz = CtdlDecodeBase64(xferbuf,
			       Buf->buf,
			       Buf->BufUsed);
	free(Buf->buf);
	Buf->buf = xferbuf;
	Buf->BufUsed = siz;
	return siz;
}


/*   
 * remove escaped strings from i.e. the url string (like %20 for blanks)
 */
long StrBufUnescape(StrBuf *Buf, int StripBlanks)
{
	int a, b;
	char hex[3];
	long len;

	while ((Buf->BufUsed > 0) && (isspace(Buf->buf[Buf->BufUsed - 1]))){
		Buf->buf[Buf->BufUsed - 1] = '\0';
		Buf->BufUsed --;
	}

	a = 0; 
	while (a < Buf->BufUsed) {
		if (Buf->buf[a] == '+')
			Buf->buf[a] = ' ';
		else if (Buf->buf[a] == '%') {
			/* don't let % chars through, rather truncate the input. */
			if (a + 2 > Buf->BufUsed) {
				Buf->buf[a] = '\0';
				Buf->BufUsed = a;
			}
			else {			
				hex[0] = Buf->buf[a + 1];
				hex[1] = Buf->buf[a + 2];
				hex[2] = 0;
				b = 0;
				sscanf(hex, "%02x", &b);
				Buf->buf[a] = (char) b;
				len = Buf->BufUsed - a - 2;
				if (len > 0)
					memmove(&Buf->buf[a + 1], &Buf->buf[a + 3], len);
			
				Buf->BufUsed -=2;
			}
		}
		a++;
	}
	return a;
}


/**
 * \brief	RFC2047-encode a header field if necessary.
 *		If no non-ASCII characters are found, the string
 *		will be copied verbatim without encoding.
 *
 * \param	target		Target buffer.
 * \param	source		Source string to be encoded.
 * \returns     encoded length; -1 if non success.
 */
int StrBufRFC2047encode(StrBuf **target, const StrBuf *source)
{
	const char headerStr[] = "=?UTF-8?Q?";
	int need_to_encode = 0;
	int i = 0;
	unsigned char ch;

	if ((source == NULL) || 
	    (target == NULL))
	    return -1;

	while ((i < source->BufUsed) &&
	       (!IsEmptyStr (&source->buf[i])) &&
	       (need_to_encode == 0)) {
		if (((unsigned char) source->buf[i] < 32) || 
		    ((unsigned char) source->buf[i] > 126)) {
			need_to_encode = 1;
		}
		i++;
	}

	if (!need_to_encode) {
		if (*target == NULL) {
			*target = NewStrBufPlain(source->buf, source->BufUsed);
		}
		else {
			FlushStrBuf(*target);
			StrBufAppendBuf(*target, source, 0);
		}
		return (*target)->BufUsed;
	}
	if (*target == NULL)
		*target = NewStrBufPlain(NULL, sizeof(headerStr) + source->BufUsed * 2);
	else if (sizeof(headerStr) + source->BufUsed > (*target)->BufSize)
		IncreaseBuf(*target, sizeof(headerStr) + source->BufUsed, 0);
	memcpy ((*target)->buf, headerStr, sizeof(headerStr) - 1);
	(*target)->BufUsed = sizeof(headerStr) - 1;
	for (i=0; (i < source->BufUsed); ++i) {
		if ((*target)->BufUsed + 4 > (*target)->BufSize)
			IncreaseBuf(*target, 1, 0);
		ch = (unsigned char) source->buf[i];
		if ((ch < 32) || (ch > 126) || (ch == 61)) {
			sprintf(&(*target)->buf[(*target)->BufUsed], "=%02X", ch);
			(*target)->BufUsed += 3;
		}
		else {
			(*target)->buf[(*target)->BufUsed] = ch;
			(*target)->BufUsed++;
		}
	}
	
	if ((*target)->BufUsed + 4 > (*target)->BufSize)
		IncreaseBuf(*target, 1, 0);

	(*target)->buf[(*target)->BufUsed++] = '?';
	(*target)->buf[(*target)->BufUsed++] = '=';
	(*target)->buf[(*target)->BufUsed] = '\0';
	return (*target)->BufUsed;;
}

void StrBufReplaceChars(StrBuf *buf, char search, char replace)
{
	long i;
	if (buf == NULL)
		return;
	for (i=0; i<buf->BufUsed; i++)
		if (buf->buf[i] == search)
			buf->buf[i] = replace;

}
