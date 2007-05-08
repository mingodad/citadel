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


int fuzzy_match(struct ctdluser *us, char *matchstring);
void process_rfc822_addr(const char *rfc822, char *user, char *node, char *name);
char *rfc822_fetch_field(char *rfc822, char *fieldname);

int IsDirectory(char *addr, int allow_masq_domains);
void CtdlDirectoryInit(void);
void CtdlDirectoryAddUser(char *internet_addr, char *citadel_addr);
void CtdlDirectoryDelUser(char *internet_addr, char *citadel_addr);
int CtdlDirectoryLookup(char *target, char *internet_addr, size_t targbuflen);
struct CtdlMessage *convert_internet_message(char *rfc822);
int CtdlHostAlias(char *fqdn);
char *harvest_collected_addresses(struct CtdlMessage *msg);

/* 
 * Values that can be returned by CtdlHostAlias()
 */
enum {
	hostalias_nomatch,
	hostalias_localhost,
	hostalias_gatewaydomain,
	hostalias_directory,
	hostalias_masq
};

extern char *inetcfg;


struct spamstrings_t {
	struct spamstrings_t *next;
	char *string;
};

extern struct spamstrings_t *spamstrings;

