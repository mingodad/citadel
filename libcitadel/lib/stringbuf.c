#include "../sysdep.h"
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

#ifdef HAVE_ICONV
#include <iconv.h>
#endif

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif


#ifdef HAVE_ZLIB
#include <zlib.h>
int ZEXPORT compress_gzip(Bytef * dest, size_t * destLen,
                          const Bytef * source, uLong sourceLen, int level);
#endif

/**
 * Private Structure for the Stringbuffer
 */
struct StrBuf {
	char *buf;         /**< the pointer to the dynamic buffer */
	long BufSize;      /**< how many spcae do we optain */
	long BufUsed;      /**< Number of Chars used excluding the trailing \0 */
	int ConstBuf;      /**< are we just a wrapper arround a static buffer and musn't we be changed? */
};


/** 
 * \Brief Cast operator to Plain String 
 * Note: if the buffer is altered by StrBuf operations, this pointer may become 
 *  invalid. So don't lean on it after altering the buffer!
 *  Since this operation is considered cheap, rather call it often than risking
 *  your pointer to become invalid!
 * \param Str the string we want to get the c-string representation for
 * \returns the Pointer to the Content. Don't mess with it!
 */
inline const char *ChrPtr(const StrBuf *Str)
{
	if (Str == NULL)
		return "";
	return Str->buf;
}

/**
 * \brief since we know strlen()'s result, provide it here.
 * \param Str the string to return the length to
 * \returns contentlength of the buffer
 */
inline int StrLength(const StrBuf *Str)
{
	return (Str != NULL) ? Str->BufUsed : 0;
}

/**
 * \brief local utility function to resize the buffer
 * \param Buf the buffer whichs storage we should increase
 * \param KeepOriginal should we copy the original buffer or just start over with a new one
 * \param DestSize what should fit in after?
 */
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
	if (KeepOriginal && (Buf->BufUsed > 0))
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

/**
 * Allocate a new buffer with default buffer size
 * \returns the new stringbuffer
 */
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

/** 
 * \brief Copy Constructor; returns a duplicate of CopyMe
 * \params CopyMe Buffer to faxmilate
 * \returns the new stringbuffer
 */
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

/**
 * \brief create a new Buffer using an existing c-string
 * this function should also be used if you want to pre-suggest
 * the buffer size to allocate in conjunction with ptr == NULL
 * \param ptr the c-string to copy; may be NULL to create a blank instance
 * \param nChars How many chars should we copy; -1 if we should measure the length ourselves
 * \returns the new stringbuffer
 */
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

/**
 * \brief Set an existing buffer from a c-string
 * \param ptr c-string to put into 
 * \param nChars set to -1 if we should work 0-terminated
 * \returns the new length of the string
 */
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


/**
 * \brief use strbuf as wrapper for a string constant for easy handling
 * \param StringConstant a string to wrap
 * \param SizeOfConstant should be sizeof(StringConstant)-1
 */
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


/**
 * \brief flush the content of a Buf; keep its struct
 * \param buf Buffer to flush
 */
int FlushStrBuf(StrBuf *buf)
{
	if (buf == NULL)
		return -1;
	if (buf->ConstBuf)
		return -1;       
	buf->buf[0] ='\0';
	buf->BufUsed = 0;
	return 0;
}

/**
 * \brief Release a Buffer
 * Its a double pointer, so it can NULL your pointer
 * so fancy SIG11 appear instead of random results
 * \param FreeMe Pointer Pointer to the buffer to free
 */
void FreeStrBuf (StrBuf **FreeMe)
{
	if (*FreeMe == NULL)
		return;
	if (!(*FreeMe)->ConstBuf) 
		free((*FreeMe)->buf);
	free(*FreeMe);
	*FreeMe = NULL;
}

/**
 * \brief Release the buffer
 * If you want put your StrBuf into a Hash, use this as Destructor.
 * \param VFreeMe untyped pointer to a StrBuf. be shure to do the right thing [TM]
 */
void HFreeStrBuf (void *VFreeMe)
{
	StrBuf *FreeMe = (StrBuf*)VFreeMe;
	if (FreeMe == NULL)
		return;
	if (!FreeMe->ConstBuf) 
		free(FreeMe->buf);
	free(FreeMe);
}

/**
 * \brief Wrapper around atol
 */
long StrTol(const StrBuf *Buf)
{
	if(Buf->BufUsed > 0)
		return atol(Buf->buf);
	else
		return 0;
}

/**
 * \brief Wrapper around atoi
 */
int StrToi(const StrBuf *Buf)
{
	if(Buf->BufUsed > 0)
		return atoi(Buf->buf);
	else
		return 0;
}

