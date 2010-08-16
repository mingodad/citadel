/* $Id$ */
void locate_host(char *tbuf, size_t n, char *abuf, size_t na, int client_socket);
int rbl_check(char *message_to_spammer);
int hostname_to_dotted_quad(char *addr, char *host);
int rblcheck_backend(char *domain, char *txtbuf, int txtbufsize);
