/* $Id$ */

/*
 * Number of days for which self-signed certs are valid.
 */
#define SIGN_DAYS	3650	/* Ten years */

/* Shared Diffie-Hellman parameters */
#define DH_P		"1A74527AEE4EE2568E85D4FB2E65E18C9394B9C80C42507D7A6A0DBE9A9A54B05A9A96800C34C7AA5297095B69C88901EEFD127F969DCA26A54C0E0B5C5473EBAEB00957D2633ECAE3835775425DE66C0DE6D024DBB17445E06E6B0C78415E589B8814F08531D02FD43778451E7685541079CFFB79EF0D26EFEEBBB69D1E80383"
#define DH_G		"2"
#define DH_L		1024
#define CIT_CIPHERS	"ALL:RC4+RSA:+SSLv2:+TLSv1:!MD5:@STRENGTH"	/* see ciphers(1) */

#ifdef HAVE_OPENSSL
void destruct_ssl(void);
void init_ssl(void);
void client_write_ssl (char *buf, int nbytes);
int client_read_ssl (char *buf, int bytes, int timeout);
void cmd_stls(char *params);
void cmd_gtls(char *params);
void endtls(void);
void ssl_lock(int mode, int n, const char *file, int line);
void CtdlStartTLS(char *ok_response, char *nosup_response, char *error_response);
extern SSL_CTX *ssl_ctx;  

#endif
