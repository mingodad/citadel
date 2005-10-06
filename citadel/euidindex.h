/*
 * $Id$
 *
 * Index messages by EUID per room.
 *
 */

long locate_message_by_euid(char *euid, struct ctdlroom *qrbuf);
void index_message_by_euid(char *euid, struct ctdlroom *qrbuf, long msgnum);
void rebuild_euid_index(void);
void cmd_euid(char *cmdbuf);

