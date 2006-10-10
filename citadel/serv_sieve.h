/*
 * $Id: $
 */

extern struct RoomProcList *sieve_list;

void sieve_queue_room(struct ctdlroom *);
void perform_sieve_processing(void);

/* If you change this string you will break all of your Sieve configs. */
#define CTDLSIEVECONFIGSEPARATOR	"\n-=<CtdlSieveConfigSeparator>=-\n"
