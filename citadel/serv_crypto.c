/* $Id$ */

#include <unistd.h>
#include <sys/types.h>
#include "sysdep.h"

#ifdef HAVE_OPENSSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#endif

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include <stdio.h>
#include "server.h"
#include "serv_crypto.h"
#include "sysdep_decls.h"
#include "dynloader.h"


#ifdef HAVE_OPENSSL
SSL_CTX *ssl_ctx;				/* SSL context */
pthread_mutex_t **SSLCritters;			/* Things needing locking */


void init_ssl(void)
{
	SSL_METHOD *ssl_method;
	DH *dh;
	
	if (!RAND_status()) {
		lprintf(2, "PRNG not adequately seeded, won't do SSL/TLS\n");
		return;
	}
	SSLCritters = mallok(CRYPTO_num_locks() * sizeof (pthread_mutex_t *));
	if (!SSLCritters) {
		lprintf(1, "citserver: can't allocate memory!!\n");
		/* Nothing's been initialized, just die */
		exit(1);
	} else {
		int a;

		for (a=0; a<CRYPTO_num_locks(); a++) {
			SSLCritters[a] = mallok(sizeof (pthread_mutex_t));
			if (!SSLCritters[a]) {
				lprintf(1, "citserver: can't allocate memory!!\n");
				/* Nothing's been initialized, just die */
				exit(1);
			}
			pthread_mutex_init(SSLCritters[a], NULL);
		}
	}

	/*
	 * Initialize SSL transport layer
	 */
	SSL_library_init();
	SSL_load_error_strings();
	ssl_method = SSLv23_server_method();
	if (!(ssl_ctx = SSL_CTX_new(ssl_method))) {
		lprintf(2, "SSL_CTX_new failed: %s\n",
				ERR_reason_error_string(ERR_get_error()));
		return;
	}
	if (!(SSL_CTX_set_cipher_list(ssl_ctx, CIT_CIPHERS))) {
		lprintf(2, "SSL: No ciphers available\n");
		SSL_CTX_free(ssl_ctx);
		ssl_ctx = NULL;
		return;
	}
#if 0
#if SSLEAY_VERSION_NUMBER >= 0x00906000L
	SSL_CTX_set_mode(ssl_ctx, SSL_CTX_get_mode(ssl_ctx) |
			SSL_MODE_AUTO_RETRY);
#endif
#endif
	CRYPTO_set_locking_callback(ssl_lock);
	CRYPTO_set_id_callback(pthread_self);

	/* Load DH parameters into the context */
	dh = DH_new();
	if (!dh) {
		lprintf(2, "init_ssl() can't allocate a DH object: %s\n",
				ERR_reason_error_string(ERR_get_error()));
		SSL_CTX_free(ssl_ctx);
		ssl_ctx = NULL;
		return;
	}
	if (!(BN_hex2bn(&(dh->p), DH_P))) {
		lprintf(2, "init_ssl() can't assign DH_P: %s\n",
				ERR_reason_error_string(ERR_get_error()));
		SSL_CTX_free(ssl_ctx);
		ssl_ctx = NULL;
		return;
	}
	if (!(BN_hex2bn(&(dh->g), DH_G))) {
		lprintf(2, "init_ssl() can't assign DH_G: %s\n",
				ERR_reason_error_string(ERR_get_error()));
		SSL_CTX_free(ssl_ctx);
		ssl_ctx = NULL;
		return;
	}
	dh->length = DH_L;
	SSL_CTX_set_tmp_dh(ssl_ctx, dh);
	DH_free(dh);

	/* Finally let the server know we're here */
	CtdlRegisterProtoHook(cmd_stls, "STLS", "Start SSL/TLS session");
	CtdlRegisterProtoHook(cmd_gtls, "GTLS", "Get SSL/TLS session status");
	CtdlRegisterSessionHook(endtls, EVT_STOP);
}


/*
 * client_write_ssl() Send binary data to the client encrypted.
 */
void client_write_ssl(char *buf, int nbytes)
{
	int retval;
	int nremain;
	char junk[1];

	nremain = nbytes;

	while (nremain > 0) {
		if (SSL_want_write(CC->ssl)) {
			if ((SSL_read(CC->ssl, junk, 0)) < 1) {
				lprintf(9, "SSL_read in client_write:\n");
				ERR_print_errors_fp(stderr);
			}
		}
		retval = SSL_write(CC->ssl, &buf[nbytes - nremain], nremain);
		if (retval < 1) {
			long errval;

			errval = SSL_get_error(CC->ssl, retval);
			if (errval == SSL_ERROR_WANT_READ ||
					errval == SSL_ERROR_WANT_WRITE) {
				sleep(1);
				continue;
			}
			lprintf(9, "SSL_write got error %ld\n", errval);
			endtls(1);
			client_write(&buf[nbytes - nremain], nremain);
			return;
		}
		nremain -= retval;
	}
}


/*
 * client_read_ssl() - read data from the encrypted layer.
 */
