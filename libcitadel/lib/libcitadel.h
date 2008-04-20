/*
 * $Id$
 *
 */


/*
 * since we reference time_t...
 */
#include <time.h>
#include <stdlib.h>
#define LIBCITADEL_VERSION_NUMBER	109

/*
 * Here's a bunch of stupid magic to make the MIME parser portable.
 */
#ifndef SIZ
#define SIZ	4096
#endif


/*
 * View definitions.
 * Note that not all views are implemented in all clients.
 */
#define	VIEW_BBS		0	/* Bulletin board view */
#define VIEW_MAILBOX		1	/* Mailbox summary */
#define VIEW_ADDRESSBOOK	2	/* Address book view */
#define VIEW_CALENDAR		3	/* Calendar view */
#define VIEW_TASKS		4	/* Tasks view */
#define VIEW_NOTES		5	/* Notes view */
#define	VIEW_WIKI		6	/* Wiki view */
#define VIEW_CALBRIEF		7	/* Brief Calendar view */
#define VIEW_JOURNAL		8	/* Journal view */


#ifndef IsEmptyStr
#define IsEmptyStr(a) ((a)[0] == '\0')
#endif

/*
 * Misc declarations
 */

char *libcitadel_version_string(void);
int libcitadel_version_number(void);


/*
 * MIME parser declarations
 */

void extract_key(char *target, char *source, char *key);

void mime_parser(char *content_start, char *content_end,
		void (*CallBack)
			(char *cbname,
			char *cbfilename,
			char *cbpartnum,
			char *cbdisp,
			void *cbcontent,
			char *cbtype,
			char *cbcharset,
			size_t cblength,
			char *cbencoding,
			void *cbuserdata),
		void (*PreMultiPartCallBack)
			(char *cbname,
			char *cbfilename,
			char *cbpartnum,
			char *cbdisp,
			void *cbcontent,
			char *cbtype,
			char *cbcharset,
			size_t cblength,
			char *cbencoding,
			void *cbuserdata),
		void (*PostMultiPartCallBack)
			(char *cbname,
			char *cbfilename,
			char *cbpartnum,
			char *cbdisp,
			void *cbcontent,
			char *cbtype,
			char *cbcharset,
			size_t cblength,
			char *cbencoding,
			void *cbuserdata),
		void *userdata,
		int dont_decode
		);



char *fixed_partnum(char *);
void mime_decode(char *partnum,
		 char *part_start, size_t length,
		 char *content_type, char *charset, char *encoding,
		 char *disposition,
		 char *name, char *filename,
		 void (*CallBack)
		  (char *cbname,
		   char *cbfilename,
		   char *cbpartnum,
		   char *cbdisp,
		   void *cbcontent,
		   char *cbtype,
		   char *cbcharset,
		   size_t cblength,
		   char *cbencoding,
		   void *cbuserdata),
		 void (*PreMultiPartCallBack)
		  (char *cbname,
		   char *cbfilename,
		   char *cbpartnum,
		   char *cbdisp,
		   void *cbcontent,
		   char *cbtype,
		   char *cbcharset,
		   size_t cblength,
		   char *cbencoding,
		   void *cbuserdata),
		 void (*PostMultiPartCallBack)
		  (char *cbname,
		   char *cbfilename,
		   char *cbpartnum,
		   char *cbdisp,
		   void *cbcontent,
		   char *cbtype,
		   char *cbcharset,
		   size_t cblength,
		   char *cbencoding,
		   void *cbuserdata),
		  void *userdata,
		  int dont_decode
);
void the_mime_parser(char *partnum,
		     char *content_start, char *content_end,
		     void (*CallBack)
		      (char *cbname,
		       char *cbfilename,
		       char *cbpartnum,
		       char *cbdisp,
		       void *cbcontent,
		       char *cbtype,
		       char *cbcharset,
		       size_t cblength,
		       char *cbencoding,
		       void *cbuserdata),
		     void (*PreMultiPartCallBack)
		      (char *cbname,
		       char *cbfilename,
		       char *cbpartnum,
		       char *cbdisp,
		       void *cbcontent,
		       char *cbtype,
		       char *cbcharset,
		       size_t cblength,
		       char *cbencoding,
		       void *cbuserdata),
		     void (*PostMultiPartCallBack)
		      (char *cbname,
		       char *cbfilename,
		       char *cbpartnum,
		       char *cbdisp,
		       void *cbcontent,
		       char *cbtype,
		       char *cbcharset,
		       size_t cblength,
		       char *cbencoding,
		       void *cbuserdata),
		      void *userdata,
		      int dont_decode
);

const char *GuessMimeType(char *data, size_t dlen);
const char* GuessMimeByFilename(const char *what, size_t len);

/* Run once at Programstart */
int LoadIconDir(const char *DirName);

/* Select the icon for a given MIME type */
const char *GetIconFilename(char *MimeType, size_t len);

void ShutDownLibCitadel(void);


/* tools */

