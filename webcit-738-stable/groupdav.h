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
void groupdav_main(struct httprequest *, char *, int, char *);
void groupdav_get(char *);
void groupdav_put(char *, char *, char *, char *, int);
void groupdav_delete(char *, char *);
void groupdav_propfind(char *, int, char *, char *);
void groupdav_options(char *);
long locate_message_by_uid(char *);
void groupdav_folder_list(void);
void euid_escapize(char *, char *);
void euid_unescapize(char *, char *);
void groupdav_identify_host(void);