/**
 * \brief modifies a Single char of the Buf
 * You can point to it via char* or a zero-based integer
 * \param ptr char* to zero; use NULL if unused
 * \param nThChar zero based pointer into the string; use -1 if unused
 * \param PeekValue The Character to place into the position
 */
long StrBufPeek(StrBuf *Buf, const char* ptr, long nThChar, char PeekValue)
{
	if (Buf == NULL)
		return -1;
	if (ptr != NULL)
		nThChar = ptr - Buf->buf;
	if ((nThChar < 0) || (nThChar > Buf->BufUsed))
		return -1;
	Buf->buf[nThChar] = PeekValue;
	return nThChar;
}

/**
 * \brief Append a StringBuffer to the buffer
 * \param Buf Buffer to modify
 * \param AppendBuf Buffer to copy at the end of our buffer
 * \param Offset Should we start copying from an offset?
 */
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


/**
 * \brief Append a C-String to the buffer
 * \param Buf Buffer to modify
 * \param AppendBuf Buffer to copy at the end of our buffer
 * \param AppendSize number of bytes to copy; set to -1 if we should count it in advance
 * \param Offset Should we start copying from an offset?
 */
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


/** 
 * \brief Escape a string for feeding out as a URL while appending it to a Buffer
 * \param outbuf the output buffer
 * \param oblen the size of outbuf to sanitize
 * \param strbuf the input buffer
 */
void StrBufUrlescAppend(StrBuf *OutBuf, const StrBuf *In, const char *PlainIn)
{
	const char *pch, *pche;
	char *pt, *pte;
	int b, c, len;
	const char ec[] = " +#&;`'|*?-~<>^()[]{}/$\"\\";
	int eclen = sizeof(ec) -1;

	if (((In == NULL) && (PlainIn == NULL)) || (OutBuf == NULL) )
		return;
	if (PlainIn != NULL) {
		len = strlen(PlainIn);
		pch = PlainIn;
		pche = pch + len;
	}
	else {
		pch = In->buf;
		pche = pch + In->BufUsed;
		len = In->BufUsed;
	}

	if (len == 0) 
		return;

	pt = OutBuf->buf + OutBuf->BufUsed;
	pte = OutBuf->buf + OutBuf->BufSize - 4; /**< we max append 3 chars at once plus the \0 */

	while (pch < pche) {
		if (pt >= pte) {
			IncreaseBuf(OutBuf, 1, -1);
			pte = OutBuf->buf + OutBuf->BufSize - 4; /**< we max append 3 chars at once plus the \0 */
			pt = OutBuf->buf + OutBuf->BufUsed;
		}
		
		c = 0;
		for (b = 0; b < eclen; ++b) {
			if (*pch == ec[b]) {
				c = 1;
				b += eclen;
			}
		}
		if (c == 1) {
			sprintf(pt,"%%%02X", *pch);
			pt += 3;
			OutBuf->BufUsed += 3;
			pch ++;
		}
		else {
			*(pt++) = *(pch++);
			OutBuf->BufUsed++;
		}
	}
	*pt = '\0';
}

/*
 * \brief Append a string, escaping characters which have meaning in HTML.  
 *
 * \param Target	target buffer
 * \param Source	source buffer; set to NULL if you just have a C-String
 * \param PlainIn       Plain-C string to append; set to NULL if unused
 * \param nbsp		If nonzero, spaces are converted to non-breaking spaces.
 * \param nolinebreaks	if set, linebreaks are removed from the string.
 */
