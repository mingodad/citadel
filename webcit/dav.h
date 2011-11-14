/*
 * Data passed back and forth between dav_get() and its
 * callback functions called by the MIME parser
 */
struct epdata {
	char desired_content_type_1[128];
	char desired_content_type_2[128];
	char found_section[128];
	char charset[128];
};


void dav_common_headers(void);
void dav_main(void);
void dav_get(void);
void dav_put(void);
void dav_delete(void);
void dav_propfind(void);
void dav_options(void);

long locate_message_by_uid(const char *);
void dav_folder_list(void);
void euid_escapize(char *, const char *);
void euid_unescapize(char *, const char *);
void dav_identify_host(void);
void dav_identify_hosthdr(void);

void RegisterDAVNamespace(const char * UrlString, 
			  long UrlSLen, 
			  const char *DisplayName, 
			  long dslen, 
			  WebcitHandlerFunc F, 
			  WebcitRESTDispatchID RID,
			  long Flags);
