/** 
 * \file serv_pager.h
 * @author Mathew McBride 
 *
 * Header file for serv_pager
 * Note some of these functions are also used by serv_funambol, so
 * this file needs to live in the top dir
 */
void notify_pager(long msgnum, void *userdata);
long pager_getConfigMessage(char *username);
int pager_isPagerAllowedByPrefs(long configMsgNum);
char *pager_getUserPhoneNumber(long configMsgNum);
int pager_isPagerAllowedByPrefs(long configMsgNum);
int pager_doesUserWant(long configMsgNum);