long StrEscAppend(StrBuf *Target, const StrBuf *Source, const char *PlainIn, int nbsp, int nolinebreaks)
{
	const char *aptr, *eiptr;
	char *bptr, *eptr;
	long len;

	if (((Source == NULL) && (PlainIn == NULL)) || (Target == NULL) )
		return -1;

	if (PlainIn != NULL) {
		aptr = PlainIn;
		len = strlen(PlainIn);
		eiptr = aptr + len;
	}
	else {
		aptr = Source->buf;
		eiptr = aptr + Source->BufUsed;
		len = Source->BufUsed;
	}

	if (len == 0) 
		return -1;

	bptr = Target->buf + Target->BufUsed;
	eptr = Target->buf + Target->BufSize - 6; /* our biggest unit to put in...  */

	while (aptr < eiptr){
		if(bptr >= eptr) {
			IncreaseBuf(Target, 1, -1);
			eptr = Target->buf + Target->BufSize - 6; 
			bptr = Target->buf + Target->BufUsed;
		}
		if (*aptr == '<') {
			memcpy(bptr, "&lt;", 4);
			bptr += 4;
			Target->BufUsed += 4;
		}
		else if (*aptr == '>') {
			memcpy(bptr, "&gt;", 4);
			bptr += 4;
			Target->BufUsed += 4;
		}
		else if (*aptr == '&') {
			memcpy(bptr, "&amp;", 5);
			bptr += 5;
			Target->BufUsed += 5;
		}
		else if (*aptr == '\"') {
			memcpy(bptr, "&quot;", 6);
			bptr += 6;
			Target->BufUsed += 6;
		}
		else if (*aptr == '\'') {
			memcpy(bptr, "&#39;", 5);
			bptr += 5;
			Target->BufUsed += 5;
		}
		else if (*aptr == LB) {
			*bptr = '<';
			bptr ++;
			Target->BufUsed ++;
		}
		else if (*aptr == RB) {
			*bptr = '>';
			bptr ++;
			Target->BufUsed ++;
		}
		else if (*aptr == QU) {
			*bptr ='"';
			bptr ++;
			Target->BufUsed ++;
		}
		else if ((*aptr == 32) && (nbsp == 1)) {
			memcpy(bptr, "&nbsp;", 6);
			bptr += 6;
			Target->BufUsed += 6;
		}
		else if ((*aptr == '\n') && (nolinebreaks)) {
			*bptr='\0';	/* nothing */
		}
		else if ((*aptr == '\r') && (nolinebreaks)) {
			*bptr='\0';	/* nothing */
		}
		else{
			*bptr = *aptr;
			bptr++;
			Target->BufUsed ++;
		}
		aptr ++;
	}
	*bptr = '\0';
	if ((bptr = eptr - 1 ) && !IsEmptyStr(aptr) )
		return -1;
	return Target->BufUsed;
}

/*
 * \brief Append a string, escaping characters which have meaning in HTML.  
 * Converts linebreaks into blanks; escapes single quotes
 * \param Target	target buffer
 * \param Source	source buffer; set to NULL if you just have a C-String
 * \param PlainIn       Plain-C string to append; set to NULL if unused
 */
void StrMsgEscAppend(StrBuf *Target, StrBuf *Source, const char *PlainIn)
{
	const char *aptr, *eiptr;
	char *tptr, *eptr;
	long len;

	if (((Source == NULL) && (PlainIn == NULL)) || (Target == NULL) )
		return ;

	if (PlainIn != NULL) {
		aptr = PlainIn;
		len = strlen(PlainIn);
		eiptr = aptr + len;
	}
	else {
		aptr = Source->buf;
		eiptr = aptr + Source->BufUsed;
		len = Source->BufUsed;
	}

	if (len == 0) 
		return;

	eptr = Target->buf + Target->BufSize - 6; 
	tptr = Target->buf + Target->BufUsed;
	
	while (aptr < eiptr){
		if(tptr >= eptr) {
			IncreaseBuf(Target, 1, -1);
			eptr = Target->buf + Target->BufSize - 6; 
			tptr = Target->buf + Target->BufUsed;
		}
	       
		if (*aptr == '\n') {
			*tptr = ' ';
			Target->BufUsed++;
		}
		else if (*aptr == '\r') {
			*tptr = ' ';
			Target->BufUsed++;
		}
		else if (*aptr == '\'') {
			*(tptr++) = '&';
			*(tptr++) = '#';
			*(tptr++) = '3';
			*(tptr++) = '9';
			*tptr = ';';
			Target->BufUsed += 5;
		} else {
			*tptr = *aptr;
			Target->BufUsed++;
		}
		tptr++; aptr++;
	}
	*tptr = '\0';
}


/**
 * \brief extracts a substring from Source into dest
 * \param dest buffer to place substring into
 * \param Source string to copy substring from
 * \param Offset chars to skip from start
 * \param nChars number of chars to copy
 * \returns the number of chars copied; may be different from nChars due to the size of Source
 */
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

/**
 * \brief sprintf like function appending the formated string to the buffer
 * vsnprintf version to wrap into own calls
 * \param Buf Buffer to extend by format and params
 * \param format printf alike format to add
 * \param ap va_list containing the items for format
 */
void StrBufVAppendPrintf(StrBuf *Buf, const char *format, va_list ap)
{
	va_list apl;
	size_t BufSize = Buf->BufSize;
	size_t nWritten = Buf->BufSize + 1;
	size_t Offset = Buf->BufUsed;
	size_t newused = Offset + nWritten;
	
	while (newused >= BufSize) {
		va_copy(apl, ap);
		nWritten = vsnprintf(Buf->buf + Offset, 
				     Buf->BufSize - Offset, 
				     format, apl);
		va_end(apl);
		newused = Offset + nWritten;
		if (newused >= Buf->BufSize) {
			IncreaseBuf(Buf, 1, newused);
		}
		else {
			Buf->BufUsed = Offset + nWritten;
			BufSize = Buf->BufSize;
		}

	}
}

