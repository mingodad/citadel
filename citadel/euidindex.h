/*
 * Index messages by EUID per room.
 */

int DoesThisRoomNeedEuidIndexing(struct ctdlroom *qrbuf);
/* locate_message_by_euid is deprecated. Use CtdlLocateMessageByEuid instead */
long locate_message_by_euid(char *euid, struct ctdlroom *qrbuf) __attribute__ ((deprecated));
void index_message_by_euid(char *euid, struct ctdlroom *qrbuf, long msgnum);
void rebuild_euid_index(void);
