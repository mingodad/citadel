/*
 * $Id$
 *
 */

/*
 * Here's a bunch of stupid magic to make the MIME parser portable between
 * Citadel and WebCit.
 */
#ifndef SIZ
#define SIZ	4096
#endif

#ifndef mallok
#define mallok(x) malloc(x)
#endif

#ifndef phree
#define phree(x) free(x)
#endif

#ifndef reallok
#define reallok(x,y) realloc(x,y)
#endif

#ifndef strdoop
#define strdoop(x) strdup(x)
#endif

/* 
 * Declarations for functions in the parser
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
			size_t cblength,
			char *cbencoding,
			void *cbuserdata),
		void *userdata,
		int dont_decode
		);