/**
 * \brief sprintf like function appending the formated string to the buffer
 * \param Buf Buffer to extend by format and params
 * \param format printf alike format to add
 * \param ap va_list containing the items for format
 */
void StrBufAppendPrintf(StrBuf *Buf, const char *format, ...)
{
	size_t BufSize = Buf->BufSize;
	size_t nWritten = Buf->BufSize + 1;
	size_t Offset = Buf->BufUsed;
	size_t newused = Offset + nWritten;
	va_list arg_ptr;
	
	while (newused >= BufSize) {
		va_start(arg_ptr, format);
		nWritten = vsnprintf(Buf->buf + Buf->BufUsed, 
				     Buf->BufSize - Buf->BufUsed, 
				     format, arg_ptr);
		va_end(arg_ptr);
		newused = Buf->BufUsed + nWritten;
		if (newused >= Buf->BufSize) {
			IncreaseBuf(Buf, 1, newused);
		}
		else {
			Buf->BufUsed += nWritten;
			BufSize = Buf->BufSize;
		}

	}
}

/**
 * \brief sprintf like function putting the formated string into the buffer
 * \param Buf Buffer to extend by format and params
 * \param format printf alike format to add
 * \param ap va_list containing the items for format
 */
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
 * \brief Counts the numbmer of tokens in a buffer
 * \param Source String to count tokens in
 * \param tok    Tokenizer char to count
 * \returns numbers of tokenizer chars found
 */
inline int StrBufNum_tokens(const StrBuf *source, char tok)
{
	if (source == NULL)
		return 0;
	return num_tokens(source->buf, tok);
}

/*
 * remove_token() - a tokenizer that kills, maims, and destroys
 */
/**
 * \brief a string tokenizer
 * \param Source StringBuffer to read into
 * \param parmnum n'th parameter to remove
 * \param separator tokenizer param
 * \returns -1 if not found, else length of token.
 */
int StrBufRemove_token(StrBuf *Source, int parmnum, char separator)
{
	int ReducedBy;
	char *d, *s;		/* dest, source */
	int count = 0;

	/* Find desired parameter */
	d = Source->buf;
	while (count < parmnum) {
		/* End of string, bail! */
		if (!*d) {
			d = NULL;
			break;
		}
		if (*d == separator) {
			count++;
		}
		d++;
	}
	if (!d) return 0;		/* Parameter not found */

	/* Find next parameter */
	s = d;
	while (*s && *s != separator) {
		s++;
	}

	ReducedBy = d - s;

	/* Hack and slash */
	if (*s) {
		memmove(d, s, Source->BufUsed - (s - Source->buf));
		Source->BufUsed -= (ReducedBy + 1);
	}
	else if (d == Source->buf) {
		*d = 0;
		Source->BufUsed = 0;
	}
	else {
		*--d = 0;
		Source->BufUsed -= (ReducedBy + 1);
	}
	/*
	while (*s) {
		*d++ = *s++;
	}
	*d = 0;
	*/
	return ReducedBy;
}


/**
 * \brief a string tokenizer
 * \param dest Destination StringBuffer
 * \param Source StringBuffer to read into
 * \param parmnum n'th parameter to extract
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


/**
 * \brief a string tokenizer to fetch an integer
 * \param dest Destination StringBuffer
 * \param parmnum n'th parameter to extract
 * \param separator tokenizer param
 * \returns 0 if not found, else integer representation of the token
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

/**
 * \brief a string tokenizer to fetch a long integer
 * \param dest Destination StringBuffer
 * \param parmnum n'th parameter to extract
 * \param separator tokenizer param
 * \returns 0 if not found, else long integer representation of the token
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


/**
 * \brief a string tokenizer to fetch an unsigned long
 * \param dest Destination StringBuffer
 * \param parmnum n'th parameter to extract
 * \param separator tokenizer param
 * \returns 0 if not found, else unsigned long representation of the token
 */
unsigned long StrBufExtract_unsigned_long(const StrBuf* Source, int parmnum, char separator)
{
	StrBuf tmp;
	char buf[64];
	char *pnum;
	
	tmp.buf = buf;
	buf[0] = '\0';
	tmp.BufSize = 64;
	tmp.BufUsed = 0;
	if (StrBufExtract_token(&tmp, Source, parmnum, separator) > 0) {
		pnum = &buf[0];
		if (*pnum == '-')
			pnum ++;
		return (unsigned long) atol(pnum);
	}
	else 
		return 0;
}



