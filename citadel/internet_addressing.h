/* 
 * $Id$
 *
 */

#include "server.h"

struct internet_address_list {
	struct internet_address_list *next;
	char ial_user[SIZ];
	char ial_node[SIZ];
	char ial_name[SIZ];
};


int fuzzy_match(struct usersupp *us, char *matchstring);
void process_rfc822_addr(char *rfc822, char *user, char *node, char *name);
char *rfc822_fetch_field(char *rfc822, char *fieldname);


int convert_internet_address(char *destuser, char *desthost, char *source);
enum {
	rfc822_address_locally_validated,
	rfc822_no_such_user,
	rfc822_address_on_citadel_network,
	rfc822_address_nonlocal,
	rfc822_room_delivery
};


struct CtdlMessage *convert_internet_message(char *rfc822);

int CtdlHostAlias(char *fqdn);

/* 
 * Values that can be returned by CtdlHostAlias()
 */
enum {
	hostalias_nomatch,
	hostalias_localhost,
	hostalias_gatewaydomain,
	hostalias_directory
};

extern DLEXP char *inetcfg;
