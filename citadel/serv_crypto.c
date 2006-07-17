/* $Id$ */

#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
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
#include "serv_extensions.h"
#include "citadel.h"
#include "config.h"


#ifdef HAVE_OPENSSL
SSL_CTX *ssl_ctx;		/* SSL context */
pthread_mutex_t **SSLCritters;	/* Things needing locking */

static unsigned long id_callback(void)
{
	return (unsigned long) pthread_self();
}

void init_ssl(void)
{
	SSL_METHOD *ssl_method;
	DH *dh;
	RSA *rsa=NULL;
	X509_REQ *req = NULL;
	X509 *cer = NULL;
	EVP_PKEY *pk = NULL;
	EVP_PKEY *req_pkey = NULL;
	X509_NAME *name = NULL;
	FILE *fp;

	if (!access(EGD_POOL, F_OK))
		RAND_egd(EGD_POOL);

	if (!RAND_status()) {
		lprintf(CTDL_CRIT,
			"PRNG not adequately seeded, won't do SSL/TLS\n");
		return;
	}
	SSLCritters =
	    malloc(CRYPTO_num_locks() * sizeof(pthread_mutex_t *));
	if (!SSLCritters) {
		lprintf(CTDL_EMERG, "citserver: can't allocate memory!!\n");
		/* Nothing's been initialized, just die */
		exit(1);
	} else {
		int a;

		for (a = 0; a < CRYPTO_num_locks(); a++) {
			SSLCritters[a] = malloc(sizeof(pthread_mutex_t));
			if (!SSLCritters[a]) {
				lprintf(CTDL_EMERG,
					"citserver: can't allocate memory!!\n");
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
		lprintf(CTDL_CRIT, "SSL_CTX_new failed: %s\n",
			ERR_reason_error_string(ERR_get_error()));
		return;
	}
	if (!(SSL_CTX_set_cipher_list(ssl_ctx, CIT_CIPHERS))) {
		lprintf(CTDL_CRIT, "SSL: No ciphers available\n");
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
	CRYPTO_set_id_callback(id_callback);

	/* Load DH parameters into the context */
	dh = DH_new();
	if (!dh) {
		lprintf(CTDL_CRIT, "init_ssl() can't allocate a DH object: %s\n",
			ERR_reason_error_string(ERR_get_error()));
		SSL_CTX_free(ssl_ctx);
		ssl_ctx = NULL;
		return;
	}
	if (!(BN_hex2bn(&(dh->p), DH_P))) {
		lprintf(CTDL_CRIT, "init_ssl() can't assign DH_P: %s\n",
			ERR_reason_error_string(ERR_get_error()));
		SSL_CTX_free(ssl_ctx);
		ssl_ctx = NULL;
		return;
	}
	if (!(BN_hex2bn(&(dh->g), DH_G))) {
		lprintf(CTDL_CRIT, "init_ssl() can't assign DH_G: %s\n",
			ERR_reason_error_string(ERR_get_error()));
		SSL_CTX_free(ssl_ctx);
		ssl_ctx = NULL;
		return;
	}
	dh->length = DH_L;
	SSL_CTX_set_tmp_dh(ssl_ctx, dh);
	DH_free(dh);

	/* Get our certificates in order.
	 * First, create the key/cert directory if it's not there already...
	 */
	mkdir(CTDL_CRYPTO_DIR, 0700);

	/*
	 * Generate a key pair if we don't have one.
	 */
	if (access(CTDL_KEY_PATH, R_OK) != 0) {
		lprintf(CTDL_INFO, "Generating RSA key pair.\n");
		rsa = RSA_generate_key(1024,	/* modulus size */
					65537,	/* exponent */
					NULL,	/* no callback */
					NULL);	/* no callback */
		if (rsa == NULL) {
			lprintf(CTDL_CRIT, "Key generation failed: %s\n",
				ERR_reason_error_string(ERR_get_error()));
		}
		if (rsa != NULL) {
			fp = fopen(CTDL_KEY_PATH, "w");
			if (fp != NULL) {
				chmod(CTDL_KEY_PATH, 0600);
				if (PEM_write_RSAPrivateKey(fp,	/* the file */
							rsa,	/* the key */
							NULL,	/* no enc */
							NULL,	/* no passphr */
							0,	/* no passphr */
							NULL,	/* no callbk */
							NULL	/* no callbk */
				) != 1) {
					lprintf(CTDL_CRIT, "Cannot write key: %s\n",
                                		ERR_reason_error_string(ERR_get_error()));
					unlink(CTDL_KEY_PATH);
				}
				fclose(fp);
			}
			RSA_free(rsa);
		}
	}

	/*
	 * Generate a CSR if we don't have one.
	 */
	if (access(CTDL_CSR_PATH, R_OK) != 0) {
		lprintf(CTDL_INFO, "Generating a certificate signing request.\n");

		/*
		 * Read our key from the file.  No, we don't just keep this
		 * in memory from the above key-generation function, because
		 * there is the possibility that the key was already on disk
		 * and we didn't just generate it now.
		 */
		fp = fopen(CTDL_KEY_PATH, "r");
		if (fp) {
			rsa = PEM_read_RSAPrivateKey(fp, NULL, NULL, NULL);
			fclose(fp);
		}

		if (rsa) {

			/* Create a public key from the private key */
			if (pk=EVP_PKEY_new(), pk != NULL) {
				EVP_PKEY_assign_RSA(pk, rsa);
				if (req = X509_REQ_new(), req != NULL) {

					/* Set the public key */
					X509_REQ_set_pubkey(req, pk);
					X509_REQ_set_version(req, 0L);

					name = X509_REQ_get_subject_name(req);

					/* Tell it who we are */

					/*
					X509_NAME_add_entry_by_txt(name, "C",
						MBSTRING_ASC, "US", -1, -1, 0);

					X509_NAME_add_entry_by_txt(name, "ST",
						MBSTRING_ASC, "New York", -1, -1, 0);

					X509_NAME_add_entry_by_txt(name, "L",
						MBSTRING_ASC, "Mount Kisco", -1, -1, 0);
					*/

					X509_NAME_add_entry_by_txt(name, "O",
						MBSTRING_ASC, config.c_humannode, -1, -1, 0);

					X509_NAME_add_entry_by_txt(name, "OU",
						MBSTRING_ASC, "Citadel server", -1, -1, 0);

					/* X509_NAME_add_entry_by_txt(name, "CN",
						MBSTRING_ASC, config.c_fqdn, -1, -1, 0);
					*/

					X509_NAME_add_entry_by_txt(name, "CN",
						MBSTRING_ASC, "*", -1, -1, 0);
				
					X509_REQ_set_subject_name(req, name);

					/* Sign the CSR */
					if (!X509_REQ_sign(req, pk, EVP_md5())) {
						lprintf(CTDL_CRIT, "X509_REQ_sign(): error\n");
					}
					else {
						/* Write it to disk. */	
						fp = fopen(CTDL_CSR_PATH, "w");
						if (fp != NULL) {
							chmod(CTDL_CSR_PATH, 0600);
							PEM_write_X509_REQ(fp, req);
							fclose(fp);
						}
					}

					X509_REQ_free(req);
				}
			}

			RSA_free(rsa);
		}

		else {
			lprintf(CTDL_CRIT, "Unable to read private key.\n");
		}
	}



	/*
	 * Generate a self-signed certificate if we don't have one.
	 */
	if (access(CTDL_CER_PATH, R_OK) != 0) {
		lprintf(CTDL_INFO, "Generating a self-signed certificate.\n");

		/* Same deal as before: always read the key from disk because
		 * it may or may not have just been generated.
		 */
		fp = fopen(CTDL_KEY_PATH, "r");
		if (fp) {
			rsa = PEM_read_RSAPrivateKey(fp, NULL, NULL, NULL);
			fclose(fp);
		}

		/* This also holds true for the CSR. */
		req = NULL;
		cer = NULL;
		pk = NULL;
		if (rsa) {
			if (pk=EVP_PKEY_new(), pk != NULL) {
				EVP_PKEY_assign_RSA(pk, rsa);
			}

			fp = fopen(CTDL_CSR_PATH, "r");
			if (fp) {
				req = PEM_read_X509_REQ(fp, NULL, NULL, NULL);
				fclose(fp);
			}

			if (req) {
				if (cer = X509_new(), cer != NULL) {

					ASN1_INTEGER_set(X509_get_serialNumber(cer), 0);
					X509_set_issuer_name(cer, req->req_info->subject);
					X509_set_subject_name(cer, req->req_info->subject);
					X509_gmtime_adj(X509_get_notBefore(cer),0);
					X509_gmtime_adj(X509_get_notAfter(cer),(long)60*60*24*SIGN_DAYS);
					req_pkey = X509_REQ_get_pubkey(req);
					X509_set_pubkey(cer, req_pkey);
					EVP_PKEY_free(req_pkey);
					
					/* Sign the cert */
					if (!X509_sign(cer, pk, EVP_md5())) {
						lprintf(CTDL_CRIT, "X509_sign(): error\n");
					}
					else {
						/* Write it to disk. */	
						fp = fopen(CTDL_CER_PATH, "w");
						if (fp != NULL) {
							chmod(CTDL_CER_PATH, 0600);
							PEM_write_X509(fp, cer);
							fclose(fp);
						}
					}
					X509_free(cer);
				}
			}

			RSA_free(rsa);
		}
	}


	/*
	 * Now try to bind to the key and certificate.
	 */
        SSL_CTX_use_certificate_chain_file(ssl_ctx, CTDL_CER_PATH);
        SSL_CTX_use_PrivateKey_file(ssl_ctx, CTDL_KEY_PATH, SSL_FILETYPE_PEM);
        if ( !SSL_CTX_check_private_key(ssl_ctx) ) {
		lprintf(CTDL_CRIT, "Cannot install certificate: %s\n",
				ERR_reason_error_string(ERR_get_error()));
        }

	/* Finally let the server know we're here */
	CtdlRegisterProtoHook(cmd_stls, "STLS", "Start SSL/TLS session");
	CtdlRegisterProtoHook(cmd_gtls, "GTLS",
			      "Get SSL/TLS session status");
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
				lprintf(CTDL_DEBUG, "SSL_read in client_write: %s\n", ERR_reason_error_string(ERR_get_error()));
			}
		}
		retval =
		    SSL_write(CC->ssl, &buf[nbytes - nremain], nremain);
		if (retval < 1) {
			long errval;

			errval = SSL_get_error(CC->ssl, retval);
			if (errval == SSL_ERROR_WANT_READ ||
			    errval == SSL_ERROR_WANT_WRITE) {
				sleep(1);
				continue;
			}
			lprintf(CTDL_DEBUG, "SSL_write got error %ld, ret %d\n", errval, retval);
			if (retval == -1)
				lprintf(CTDL_DEBUG, "errno is %d\n", errno);
			endtls();
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
#if 0
	fd_set rfds;
	struct timeval tv;
	int retval;
	int s;
#endif
	int len, rlen;
	char junk[1];

	len = 0;
	while (len < bytes) {
#if 0
		/*
		 * This code is disabled because we don't need it when
		 * using blocking reads (which we are). -IO
		 */
		FD_ZERO(&rfds);
		s = BIO_get_fd(CC->ssl->rbio, NULL);
		FD_SET(s, &rfds);
		tv.tv_sec = timeout;
		tv.tv_usec = 0;

		retval = select(s + 1, &rfds, NULL, NULL, &tv);

		if (FD_ISSET(s, &rfds) == 0) {
			return (0);
		}

#endif
		if (SSL_want_read(CC->ssl)) {
			if ((SSL_write(CC->ssl, junk, 0)) < 1) {
				lprintf(CTDL_DEBUG, "SSL_write in client_read: %s\n", ERR_reason_error_string(ERR_get_error()));
			}
		}
		rlen = SSL_read(CC->ssl, &buf[len], bytes - len);
		if (rlen < 1) {
			long errval;

			errval = SSL_get_error(CC->ssl, rlen);
			if (errval == SSL_ERROR_WANT_READ ||
			    errval == SSL_ERROR_WANT_WRITE) {
				sleep(1);
				continue;
			}
			lprintf(CTDL_DEBUG, "SSL_read got error %ld\n", errval);
			endtls();
			return (client_read_to
				(&buf[len], bytes - len, timeout));
		}
		len += rlen;
	}
	return (1);
}


/*
 * CtdlStartTLS() starts SSL/TLS encryption for the current session.  It
 * must be supplied with pre-generated strings for responses of "ok," "no
 * support for TLS," and "error" so that we can use this in any protocol.
 */
void CtdlStartTLS(char *ok_response, char *nosup_response,
			char *error_response) {

	int retval, bits, alg_bits;

	if (!ssl_ctx) {
		lprintf(CTDL_CRIT, "SSL failed: no ssl_ctx exists?\n");
		if (nosup_response != NULL) cprintf("%s", nosup_response);
		return;
	}
	if (!(CC->ssl = SSL_new(ssl_ctx))) {
		lprintf(CTDL_CRIT, "SSL_new failed: %s\n",
				ERR_reason_error_string(ERR_get_error()));
		if (error_response != NULL) cprintf("%s", error_response);
		return;
	}
	if (!(SSL_set_fd(CC->ssl, CC->client_socket))) {
		lprintf(CTDL_CRIT, "SSL_set_fd failed: %s\n",
			ERR_reason_error_string(ERR_get_error()));
		SSL_free(CC->ssl);
		CC->ssl = NULL;
		if (error_response != NULL) cprintf("%s", error_response);
		return;
	}
	if (ok_response != NULL) cprintf("%s", ok_response);
	retval = SSL_accept(CC->ssl);
	if (retval < 1) {
		/*
		 * Can't notify the client of an error here; they will
		 * discover the problem at the SSL layer and should
		 * revert to unencrypted communications.
		 */
		long errval;
		char error_string[128];

		errval = SSL_get_error(CC->ssl, retval);
		lprintf(CTDL_CRIT, "SSL_accept failed: retval=%d, errval=%ld, err=%s\n",
			retval,
			errval,
			ERR_error_string(errval, error_string)
		);
		SSL_free(CC->ssl);
		CC->ssl = NULL;
		return;
	}
	BIO_set_close(CC->ssl->rbio, BIO_NOCLOSE);
	bits = SSL_CIPHER_get_bits(SSL_get_current_cipher(CC->ssl), &alg_bits);
	lprintf(CTDL_INFO, "SSL/TLS using %s on %s (%d of %d bits)\n",
		SSL_CIPHER_get_name(SSL_get_current_cipher(CC->ssl)),
		SSL_CIPHER_get_version(SSL_get_current_cipher(CC->ssl)),
		bits, alg_bits);
	CC->redirect_ssl = 1;
}


/*
 * cmd_stls() starts SSL/TLS encryption for the current session
 */
void cmd_stls(char *params)
{
	char ok_response[SIZ];
	char nosup_response[SIZ];
	char error_response[SIZ];

	unbuffer_output();

	sprintf(ok_response,
		"%d Begin TLS negotiation now\n",
		CIT_OK);
	sprintf(nosup_response,
		"%d TLS not supported here\n",
		ERROR + CMD_NOT_SUPPORTED);
	sprintf(error_response,
		"%d TLS negotiation error\n",
		ERROR + INTERNAL_ERROR);

	CtdlStartTLS(ok_response, nosup_response, error_response);
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
	bits =
	    SSL_CIPHER_get_bits(SSL_get_current_cipher(CC->ssl),
				&alg_bits);
	cprintf("%d %s|%s|%d|%d\n", CIT_OK,
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
	if (!CC->ssl) {
		CC->redirect_ssl = 0;
		return;
	}

	lprintf(CTDL_INFO, "Ending SSL/TLS\n");
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
#endif				/* HAVE_OPENSSL */
