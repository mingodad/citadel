/*
 * $Id$
 *
 */

void ft_index_message(long msgnum, int op);
void ft_search(int *fts_num_msgs, long **fts_msgs, char *search_string);
void *indexer_thread(void *arg);
