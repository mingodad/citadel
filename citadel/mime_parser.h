/*
 * $Id$
 *
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
		void *userdata,
		int dont_decode
		);
