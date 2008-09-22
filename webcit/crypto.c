/*
 * $Id$
 */
/**
 * \defgroup https  Provides HTTPS, when the OpenSSL library is available.
 * \ingroup WebcitHttpServer 
 */

/*@{*/
#include "sysdep.h"
#ifdef HAVE_OPENSSL

#include "webcit.h"
#include "webserver.h"
/** \todo dirify */
/** where to find the keys */
#define	CTDL_CRYPTO_DIR		ctdl_key_dir
#define CTDL_KEY_PATH		file_crpt_file_key /**< the key */
#define CTDL_CSR_PATH		file_crpt_file_csr /**< the csr file */
#define CTDL_CER_PATH		file_crpt_file_cer /**< the cer file */
#define SIGN_DAYS		365 /**< how long our certificate should live */

SSL_CTX *ssl_ctx;		/**< SSL context */
pthread_mutex_t **SSLCritters;	/**< Things needing locking */

pthread_key_t ThreadSSL;	/**< Per-thread SSL context */

/**
 * \brief what?????
 * \return thread id??? 
 */
static unsigned long id_callback(void)
{
	return (unsigned long) pthread_self();
}

void shutdown_ssl(void)
{
	ERR_free_strings();

	/* Openssl requires these while shutdown. 
	 * Didn't find a way to get out of this clean.
	 * int i, n = CRYPTO_num_locks();
	 * for (i = 0; i < n; i++)
	 * 	free(SSLCritters[i]);
	 * free(SSLCritters);
	*/
}

/**
 * \brief initialize ssl engine
 * load certs and initialize openssl internals
 */
