/* $Id$ */

/* Shared Diffie-Hellman parameters */
#define DH_P		"1A74527AEE4EE2568E85D4FB2E65E18C9394B9C80C42507D7A6A0DBE9A9A54B05A9A96800C34C7AA5297095B69C88901EEFD127F969DCA26A54C0E0B5C5473EBAEB00957D2633ECAE3835775425DE66C0DE6D024DBB17445E06E6B0C78415E589B8814F08531D02FD43778451E7685541079CFFB79EF0D26EFEEBBB69D1E80383"
#define DH_G		"2"
#define DH_L		1024
#define CIT_CIPHERS	"ALL:RC4+RSA:+SSLv2:@STRENGTH"	/* see ciphers(1) */

int starttls(void);
void endtls(void);
void serv_read(char *buf, int bytes);
void serv_write(char *buf, int nbytes);
#ifdef HAVE_OPENSSL
void serv_read_ssl(char *buf, int bytes);
void serv_write_ssl(char *buf, int nbytes);
void ssl_lock(int mode, int n, const char *file, int line);
#endif /* HAVE_OPENSSL */

extern int ssl_is_connected;

void setCryptoStatusHook(void (*hook)(char *s));