int client_read_ssl(char *buf, int bytes, int timeout)
{
	int len,rlen;
	fd_set rfds;
	struct timeval tv;
	int retval;
	int s;
	char junk[1];

	len = 0;
	while(len<bytes) {
		FD_ZERO(&rfds);
		s = BIO_get_fd(CC->ssl->rbio, NULL);
		FD_SET(s, &rfds);
		tv.tv_sec = timeout;
		tv.tv_usec = 0;

		retval = select(s+1, &rfds, NULL, NULL, &tv);

		if (FD_ISSET(s, &rfds) == 0) {
			return(0);
		}

		if (SSL_want_read(CC->ssl)) {
			if ((SSL_write(CC->ssl, junk, 0)) < 1) {
				lprintf(9, "SSL_write in client_read:\n");
				ERR_print_errors_fp(stderr);
			}
		}
		rlen = SSL_read(CC->ssl, &buf[len], bytes-len);
		if (rlen<1) {
			long errval;

			errval = SSL_get_error(CC->ssl, rlen);
			if (errval == SSL_ERROR_WANT_READ ||
					errval == SSL_ERROR_WANT_WRITE) {
				sleep(1);
				continue;
			}
			lprintf(9, "SSL_read got error %ld\n", errval);
			endtls(1);
			return (client_read_to(&buf[len], bytes - len, timeout));
		}
		len += rlen;
	}
	return(1);
}


/*
 * cmd_stls() starts SSL/TLS encryption for the current session
 */
void cmd_stls(char *params)
{
	int retval, bits, alg_bits;

	if (!ssl_ctx) {
		cprintf("%d No SSL_CTX available\n", ERROR);
		return;
	}
	if (!(CC->ssl = SSL_new(ssl_ctx))) {
		lprintf(2, "SSL_new failed: %s\n",
				ERR_reason_error_string(ERR_peek_error()));
		cprintf("%d SSL_new: %s\n", ERROR,
				ERR_reason_error_string(ERR_get_error()));
		return;
	}
	if (!(SSL_set_fd(CC->ssl, CC->client_socket))) {
		lprintf(2, "SSL_set_fd failed: %s\n",
				ERR_reason_error_string(ERR_peek_error()));
		SSL_free(CC->ssl);
		CC->ssl = NULL;
		cprintf("%d SSL_set_fd: %s\n", ERROR,
				ERR_reason_error_string(ERR_get_error()));
		return;
	}
	cprintf("%d \n", OK);
	retval = SSL_accept(CC->ssl);
	if (retval < 1) {
		/*
		 * Can't notify the client of an error here; they will
		 * discover the problem at the SSL layer and should
		 * revert to unencrypted communications.
		 */
		long errval;

		errval = SSL_get_error(CC->ssl, retval);
		lprintf(2, "SSL_accept failed: %s\n",
				ERR_reason_error_string(ERR_get_error()));
		SSL_free(CC->ssl);
		CC->ssl = NULL;
		return;
	}
	BIO_set_close(CC->ssl->rbio, BIO_NOCLOSE);
	bits = SSL_CIPHER_get_bits(SSL_get_current_cipher(CC->ssl), &alg_bits);
	lprintf(3, "SSL/TLS using %s on %s (%d of %d bits)\n",
			SSL_CIPHER_get_name(SSL_get_current_cipher(CC->ssl)),
			SSL_CIPHER_get_version(SSL_get_current_cipher(CC->ssl)),
			bits, alg_bits);
	CC->redirect_ssl = 1;
}


/*
 * cmd_gtls() returns status info about the TLS connection
 */
void cmd_gtls(char *params)
{
	int bits, alg_bits;
	
	if (!CC->ssl || !CC->redirect_ssl) {
		cprintf("%d Session is not encrypted.\n", ERROR);
		return;
	}
	bits = SSL_CIPHER_get_bits(SSL_get_current_cipher(CC->ssl), &alg_bits);
	cprintf("%d %s|%s|%d|%d\n", OK,
		SSL_CIPHER_get_version(SSL_get_current_cipher(CC->ssl)),
		SSL_CIPHER_get_name(SSL_get_current_cipher(CC->ssl)),
		alg_bits, bits);
}


/*
 * endtls() shuts down the TLS connection
 *
 * WARNING:  This may make your session vulnerable to a known plaintext
 * attack in the current implmentation.
 */
void endtls(void)
{
	lprintf(7, "Ending SSL/TLS%s\n");

	if (!CC->ssl) {
		CC->redirect_ssl = 0;
		return;
	}
	
	SSL_shutdown(CC->ssl);
	SSL_free(CC->ssl);
	CC->ssl = NULL;
	CC->redirect_ssl = 0;
}


/*
 * ssl_lock() callback for OpenSSL mutex locks
 */
void ssl_lock(int mode, int n, const char *file, int line)
{
	if (mode & CRYPTO_LOCK)
		pthread_mutex_lock(SSLCritters[n]);
	else
		pthread_mutex_unlock(SSLCritters[n]);
}
#endif /* HAVE_OPENSSL */
