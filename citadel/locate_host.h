/* $Id$ */
void locate_host(char *tbuf, size_t n,
		char *abuf, size_t na,
		const struct in_addr *addr);
int rbl_check(char *message_to_spammer);
int hostname_to_dotted_quad(char *addr, char *host);