int Ctdl_IsUtf8SequenceStart(char Char);
int Ctdl_GetUtf8SequenceLength(char Char);
int Ctdl_Utf8StrLen(char *str);
char *Ctdl_Utf8StrCut(char *str, int maxlen);


int safestrncpy(char *dest, const char *src, size_t n);
int num_tokens (const char *source, char tok);
long extract_token(char *dest, const char *source, int parmnum, char separator, int maxlen);
long grab_token(char **dest, const char *source, int parmnum, char separator);
int extract_int (const char *source, int parmnum);
long extract_long (const char *source, int parmnum);
unsigned long extract_unsigned_long(const char *source, int parmnum);
void CtdlInitBase64Table(void);
size_t CtdlEncodeBase64(char *dest, const char *source, size_t sourcelen, int linebreaks);
int CtdlDecodeBase64(char *dest, const char *source, size_t length);
int CtdlDecodeQuotedPrintable(char *decoded, char *encoded, int sourcelen);
void striplt(char *);
int haschar(const char *st, int ch);
void remove_token(char *source, int parmnum, char separator);
void fmt_date(char *buf, size_t n, time_t thetime, int seconds);
int is_msg_in_sequence_set(char *mset, long msgnum);
char *memreadline(char *start, char *buf, int maxlen);
char *memreadlinelen(char *start, char *buf, int maxlen, int *retlen);
#define IsEmptyStr(a) ((a)[0] == '\0')
#define num_parms(source)		num_tokens(source,(char)'|')
void stripout(char *str, char leftboundary, char rightboundary);
void stripallbut(char *str, char leftboundary, char rightboundary);
char *myfgets(char *s, int size, FILE *stream);
void urlesc(char *outbuf, size_t oblen, char *strbuf);
char *CtdlTempFileName(char *prefix1, int prefix2);
FILE *CtdlTempFile(void);
void generate_uuid(char *buf);
char *bmstrcasestr(char *text, char *pattern);
void CtdlMakeTempFileName(char *name, int len);
char *rfc2047encode(char *line, long length);
int is_msg_in_mset(char *mset, long msgnum);
int pattern2(char *search, char *patn);
void stripltlen(char *, int *);
char *html_to_ascii(char *inputmsg, int msglen, int screenwidth, int do_citaformat);



/* vCard stuff */

#define CTDL_VCARD_MAGIC	0xa1f9

/* This data structure represents a vCard object currently in memory. */
struct vCard {
	int magic;
	int numprops;
	struct vCardProp {
		char *name;
		char *value;
	} *prop;
};


struct vCard *vcard_new(void);
void vcard_add_prop(struct vCard *v, char *propname, char *propvalue);
struct vCard *vcard_load(char *);
void vcard_free(struct vCard *);
void vcard_set_prop(struct vCard *v, char *name, char *value, int append);
char *vcard_get_prop(struct vCard *v, char *propname, int is_partial,
			int instance, int return_propname);
char *vcard_serialize(struct vCard *);
void vcard_fn_to_n(char *vname, char *n, size_t vname_size);
void remove_charset_attribute(char *strbuf);

/*
 * Hash list implementation for Citadel
 */
#define HKEY(a) a, sizeof(a) - 1
typedef struct HashList HashList;

typedef struct HashKey HashKey;

typedef struct HashPos HashPos;

typedef void (*DeleteHashDataFunc)(void * Data);
typedef const char *(*PrintHashContent)(void * Data);
typedef int (*CompareFunc)(const void* Item1, const void*Item2);

HashList *NewHash(void);

void DeleteHash(HashList **Hash);

int GetHash(HashList *Hash, const char *HKey, long HKLen, void **Data);

void Put(HashList *Hash, char *HKey, long HKLen, void *Data, DeleteHashDataFunc DeleteIt);

int GetKey(HashList *Hash, char *HKey, long HKLen, void **Data);

int GetHashKeys(HashList *Hash, char ***List);

int PrintHash(HashList *Hash, PrintHashContent first, PrintHashContent Second);

HashPos *GetNewHashPos(void);

void DeleteHashPos(HashPos **DelMe);

int GetNextHashPos(HashList *Hash, HashPos *At, long *HKLen, char **HashKey, void **Data);

void SortByHashKey(HashList *Hash);
void SortByHashKeyStr(HashList *Hash);

const void *GetSearchPayload(const void *HashVoid);
void SortByPayload(HashList *Hash, CompareFunc SortBy);

void convert_spaces_to_underscores(char *str);



/* vNote implementation */

#define CTDL_VNOTE_MAGIC	0xa1fa

struct vnote {
	int magic;
	char *uid;
	char *summary;
	char *body;
	int pos_left;
	int pos_top;
	int pos_width;
	int pos_height;
	int color_red;
	int color_green;
	int color_blue;
};



struct vnote *vnote_new(void);
struct vnote *vnote_new_from_str(char *s);
void vnote_free(struct vnote *v);
char *vnote_serialize(struct vnote *v);
void vnote_serialize_output_field(char *append_to, char *field, char *label);
