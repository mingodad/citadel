/* $Id$ */

#include "sysdep.h"
#ifdef HAVE_OPENSSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#endif
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif
#include <unistd.h>
#include <sys/types.h>
#include "citadel.h"
#include "client_crypto.h"

#ifdef HAVE_OPENSSL
SSL *ssl;
SSL_CTX *ssl_ctx;
int ssl_is_connected = 0;
#ifdef THREADED_CLIENT
pthread_mutex_t **Critters;			/* Things that need locking */
#endif /* THREADED_CLIENT */

extern int serv_sock;
extern int server_is_local;
#endif /* HAVE_OPENSSL */


#ifdef HAVE_OPENSSL
/*
 * input binary data from encrypted connection
 */
void serv_read_ssl(char *buf, int bytes)
{
	int len, rlen;
	char junk[1];

	len = 0;
	while (len < bytes) {
		if (SSL_want_read(ssl)) {
			if ((SSL_write(ssl, junk, 0)) < 1) {
				fprintf(stderr, "SSL_write in serv_read:\n");
				ERR_print_errors_fp(stderr);
			}
		}
		rlen = SSL_read(ssl, &buf[len], bytes - len);
		if (rlen < 1) {
			long errval;

			errval = SSL_get_error(ssl, rlen);
			if (errval == SSL_ERROR_WANT_READ ||
					errval == SSL_ERROR_WANT_WRITE) {
				sleep(1);
				continue;
			}
			if (errval == SSL_ERROR_ZERO_RETURN ||
					errval == SSL_ERROR_SSL) {
				endtls();
				serv_read(&buf[len], bytes - len);
				return;
			}
			fprintf(stderr, "SSL_read in serv_read:\n");
			ERR_print_errors_fp(stderr);
			connection_died();
			return;
		}
		len += rlen;
	}
}


/*
 * send binary to server encrypted
 */
void serv_write_ssl(char *buf, int nbytes)
{
	int bytes_written = 0;
	int retval;
	char junk[1];

	while (bytes_written < nbytes) {
		if (SSL_want_write(ssl)) {
			if ((SSL_read(ssl, junk, 0)) < 1) {
				fprintf(stderr, "SSL_read in serv_write:\n");
				ERR_print_errors_fp(stderr);
			}
		}
		retval = SSL_write(ssl, &buf[bytes_written],
				nbytes - bytes_written);
		if (retval < 1) {
			long errval;

			errval = SSL_get_error(ssl, retval);
			if (errval == SSL_ERROR_WANT_READ ||
					errval == SSL_ERROR_WANT_WRITE) {
				sleep(1);
				continue;
			}
			if (errval == SSL_ERROR_ZERO_RETURN ||
					errval == SSL_ERROR_SSL) {
				endtls();
				serv_write(&buf[bytes_written],
						nbytes - bytes_written);
				return;
			}
			fprintf(stderr, "SSL_write in serv_write:\n");
			ERR_print_errors_fp(stderr);
			connection_died();
			return;
		}
		bytes_written += retval;
	}
}


void ssl_lock(int mode, int n, const char *file, int line)
{
#ifdef THREADED_CLIENT
	if (mode & CRYPTO_LOCK)
		pthread_mutex_lock(Critters[n]);
	else
		pthread_mutex_unlock(Critters[n]);
#endif /* THREADED_CLIENT */
}
#endif /* HAVE_OPENSSL */


/*
 * starttls() starts SSL/TLS if possible
 * Returns 1 if the session is encrypted, 0 otherwise
 */