void init_ssl(void)
{
	SSL_METHOD *ssl_method;
	RSA *rsa=NULL;
	X509_REQ *req = NULL;
	X509 *cer = NULL;
	EVP_PKEY *pk = NULL;
	EVP_PKEY *req_pkey = NULL;
	X509_NAME *name = NULL;
	FILE *fp;
	char buf[SIZ];

	if (!access("/var/run/egd-pool", F_OK))
		RAND_egd("/var/run/egd-pool");

	if (!RAND_status()) {
		lprintf(3,
			"PRNG not adequately seeded, won't do SSL/TLS\n");
		return;
	}
	SSLCritters =
	    malloc(CRYPTO_num_locks() * sizeof(pthread_mutex_t *));
	if (!SSLCritters) {
		lprintf(1, "citserver: can't allocate memory!!\n");
		/* Nothing's been initialized, just die */
		ShutDownWebcit();
		exit(WC_EXIT_SSL);
	} else {
		int a;

		for (a = 0; a < CRYPTO_num_locks(); a++) {
			SSLCritters[a] = malloc(sizeof(pthread_mutex_t));
			if (!SSLCritters[a]) {
				lprintf(1,
					"citserver: can't allocate memory!!\n");
				/** Nothing's been initialized, just die */
				ShutDownWebcit();
				exit(WC_EXIT_SSL);
			}
			pthread_mutex_init(SSLCritters[a], NULL);
		}
	}

	/**
	 * Initialize SSL transport layer
	 */
	SSL_library_init();
	SSL_load_error_strings();
	ssl_method = SSLv23_server_method();
	if (!(ssl_ctx = SSL_CTX_new(ssl_method))) {
		lprintf(3, "SSL_CTX_new failed: %s\n",
			ERR_reason_error_string(ERR_get_error()));
		return;
	}

	CRYPTO_set_locking_callback(ssl_lock);
	CRYPTO_set_id_callback(id_callback);

	/**
	 * Get our certificates in order. \todo dirify. this is a setup job.
	 * First, create the key/cert directory if it's not there already...
	 */
	mkdir(CTDL_CRYPTO_DIR, 0700);

	/**
	 * Before attempting to generate keys/certificates, first try
	 * link to them from the Citadel server if it's on the same host.
	 * We ignore any error return because it either meant that there
	 * was nothing in Citadel to link from (in which case we just
	 * generate new files) or the target files already exist (which
	 * is not fatal either). \todo dirify
	 */
	if (!strcasecmp(ctdlhost, "uds")) {
		sprintf(buf, "%s/keys/citadel.key", ctdlport);
		symlink(buf, CTDL_KEY_PATH);
		sprintf(buf, "%s/keys/citadel.csr", ctdlport);
		symlink(buf, CTDL_CSR_PATH);
		sprintf(buf, "%s/keys/citadel.cer", ctdlport);
		symlink(buf, CTDL_CER_PATH);
	}

	/**
	 * If we still don't have a private key, generate one.
	 */
	if (access(CTDL_KEY_PATH, R_OK) != 0) {
		lprintf(5, "Generating RSA key pair.\n");
		rsa = RSA_generate_key(1024,	/**< modulus size */
					65537,	/**< exponent */
					NULL,	/**< no callback */
					NULL);	/**< no callback */
		if (rsa == NULL) {
			lprintf(3, "Key generation failed: %s\n",
				ERR_reason_error_string(ERR_get_error()));
		}
		if (rsa != NULL) {
			fp = fopen(CTDL_KEY_PATH, "w");
			if (fp != NULL) {
				chmod(CTDL_KEY_PATH, 0600);
				if (PEM_write_RSAPrivateKey(fp,	/**< the file */
							rsa,	/**< the key */
							NULL,	/**< no enc */
							NULL,	/**< no passphr */
							0,	/**< no passphr */
							NULL,	/**< no callbk */
							NULL	/**< no callbk */
				) != 1) {
					lprintf(3, "Cannot write key: %s\n",
                                		ERR_reason_error_string(ERR_get_error()));
					unlink(CTDL_KEY_PATH);
				}
				fclose(fp);
			}
			else {
				lprintf(3, "Cannot write key: %s\n", CTDL_KEY_PATH);
				ShutDownWebcit();
				exit(0);
			}
			RSA_free(rsa);
		}
	}

	/*
	 * If there is no certificate file on disk, we will be generating a self-signed certificate
	 * in the next step.  Therefore, if we have neither a CSR nor a certificate, generate
	 * the CSR in this step so that the next step may commence.
	 */
	if ( (access(CTDL_CER_PATH, R_OK) != 0) && (access(CTDL_CSR_PATH, R_OK) != 0) ) {
		lprintf(5, "Generating a certificate signing request.\n");

		/**
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

			/** Create a public key from the private key */
			if (pk=EVP_PKEY_new(), pk != NULL) {
				EVP_PKEY_assign_RSA(pk, rsa);
				if (req = X509_REQ_new(), req != NULL) {

					/** Set the public key */
					X509_REQ_set_pubkey(req, pk);
					X509_REQ_set_version(req, 0L);

					name = X509_REQ_get_subject_name(req);

					/** Tell it who we are */

					/* \todo whats this?
					X509_NAME_add_entry_by_txt(name, "C",
						MBSTRING_ASC, "US", -1, -1, 0);

					X509_NAME_add_entry_by_txt(name, "ST",
						MBSTRING_ASC, "New York", -1, -1, 0);

					X509_NAME_add_entry_by_txt(name, "L",
						MBSTRING_ASC, "Mount Kisco", -1, -1, 0);
					*/

					X509_NAME_add_entry_by_txt(
						name, "O",
						MBSTRING_ASC, 
						(unsigned char*)"Organization name",
						-1, -1, 0);

					X509_NAME_add_entry_by_txt(
						name, "OU",
						MBSTRING_ASC, 
						(unsigned char*)"Citadel server1",
						-1, -1, 0);

					X509_NAME_add_entry_by_txt(
						name, "CN",
						MBSTRING_ASC, 
						(unsigned char*)"*", -1, -1, 0);
				
					X509_REQ_set_subject_name(req, name);

					/** Sign the CSR */
					if (!X509_REQ_sign(req, pk, EVP_md5())) {
						lprintf(3, "X509_REQ_sign(): error\n");
					}
					else {
						/** Write it to disk. */	
						fp = fopen(CTDL_CSR_PATH, "w");
						if (fp != NULL) {
							chmod(CTDL_CSR_PATH, 0600);
							PEM_write_X509_REQ(fp, req);
							fclose(fp);
						}
						else {
							lprintf(3, "Cannot write key: %s\n", CTDL_CSR_PATH);
							ShutDownWebcit();
							exit(0);
						}
					}

					X509_REQ_free(req);
				}
			}

			RSA_free(rsa);
		}

		else {
			lprintf(3, "Unable to read private key.\n");
		}
	}



	/**
	 * Generate a self-signed certificate if we don't have one.
	 */
	if (access(CTDL_CER_PATH, R_OK) != 0) {
		lprintf(5, "Generating a self-signed certificate.\n");

		/** Same deal as before: always read the key from disk because
		 * it may or may not have just been generated.
		 */
		fp = fopen(CTDL_KEY_PATH, "r");
		if (fp) {
			rsa = PEM_read_RSAPrivateKey(fp, NULL, NULL, NULL);
			fclose(fp);
		}

		/** This also holds true for the CSR. */
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
					X509_gmtime_adj(X509_get_notBefore(cer), 0);
					X509_gmtime_adj(X509_get_notAfter(cer),(long)60*60*24*SIGN_DAYS);

					req_pkey = X509_REQ_get_pubkey(req);
					X509_set_pubkey(cer, req_pkey);
					EVP_PKEY_free(req_pkey);
					
					/** Sign the cert */
					if (!X509_sign(cer, pk, EVP_md5())) {
						lprintf(3, "X509_sign(): error\n");
					}
					else {
						/** Write it to disk. */	
						fp = fopen(CTDL_CER_PATH, "w");
						if (fp != NULL) {
							chmod(CTDL_CER_PATH, 0600);
							PEM_write_X509(fp, cer);
							fclose(fp);
						}
						else {
							lprintf(3, "Cannot write key: %s\n", CTDL_CER_PATH);
							ShutDownWebcit();
							exit(0);
						}
					}
					X509_free(cer);
				}
			}

			RSA_free(rsa);
		}
	}

	/**
	 * Now try to bind to the key and certificate.
	 * Note that we use SSL_CTX_use_certificate_chain_file() which allows
	 * the certificate file to contain intermediate certificates.
	 */
	SSL_CTX_use_certificate_chain_file(ssl_ctx, CTDL_CER_PATH);
	SSL_CTX_use_PrivateKey_file(ssl_ctx, CTDL_KEY_PATH, SSL_FILETYPE_PEM);
	if ( !SSL_CTX_check_private_key(ssl_ctx) ) {
		lprintf(3, "Cannot install certificate: %s\n",
				ERR_reason_error_string(ERR_get_error()));
	}
	
}