/**
 * \brief Read a line from socket
 * flushes and closes the FD on error
 * \param buf the buffer to get the input to
 * \param fd pointer to the filedescriptor to read
 * \param append Append to an existing string or replace?
 * \param Error strerror() on error 
 * \returns numbers of chars read
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
 * \brief Read a line from socket
 * flushes and closes the FD on error
 * \param buf the buffer to get the input to
 * \param fd pointer to the filedescriptor to read
 * \param append Append to an existing string or replace?
 * \param Error strerror() on error 
 * \returns numbers of chars read
 */
int StrBufTCP_read_buffered_line(StrBuf *Line, 
				 StrBuf *buf, 
				 int *fd, 
				 int timeout, 
				 int selectresolution, 
				 const char **Error)
{
	int len, rlen;
	int nSuccessLess = 0;
	fd_set rfds;
	char *pch = NULL;
        int fdflags;
	struct timeval tv;

	if (buf->BufUsed > 0) {
		pch = strchr(buf->buf, '\n');
		if (pch != NULL) {
			rlen = 0;
			len = pch - buf->buf;
			if (len > 0 && (*(pch - 1) == '\r') )
				rlen ++;
			StrBufSub(Line, buf, 0, len - rlen);
			StrBufCutLeft(buf, len + 1);
			return len - rlen;
		}
	}
	
	if (buf->BufSize - buf->BufUsed < 10)
		IncreaseBuf(buf, 1, -1);

	fdflags = fcntl(*fd, F_GETFL);
	if ((fdflags & O_NONBLOCK) == O_NONBLOCK)
		return -1;

	while ((nSuccessLess < timeout) && (pch == NULL)) {
		tv.tv_sec = selectresolution;
		tv.tv_usec = 0;
		
		FD_ZERO(&rfds);
		FD_SET(*fd, &rfds);
		if (select(*fd + 1, NULL, &rfds, NULL, &tv) == -1) {
			*Error = strerror(errno);
			close (*fd);
			*fd = -1;
			return -1;
		}		
		if (FD_ISSET(*fd, &rfds)) {
			rlen = read(*fd, 
				    &buf->buf[buf->BufUsed], 
				    buf->BufSize - buf->BufUsed - 1);
			if (rlen < 1) {
				*Error = strerror(errno);
				close(*fd);
				*fd = -1;
				return -1;
			}
			else if (rlen > 0) {
				nSuccessLess = 0;
				buf->BufUsed += rlen;
				buf->buf[buf->BufUsed] = '\0';
				if (buf->BufUsed + 10 > buf->BufSize) {
					IncreaseBuf(buf, 1, -1);
				}
				pch = strchr(buf->buf, '\n');
				continue;
			}
		}
		nSuccessLess ++;
	}
	if (pch != NULL) {
		rlen = 0;
		len = pch - buf->buf;
		if (len > 0 && (*(pch - 1) == '\r') )
			rlen ++;
		StrBufSub(Line, buf, 0, len - rlen);
		StrBufCutLeft(buf, len + 1);
		return len - rlen;
	}
	return -1;

}

/**
 * \brief Input binary data from socket
 * flushes and closes the FD on error
 * \param buf the buffer to get the input to
 * \param fd pointer to the filedescriptor to read
 * \param append Append to an existing string or replace?
 * \param nBytes the maximal number of bytes to read
 * \param Error strerror() on error 
 * \returns numbers of chars read
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
		ptr += rlen;
		Buf->BufUsed += rlen;
	}
	Buf->buf[Buf->BufUsed] = '\0';
	return nRead;
}

/**
 * \brief Cut nChars from the start of the string
 * \param Buf Buffer to modify
 * \param nChars how many chars should be skipped?
 */
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

/**
 * \brief Cut the trailing n Chars from the string
 * \param Buf Buffer to modify
 * \param nChars how many chars should be trunkated?
 */
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
 * Strip leading and trailing spaces from a string; with premeasured and adjusted length.
 * buf - the string to modify
 * len - length of the string. 
 */
void StrBufTrim(StrBuf *Buf)
{
	int delta = 0;
	if ((Buf == NULL) || (Buf->BufUsed == 0)) return;

	while ((Buf->BufUsed > delta) && (isspace(Buf->buf[delta]))){
		delta ++;
	}
	if (delta > 0) StrBufCutLeft(Buf, delta);

	if (Buf->BufUsed == 0) return;
	while (isspace(Buf->buf[Buf->BufUsed - 1])){
		Buf->BufUsed --;
	}
	Buf->buf[Buf->BufUsed] = '\0';
}


