
/*
 * In the extended form of LIST the client is allowed to specify
 * multiple match patterns.  How many will we allow?
 */
#define MAX_PATTERNS 20

void imap_list(int num_parms, ConstStr *Params);
