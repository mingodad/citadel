/*
 * $Id$
 *
 */

void imap_pick_range(char *range, int is_uid);
void imap_fetch(int num_parms, char *parms[]);
void imap_uidfetch(int num_parms, char *parms[]);
int imap_extract_data_items(char **argv, char *items);
void imap_output_flags(int num);
