int fuzzy_match(struct usersupp *us, char *matchstring);
void process_rfc822_addr(char *rfc822, char *user, char *node, char *name);


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
	hostalias_gatewaydomain
};

extern char *inetcfg;