/**
 * \brief starts SSL/TLS encryption for the current session.
 * \param sock the socket connection
 * \return Zero if the SSL/TLS handshake succeeded, non-zero otherwise.
 */
int starttls(int sock) {
	int retval, bits, alg_bits, r;
	SSL *newssl;

	pthread_setspecific(ThreadSSL, NULL);

	if (!ssl_ctx) {
		return(1);
	}
	if (!(newssl = SSL_new(ssl_ctx))) {
		lprintf(3, "SSL_new failed: %s\n",
				ERR_reason_error_string(ERR_get_error()));
		return(2);
	}
	if (!(SSL_set_fd(newssl, sock))) {
		lprintf(3, "SSL_set_fd failed: %s\n",
			ERR_reason_error_string(ERR_get_error()));
		SSL_free(newssl);
		return(3);
	}
	retval = SSL_accept(newssl);
	if (retval < 1) {
		/**
		 * Can't notify the client of an error here; they will
		 * discover the problem at the SSL layer and should
		 * revert to unencrypted communications.
		 */
		long errval;
		const char *ssl_error_reason = NULL;

		errval = SSL_get_error(newssl, retval);
		ssl_error_reason = ERR_reason_error_string(ERR_get_error());
		if (ssl_error_reason == NULL)
			lprintf(3, "SSL_accept failed: errval=%i, retval=%i\n", errval, retval);
		else
			lprintf(3, "SSL_accept failed: %s\n", ssl_error_reason);
		sleep(1);
		retval = SSL_accept(newssl);
	}
	if (retval < 1) {
		long errval;
		const char *ssl_error_reason = NULL;

		errval = SSL_get_error(newssl, retval);
		ssl_error_reason = ERR_reason_error_string(ERR_get_error());
		if (ssl_error_reason == NULL)
			lprintf(3, "SSL_accept failed: errval=%i, retval=%i\n", errval, retval);
		else
			lprintf(3, "SSL_accept failed: %s\n", ssl_error_reason);
		SSL_free(newssl);
		newssl = NULL;
		return(4);
	} else lprintf(15, "SSL_accept success\n");
	r = BIO_set_close(newssl->rbio, BIO_NOCLOSE);
	bits = SSL_CIPHER_get_bits(SSL_get_current_cipher(newssl), &alg_bits);
	lprintf(15, "SSL/TLS using %s on %s (%d of %d bits)\n",
		SSL_CIPHER_get_name(SSL_get_current_cipher(newssl)),
		SSL_CIPHER_get_version(SSL_get_current_cipher(newssl)),
		bits, alg_bits);

	pthread_setspecific(ThreadSSL, newssl);
	lprintf(15, "SSL started\n");
	return(0);
}



