/* 
 * $Id$
 *
 */


void determine_pwfilename(char *pwfile, size_t n);
void get_stored_password(
		char *host,
		char *port,
		char *username,
		char *password);
void set_stored_password(
		char *host,
		char *port,
		char *username,
		char *password);
void offer_to_remember_password(CtdlIPC *ipc,
		char *host,
		char *port,
		char *username,
		char *password);
