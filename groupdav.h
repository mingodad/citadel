/* $Id$ */


/*
 * Data passed back and forth between groupdav_get() and its
 * callback functions called by the MIME parser
 */
struct epdata {
	char desired_content_type_1[128];
	char desired_content_type_2[128];
	char found_section[128];
	char charset[128];
};


void groupdav_common_headers(void);
void groupdav_main(HashList *HTTPHeaders,
		   StrBuf *DavPathname,
		   StrBuf *DavMethod,
		   StrBuf *dav_content_type,
		   int dav_content_length,
		   StrBuf *dav_content,
		   int Offset);
void groupdav_get(StrBuf *dav_pathname);
void groupdav_put(StrBuf *dav_pathname, char *dav_ifmatch,
		  const char *dav_content_type, StrBuf *dav_content,
		  int offset);
void groupdav_delete(StrBuf *dav_pathname, char *dav_ifmatch);
void groupdav_propfind(StrBuf *dav_pathname, int dav_depth, StrBuf *dav_content_type, StrBuf *dav_content, int offset);
void groupdav_options(StrBuf *dav_pathname);
long locate_message_by_uid(const char *);
void groupdav_folder_list(void);
void euid_escapize(char *, const char *);
void euid_unescapize(char *, const char *);
void groupdav_identify_host(void);
