void notify_pager(long msgnum, void *userdata);
long pager_getConfigMessage(char *username);
int pager_isPagerAllowedByPrefs(long configMsgNum);
char *pager_getUserPhoneNumber(long configMsgNum);