/**
 * \brief shuts down the TLS connection
 *
 * WARNING:  This may make your session vulnerable to a known plaintext
 * attack in the current implmentation.
 */
void endtls(void)
{
	SSL_CTX *ctx = NULL;

	if (THREADSSL == NULL) return;

	lprintf(15, "Ending SSL/TLS\n");
	SSL_shutdown(THREADSSL);
	ctx = SSL_get_SSL_CTX(THREADSSL);

	/** I don't think this is needed, and it crashes the server anyway
	 *
	 * 	if (ctx != NULL) {
	 *		lprintf(9, "Freeing CTX at %x\n", (int)ctx );
	 *		SSL_CTX_free(ctx);
	 *	}
	 */

	SSL_free(THREADSSL);
	pthread_setspecific(ThreadSSL, NULL);
}


/**
 * \brief callback for OpenSSL mutex locks
 * \param mode which mode??????
 * \param n  how many???
 * \param file which filename ???
 * \param line what line????
 */
void ssl_lock(int mode, int n, const char *file, int line)
{
	if (mode & CRYPTO_LOCK)
		pthread_mutex_lock(SSLCritters[n]);
	else
		pthread_mutex_unlock(SSLCritters[n]);
}

/**
 * \brief Send binary data to the client encrypted.
 * \param buf chars to send to the client
 * \param nbytes how many chars
 */
void client_write_ssl(const StrBuf *Buf)
{
	const char *buf;
	int retval;
	int nremain;
	long nbytes;
	char junk[1];

	if (THREADSSL == NULL) return;

	nbytes = nremain = StrLength(Buf);
	buf = ChrPtr(Buf);

	while (nremain > 0) {
		if (SSL_want_write(THREADSSL)) {
			if ((SSL_read(THREADSSL, junk, 0)) < 1) {
				lprintf(9, "SSL_read in client_write: %s\n",
						ERR_reason_error_string(ERR_get_error()));
			}
		}
		retval = SSL_write(THREADSSL, &buf[nbytes - nremain], nremain);
		if (retval < 1) {
			long errval;

			errval = SSL_get_error(THREADSSL, retval);
			if (errval == SSL_ERROR_WANT_READ ||
			    errval == SSL_ERROR_WANT_WRITE) {
				sleep(1);
				continue;
			}
			lprintf(9, "SSL_write got error %ld, ret %d\n", errval, retval);
			if (retval == -1) {
				lprintf(9, "errno is %d\n", errno);
			}
			endtls();
			return;
		}
		nremain -= retval;
	}
}


/**
 * \brief read data from the encrypted layer.
 * \param buf charbuffer to read to 
 * \param bytes how many
 * \param timeout how long should we wait?
 * \returns what???
 */
int client_read_sslbuffer(StrBuf *buf, int timeout)
{
#if 0
	fd_set rfds;
	struct timeval tv;
	int retval;
	int s;
#endif
	char sbuf[16384]; /**< Openssl communicates in 16k blocks, so lets speak its native tongue. */
	int rlen;
	char junk[1];

	if (THREADSSL == NULL) return(0);

	while (1) {
#if 0
		/**
		 * This code is disabled because we don't need it when
		 * using blocking reads (which we are). -IO
		 */
		FD_ZERO(&rfds);
		s = BIO_get_fd(THREADSSL->rbio, NULL);
		FD_SET(s, &rfds);
		tv.tv_sec = timeout;
		tv.tv_usec = 0;

		retval = select(s + 1, &rfds, NULL, NULL, &tv);

		if (FD_ISSET(s, &rfds) == 0) {
			return (0);
		}

#endif
		if (SSL_want_read(THREADSSL)) {
			if ((SSL_write(THREADSSL, junk, 0)) < 1) {
				lprintf(9, "SSL_write in client_read\n");
			}
		}
		rlen = SSL_read(THREADSSL, sbuf, sizeof(sbuf));
		if (rlen < 1) {
			long errval;

			errval = SSL_get_error(THREADSSL, rlen);
			if (errval == SSL_ERROR_WANT_READ ||
			    errval == SSL_ERROR_WANT_WRITE) {
				sleep(1);
				continue;
			}
			lprintf(9, "SSL_read got error %ld\n", errval);
			endtls();
			return (0);
		}
		StrBufAppendBufPlain(buf, sbuf, rlen, 0);
		return(1);
	}
	return (1);
}


#endif				/* HAVE_OPENSSL */
/*@}*/