void StrBufUpCase(StrBuf *Buf) 
{
	char *pch, *pche;

	pch = Buf->buf;
	pche = pch + Buf->BufUsed;
	while (pch < pche) {
		*pch = toupper(*pch);
		pch ++;
	}
}


/**
 * \brief unhide special chars hidden to the HTML escaper
 * \param target buffer to put the unescaped string in
 * \param source buffer to unescape
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


/**
 * \brief hide special chars from the HTML escapers and friends
 * \param target buffer to put the escaped string in
 * \param source buffer to escape
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
		if (!Buf->ConstBuf)
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

/**
 * \brief decode a buffer from base 64 encoding; destroys original
 * \param Buf Buffor to transform
 */
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


/**
 * \brief  remove escaped strings from i.e. the url string (like %20 for blanks)
 * \param Buf Buffer to translate
 * \param StripBlanks Reduce several blanks to one?
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

/**
 * \brief replaces all occurances of 'search' by 'replace'
 * \param buf Buffer to modify
 * \param search character to search
 * \param relpace character to replace search by
 */
void StrBufReplaceChars(StrBuf *buf, char search, char replace)
{
	long i;
	if (buf == NULL)
		return;
	for (i=0; i<buf->BufUsed; i++)
		if (buf->buf[i] == search)
			buf->buf[i] = replace;

}



/*
 * Wrapper around iconv_open()
 * Our version adds aliases for non-standard Microsoft charsets
 * such as 'MS950', aliasing them to names like 'CP950'
 *
 * tocode	Target encoding
 * fromcode	Source encoding
 */
void  ctdl_iconv_open(const char *tocode, const char *fromcode, void *pic)
{
#ifdef HAVE_ICONV
	iconv_t ic = (iconv_t)(-1) ;
	ic = iconv_open(tocode, fromcode);
	if (ic == (iconv_t)(-1) ) {
		char alias_fromcode[64];
		if ( (strlen(fromcode) == 5) && (!strncasecmp(fromcode, "MS", 2)) ) {
			safestrncpy(alias_fromcode, fromcode, sizeof alias_fromcode);
			alias_fromcode[0] = 'C';
			alias_fromcode[1] = 'P';
			ic = iconv_open(tocode, alias_fromcode);
		}
	}
	*(iconv_t *)pic = ic;
#endif
}



static inline char *FindNextEnd (StrBuf *Buf, char *bptr)
{
	char * end;
	/* Find the next ?Q? */
	if (Buf->BufUsed - (bptr - Buf->buf)  < 6)
		return NULL;

	end = strchr(bptr + 2, '?');

	if (end == NULL)
		return NULL;

	if ((Buf->BufUsed - (end - Buf->buf) > 3) &&
	    ((*(end + 1) == 'B') || (*(end + 1) == 'Q')) && 
	    (*(end + 2) == '?')) {
		/* skip on to the end of the cluster, the next ?= */
		end = strstr(end + 3, "?=");
	}
	else
		/* sort of half valid encoding, try to find an end. */
		end = strstr(bptr, "?=");
	return end;
}


void StrBufConvert(StrBuf *ConvertBuf, StrBuf *TmpBuf, void *pic)
{
#ifdef HAVE_ICONV
	int BufSize;
	iconv_t ic;
	char *ibuf;			/**< Buffer of characters to be converted */
	char *obuf;			/**< Buffer for converted characters */
	size_t ibuflen;			/**< Length of input buffer */
	size_t obuflen;			/**< Length of output buffer */


	if (ConvertBuf->BufUsed > TmpBuf->BufSize)
		IncreaseBuf(TmpBuf, 0, ConvertBuf->BufUsed);

	ic = *(iconv_t*)pic;
	ibuf = ConvertBuf->buf;
	ibuflen = ConvertBuf->BufUsed;
	obuf = TmpBuf->buf;
	obuflen = TmpBuf->BufSize;
	
	iconv(ic, &ibuf, &ibuflen, &obuf, &obuflen);

	/* little card game: wheres the red lady? */
	ibuf = ConvertBuf->buf;
	BufSize = ConvertBuf->BufSize;

	ConvertBuf->buf = TmpBuf->buf;
	ConvertBuf->BufSize = TmpBuf->BufSize;
	ConvertBuf->BufUsed = TmpBuf->BufSize - obuflen;
	ConvertBuf->buf[ConvertBuf->BufUsed] = '\0';
	
	TmpBuf->buf = ibuf;
	TmpBuf->BufSize = BufSize;
	TmpBuf->BufUsed = 0;
	TmpBuf->buf[0] = '\0';
#endif
}