int starttls(void)
{
#ifdef HAVE_OPENSSL
	int a;
	char buf[SIZ];
	SSL_METHOD *ssl_method;
	DH *dh;
	
	/* TLS is pointless when server is local */
	if (server_is_local) {
		return 0;
	}

	/* Get started */
	ssl = NULL;
	ssl_ctx = NULL;
	dh = NULL;
	SSL_load_error_strings();
	SSLeay_add_ssl_algorithms();

	/* Set up the SSL context in which we will oeprate */
	ssl_method = SSLv23_client_method();
	ssl_ctx = SSL_CTX_new(ssl_method);
	if (!ssl_ctx) {
		fprintf(stderr, "SSL_CTX_new failed: %s\n",
				ERR_reason_error_string(ERR_get_error()));
		return 0;
	}
	/* Any reasonable cipher we can get */
	if (!(SSL_CTX_set_cipher_list(ssl_ctx, CIT_CIPHERS))) {
		fprintf(stderr, "No ciphers available for encryption\n");
		SSL_CTX_free(ssl_ctx);
		ssl_ctx = NULL;
		return 0;
	}
	SSL_CTX_set_session_cache_mode(ssl_ctx, SSL_SESS_CACHE_BOTH);
	
	/* Load DH parameters into the context */
	dh = DH_new();
	if (!dh) {
		fprintf(stderr, "Can't allocate a DH object: %s\n",
				ERR_reason_error_string(ERR_get_error()));
		return 0;
	}
	if (!(BN_hex2bn(&(dh->p), DH_P))) {
		fprintf(stderr, "Can't assign DH_P: %s\n",
				ERR_reason_error_string(ERR_get_error()));
		return 0;
	}
	if (!(BN_hex2bn(&(dh->g), DH_G))) {
		fprintf(stderr, "Can't assign DH_G: %s\n",
				ERR_reason_error_string(ERR_get_error()));
		return 0;
	}
	dh->length = DH_L;
	SSL_CTX_set_tmp_dh(ssl_ctx, dh);
	DH_free(dh);

#ifdef THREADED_CLIENT
	/* OpenSSL requires callbacks for threaded clients */
	CRYPTO_set_locking_callback(ssl_lock);
	CRYPTO_set_id_callback(pthread_self);

	/* OpenSSL requires us to do semaphores for threaded clients */
	Critters = malloc(CRYPTO_num_locks() * sizeof (pthread_mutex_t *));
	if (!Critters) {
		perror("malloc failed");
		exit(1);
	} else {
		for (a = 0; a < CRYPTO_num_locks(); a++) {
			Critters[a] = malloc(sizeof (pthread_mutex_t));
			if (!Critters[a]) {
				perror("malloc failed");
				exit(1);
			}
			pthread_mutex_init(Critters[a], NULL);
		}
	}
#endif /* THREADED_CLIENT */

	/* New SSL object */
	ssl = SSL_new(ssl_ctx);
	if (!ssl) {
		fprintf(stderr, "SSL_new failed: %s\n",
				ERR_reason_error_string(ERR_get_error()));
		SSL_CTX_free(ssl_ctx);
		ssl_ctx = NULL;
		return 0;
	}
	/* Pointless flag waving */
#if SSLEAY_VERSION_NUMBER >= 0x0922
	SSL_set_session_id_context(ssl, "Citadel/UX SID", 14);
#endif
	if (!RAND_status()) {
		fprintf(stderr, "PRNG not properly seeded\n");
		return 0;
	}

	/* Associate network connection with SSL object */
	if (SSL_set_fd(ssl, serv_sock) < 1) {
		fprintf(stderr, "SSL_set_fd failed: %s\n",
				ERR_reason_error_string(ERR_get_error()));
		SSL_CTX_free(ssl_ctx);
		ssl_ctx = NULL;
		SSL_free(ssl);
		ssl = NULL;
		return 0;
	}

	printf("Requesting encryption...\r");
	fflush(stdout);

	/* Ready to start SSL/TLS */
	serv_puts("STLS");
	serv_gets(buf);
	if (buf[0] != '2') {
		fprintf(stderr, "Server can't start TLS: %s\n", &buf[4]);
		return 0;
	}

	/* Do SSL/TLS handshake */
	if ((a = SSL_connect(ssl)) < 1) {
		fprintf(stderr, "SSL_connect failed: %s\n",
				ERR_reason_error_string(ERR_get_error()));
		SSL_CTX_free(ssl_ctx);
		ssl_ctx = NULL;
		SSL_free(ssl);
		ssl = NULL;
		return 0;
	}
	BIO_set_close(ssl->rbio, BIO_NOCLOSE);
	{
		int bits, alg_bits;

		bits = SSL_CIPHER_get_bits(SSL_get_current_cipher(ssl), &alg_bits);
		printf("Encrypting with %s cipher %s (%d of %d bits)\n",
				SSL_CIPHER_get_version(SSL_get_current_cipher(ssl)),
				SSL_CIPHER_get_name(SSL_get_current_cipher(ssl)),
				bits, alg_bits);
	}
	ssl_is_connected = 1;
	return 1;
#else
	return 0;
#endif /* HAVE_OPENSSL */
}


/*
 * void endtls() - end SSL/TLS session
 */
void endtls(void)
{
#ifdef HAVE_OPENSSL
	if (ssl) {
		SSL_shutdown(ssl);
		SSL_free(ssl);
		ssl = NULL;
	}
	ssl_is_connected = 0;
	if (ssl_ctx) {
		SSL_CTX_free(ssl_ctx);
		ssl_ctx = NULL;
	}
#endif
}
