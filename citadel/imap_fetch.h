/*
 * $Id$
 *
 */

void imap_pick_range(char *range, int is_uid);
void imap_fetch(int num_parms, char *parms[]);
void imap_uidfetch(int num_parms, char *parms[]);
void imap_fetch_flags(int seq);
int imap_extract_data_items(char **argv, char *items);