inline static void DecodeSegment(StrBuf *Target, 
				 StrBuf *DecodeMe, 
				 char *SegmentStart, 
				 char *SegmentEnd, 
				 StrBuf *ConvertBuf,
				 StrBuf *ConvertBuf2, 
				 StrBuf *FoundCharset)
{
	StrBuf StaticBuf;
	char charset[128];
	char encoding[16];
	iconv_t ic = (iconv_t)(-1);

	/* Now we handle foreign character sets properly encoded
	 * in RFC2047 format.
	 */
	StaticBuf.buf = SegmentStart;
	StaticBuf.BufUsed = SegmentEnd - SegmentStart;
	StaticBuf.BufSize = DecodeMe->BufSize - (SegmentStart - DecodeMe->buf);
	extract_token(charset, SegmentStart, 1, '?', sizeof charset);
	if (FoundCharset != NULL) {
		FlushStrBuf(FoundCharset);
		StrBufAppendBufPlain(FoundCharset, charset, -1, 0);
	}
	extract_token(encoding, SegmentStart, 2, '?', sizeof encoding);
	StrBufExtract_token(ConvertBuf, &StaticBuf, 3, '?');
	
	*encoding = toupper(*encoding);
	if (*encoding == 'B') {	/**< base64 */
		ConvertBuf2->BufUsed = CtdlDecodeBase64(ConvertBuf2->buf, 
							ConvertBuf->buf, 
							ConvertBuf->BufUsed);
	}
	else if (*encoding == 'Q') {	/**< quoted-printable */
		long pos;
		
		pos = 0;
		while (pos < ConvertBuf->BufUsed)
		{
			if (ConvertBuf->buf[pos] == '_') 
				ConvertBuf->buf[pos] = ' ';
			pos++;
		}
		
		ConvertBuf2->BufUsed = CtdlDecodeQuotedPrintable(
			ConvertBuf2->buf, 
			ConvertBuf->buf,
			ConvertBuf->BufUsed);
	}
	else {
		StrBufAppendBuf(ConvertBuf2, ConvertBuf, 0);
	}

	ctdl_iconv_open("UTF-8", charset, &ic);
	if (ic != (iconv_t)(-1) ) {		
		StrBufConvert(ConvertBuf2, ConvertBuf, &ic);
		StrBufAppendBuf(Target, ConvertBuf2, 0);
		iconv_close(ic);
	}
	else {
		StrBufAppendBufPlain(Target, HKEY("(unreadable)"), 0);
	}
}
/*
 * Handle subjects with RFC2047 encoding such as:
 * =?koi8-r?B?78bP0s3Mxc7JxSDXz9rE1dvO2c3JINvB0sHNySDP?=
 */
