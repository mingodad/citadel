/* $Id$ */

void groupdav_common_headers(void);
void groupdav_main(struct httprequest *, char *, int, char *);
void groupdav_get(char *);
void groupdav_put(char *, char *, char *, char *);
void groupdav_delete(char *, char *);
void groupdav_propfind(char *);
long locate_message_by_uid(char *);
void groupdav_folder_list(void);
