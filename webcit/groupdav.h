/* $Id$ */

void groupdav_common_headers(void);
void groupdav_main(struct httprequest *);
void groupdav_get(char *);
void groupdav_propfind(char *);
long locate_message_by_uid(char *);
