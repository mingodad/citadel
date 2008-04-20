/* 
 * File:   extnotify.h
 * Author: Mathew McBride <matt@mcbridematt.dhs.org> / <matt@comalies>
 *
 * Created on January 13, 2008, 9:34 PM
 */

#ifdef	__cplusplus
extern "C" {
#endif

#define FUNAMBOL_CONFIG_TEXT "funambol"
#define PAGER_CONFIG_MESSAGE "__ Push email settings __"
#define PAGER_CONFIG_TEXT  "textmessage"    

#define FUNAMBOL_WS "/funambol/services/admin"

int notify_funambol_server(char *user);

void extNotify_getPrefs(long configMsgNum, char *configMsg);
long extNotify_getConfigMessage(char *username);
void process_notify(long msgnum, void *usrdata);

#ifdef	__cplusplus
}
#endif


