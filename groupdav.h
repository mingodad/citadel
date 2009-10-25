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
void groupdav_main(void);
void groupdav_get(void);
void groupdav_put(void);
void groupdav_delete(void);
void groupdav_propfind(void);
void groupdav_options(void);

long locate_message_by_uid(const char *);
void groupdav_folder_list(void);
void euid_escapize(char *, const char *);
void euid_unescapize(char *, const char *);
void groupdav_identify_host(void);
void groupdav_identify_hosthdr(void);

void RegisterDAVNamespace(const char * UrlString, long UrlSLen, const char *DisplayName, long dslen, WebcitHandlerFunc F, long Flags);
