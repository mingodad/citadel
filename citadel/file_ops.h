/* $Id$ */
void OpenCmdResult (char *, const char *);
void abort_upl (struct CitContext *who);

int network_talking_to(char *nodename, int operation);

/*
 * Operations that can be performed by network_talking_to()
 */
enum {
        NTT_ADD,
        NTT_REMOVE,
        NTT_CHECK
};