void StrBuf_RFC822_to_Utf8(StrBuf *Target, StrBuf *DecodeMe, const StrBuf* DefaultCharset, StrBuf *FoundCharset)
{
	StrBuf *ConvertBuf, *ConvertBuf2;
	char *start, *end, *next, *nextend, *ptr = NULL;
	iconv_t ic = (iconv_t)(-1) ;
	const char *eptr;
	int passes = 0;
	int i, len, delta;
	int illegal_non_rfc2047_encoding = 0;

	/* Sometimes, badly formed messages contain strings which were simply
	 *  written out directly in some foreign character set instead of
	 *  using RFC2047 encoding.  This is illegal but we will attempt to
	 *  handle it anyway by converting from a user-specified default
	 *  charset to UTF-8 if we see any nonprintable characters.
	 */
	
	len = StrLength(DecodeMe);
	for (i=0; i<DecodeMe->BufUsed; ++i) {
		if ((DecodeMe->buf[i] < 32) || (DecodeMe->buf[i] > 126)) {
			illegal_non_rfc2047_encoding = 1;
			break;
		}
	}

	ConvertBuf = NewStrBufPlain(NULL, StrLength(DecodeMe));
	if ((illegal_non_rfc2047_encoding) &&
	    (strcasecmp(ChrPtr(DefaultCharset), "UTF-8")) && 
	    (strcasecmp(ChrPtr(DefaultCharset), "us-ascii")) )
	{
		ctdl_iconv_open("UTF-8", ChrPtr(DefaultCharset), &ic);
		if (ic != (iconv_t)(-1) ) {
			StrBufConvert(DecodeMe, ConvertBuf, &ic);
			iconv_close(ic);
		}
	}

	/* pre evaluate the first pair */
	nextend = end = NULL;
	len = StrLength(DecodeMe);
	start = strstr(DecodeMe->buf, "=?");
	eptr = DecodeMe->buf + DecodeMe->BufUsed;
	if (start != NULL) 
		end = FindNextEnd (DecodeMe, start);
	else {
		StrBufAppendBuf(Target, DecodeMe, 0);
		FreeStrBuf(&ConvertBuf);
		return;
	}

	ConvertBuf2 = NewStrBufPlain(NULL, StrLength(DecodeMe));

	if (start != DecodeMe->buf)
		StrBufAppendBufPlain(Target, DecodeMe->buf, start - DecodeMe->buf, 0);
	/*
	 * Since spammers will go to all sorts of absurd lengths to get their
	 * messages through, there are LOTS of corrupt headers out there.
	 * So, prevent a really badly formed RFC2047 header from throwing
	 * this function into an infinite loop.
	 */
	while ((start != NULL) && 
	       (end != NULL) && 
	       (start < eptr) && 
	       (end < eptr) && 
	       (passes < 20))
	{
		passes++;
		DecodeSegment(Target, 
			      DecodeMe, 
			      start, 
			      end, 
			      ConvertBuf,
			      ConvertBuf2,
			      FoundCharset);
		
		next = strstr(end, "=?");
		nextend = NULL;
		if ((next != NULL) && 
		    (next < eptr))
			nextend = FindNextEnd(DecodeMe, next);
		if (nextend == NULL)
			next = NULL;

		/* did we find two partitions */
		if ((next != NULL) && 
		    ((next - end) > 2))
		{
			ptr = end + 2;
			while ((ptr < next) && 
			       (isspace(*ptr) ||
				(*ptr == '\r') ||
				(*ptr == '\n') || 
				(*ptr == '\t')))
				ptr ++;
			/* did we find a gab just filled with blanks? */
			if (ptr == next)
			{
				memmove (end + 2,
					 next,
					 len - (next - start));
				
				/* now terminate the gab at the end */
				delta = (next - end) - 2;
				DecodeMe->BufUsed -= delta;
				DecodeMe->buf[DecodeMe->BufUsed] = '\0';

				/* move next to its new location. */
				next -= delta;
				nextend -= delta;
			}
		}
		/* our next-pair is our new first pair now. */
		ptr = end + 2;
		start = next;
		end = nextend;
	}
	end = ptr;
	nextend = DecodeMe->buf + DecodeMe->BufUsed;
	if ((end != NULL) && (end < nextend)) {
		ptr = end;
		while ( (ptr < nextend) &&
			(isspace(*ptr) ||
			 (*ptr == '\r') ||
			 (*ptr == '\n') || 
			 (*ptr == '\t')))
			ptr ++;
		if (ptr < nextend)
			StrBufAppendBufPlain(Target, end, nextend - end, 0);
	}
	FreeStrBuf(&ConvertBuf);
	FreeStrBuf(&ConvertBuf2);
}



long StrBuf_Utf8StrLen(StrBuf *Buf)
{
	return Ctdl_Utf8StrLen(Buf->buf);
}

long StrBuf_Utf8StrCut(StrBuf *Buf, int maxlen)
{
	char *CutAt;

	CutAt = Ctdl_Utf8StrCut(Buf->buf, maxlen);
	if (CutAt != NULL) {
		Buf->BufUsed = CutAt - Buf->buf;
		Buf->buf[Buf->BufUsed] = '\0';
	}
	return Buf->BufUsed;	
}



int StrBufSipLine(StrBuf *LineBuf, StrBuf *Buf, const char **Ptr)
{
	const char *aptr, *ptr, *eptr;
	char *optr, *xptr;

	if (Buf == NULL)
		return 0;

	if (*Ptr==NULL)
		ptr = aptr = Buf->buf;
	else
		ptr = aptr = *Ptr;

	optr = LineBuf->buf;
	eptr = Buf->buf + Buf->BufUsed;
	xptr = LineBuf->buf + LineBuf->BufSize;

	while ((*ptr != '\n') &&
	       (*ptr != '\r') &&
	       (ptr < eptr))
	{
		*optr = *ptr;
		optr++; ptr++;
		if (optr == xptr) {
			LineBuf->BufUsed = optr - LineBuf->buf;
			IncreaseBuf(LineBuf,  1, LineBuf->BufUsed + 1);
			optr = LineBuf->buf + LineBuf->BufUsed;
			xptr = LineBuf->buf + LineBuf->BufSize;
		}
	}
	LineBuf->BufUsed = optr - LineBuf->buf;
	*optr = '\0';       
	if (*ptr == '\r')
		ptr ++;
	if (*ptr == '\n')
		ptr ++;

	*Ptr = ptr;

	return Buf->BufUsed - (ptr - Buf->buf);
}
