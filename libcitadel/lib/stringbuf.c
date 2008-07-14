#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include "libcitadel.h"

#include <stdarg.h>

struct StrBuf {
	char *buf;
	long BufSize;
	long BufUsed;
	int ConstBuf;
};


inline const char *ChrPtr(StrBuf *Str)
{
	if (Str == NULL)
		return "";
	return Str->buf;
}

inline int StrLength(StrBuf *Str)
{
	return Str->BufUsed;
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


StrBuf* NewStrBufPlain(const char* ptr, int nChars)
{
	StrBuf *NewBuf;
	size_t Siz = SIZ;
	size_t CopySize;

	NewBuf = (StrBuf*) malloc(sizeof(StrBuf));
	if (nChars < 0)
		CopySize = strlen(ptr);
	else
		CopySize = nChars;

	while (Siz <= CopySize)
		Siz *= 2;

	NewBuf->buf = (char*) malloc(Siz);
	memcpy(NewBuf->buf, ptr, CopySize);
	NewBuf->buf[CopySize] = '\0';
	NewBuf->BufSize = Siz;
	NewBuf->BufUsed = CopySize;
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

long StrTol(StrBuf *Buf)
{
	if(Buf->BufUsed > 0)
		return atol(Buf->buf);
	else
		return 0;
}


void StrBufAppendBuf(StrBuf *Buf, StrBuf *AppendBuf, size_t Offset)
{
	if ((AppendBuf == NULL) || (Buf == NULL))
		return;
	if (Buf->BufSize - Offset < AppendBuf->BufUsed)
		IncreaseBuf(Buf, (Buf->BufUsed > 0), AppendBuf->BufUsed);
	memcpy(Buf->buf + Buf->BufUsed, 
	       AppendBuf->buf + Offset, 
	       AppendBuf->BufUsed - Offset);
	Buf->BufUsed += AppendBuf->BufUsed - Offset;
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
int StrBufTCP_read_line(StrBuf *buf, int fd, int append, const char **Error)
{
	int len, rlen, slen;

	if (!append)
		FlushStrBuf(buf);

	slen = len = buf->BufUsed;
	while (1) {
		rlen = read(fd, &buf->buf[len], 1);
		if (rlen < 1) {
			*Error = strerror(errno);
			
			close(fd);
			
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

void StrBufCutLeft(StrBuf *Buf, int nChars)
{
	if (nChars >= Buf->BufUsed) {
		FlushStrBuf(Buf);
		return;
	}
	memmove(Buf->buf, Buf->buf + nChars, Buf->BufUsed - nChars);
	Buf->BufUsed -= nChars;
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
void StrBufEUid_unescapize(StrBuf *target, StrBuf *source) 
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
void StrBufEUid_escapize(StrBuf *target, StrBuf *source) 
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
