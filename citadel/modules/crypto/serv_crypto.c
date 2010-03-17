/* $Id$
 *
 * Copyright (c) 1987-2009 by the citadel.org team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

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
#include <libcitadel.h>
#include "server.h"
#include "serv_crypto.h"
#include "sysdep_decls.h"
#include "citadel.h"
#include "config.h"


#include "ctdl_module.h"
/* TODO: should we use the standard module init stuff to start this? */
/* TODO: should we register an event handler to call destruct_ssl? */

#ifdef HAVE_OPENSSL
SSL_CTX *ssl_ctx;		/* SSL context */
pthread_mutex_t **SSLCritters;	/* Things needing locking */

static unsigned long id_callback(void)
{
	return (unsigned long) pthread_self();
}

void destruct_ssl(void)
{
	int a;
	for (a = 0; a < CRYPTO_num_locks(); a++) 
		free(SSLCritters[a]);
	free (SSLCritters);
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
		CtdlLogPrintf(CTDL_CRIT,
			"PRNG not adequately seeded, won't do SSL/TLS\n");
		return;
	}
	SSLCritters =
	    malloc(CRYPTO_num_locks() * sizeof(pthread_mutex_t *));
	if (!SSLCritters) {
		CtdlLogPrintf(CTDL_EMERG, "citserver: can't allocate memory!!\n");
		/* Nothing's been initialized, just die */
		exit(1);
	} else {
		int a;

		for (a = 0; a < CRYPTO_num_locks(); a++) {
			SSLCritters[a] = malloc(sizeof(pthread_mutex_t));
			if (!SSLCritters[a]) {
				CtdlLogPrintf(CTDL_EMERG,
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
		CtdlLogPrintf(CTDL_CRIT, "SSL_CTX_new failed: %s\n",
			ERR_reason_error_string(ERR_get_error()));
		return;
	}
	if (!(SSL_CTX_set_cipher_list(ssl_ctx, CIT_CIPHERS))) {
		CtdlLogPrintf(CTDL_CRIT, "SSL: No ciphers available\n");
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
		CtdlLogPrintf(CTDL_CRIT, "init_ssl() can't allocate a DH object: %s\n",
			ERR_reason_error_string(ERR_get_error()));
		SSL_CTX_free(ssl_ctx);
		ssl_ctx = NULL;
		return;
	}
	if (!(BN_hex2bn(&(dh->p), DH_P))) {
		CtdlLogPrintf(CTDL_CRIT, "init_ssl() can't assign DH_P: %s\n",
			ERR_reason_error_string(ERR_get_error()));
		SSL_CTX_free(ssl_ctx);
		ssl_ctx = NULL;
		return;
	}
	if (!(BN_hex2bn(&(dh->g), DH_G))) {
		CtdlLogPrintf(CTDL_CRIT, "init_ssl() can't assign DH_G: %s\n",
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
	mkdir(ctdl_key_dir, 0700);

	/*
	 * Generate a key pair if we don't have one.
	 */
	if (access(file_crpt_file_key, R_OK) != 0) {
		CtdlLogPrintf(CTDL_INFO, "Generating RSA key pair.\n");
		rsa = RSA_generate_key(1024,	/* modulus size */
					65537,	/* exponent */
					NULL,	/* no callback */
					NULL);	/* no callback */
		if (rsa == NULL) {
			CtdlLogPrintf(CTDL_CRIT, "Key generation failed: %s\n",
				ERR_reason_error_string(ERR_get_error()));
		}
		if (rsa != NULL) {
			fp = fopen(file_crpt_file_key, "w");
			if (fp != NULL) {
				chmod(file_crpt_file_key, 0600);
				if (PEM_write_RSAPrivateKey(fp,	/* the file */
							rsa,	/* the key */
							NULL,	/* no enc */
							NULL,	/* no passphr */
							0,	/* no passphr */
							NULL,	/* no callbk */
							NULL	/* no callbk */
				) != 1) {
					CtdlLogPrintf(CTDL_CRIT, "Cannot write key: %s\n",
                                		ERR_reason_error_string(ERR_get_error()));
					unlink(file_crpt_file_key);
				}
				fclose(fp);
			}
			RSA_free(rsa);
		}
	}

	/*
	 * If there is no certificate file on disk, we will be generating a self-signed certificate
	 * in the next step.  Therefore, if we have neither a CSR nor a certificate, generate
	 * the CSR in this step so that the next step may commence.
	 */
	if ( (access(file_crpt_file_cer, R_OK) != 0) && (access(file_crpt_file_csr, R_OK) != 0) ) {
		CtdlLogPrintf(CTDL_INFO, "Generating a certificate signing request.\n");

		/*
		 * Read our key from the file.  No, we don't just keep this
		 * in memory from the above key-generation function, because
		 * there is the possibility that the key was already on disk
		 * and we didn't just generate it now.
		 */
		fp = fopen(file_crpt_file_key, "r");
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
								   MBSTRING_ASC, 
								   (unsigned char*) config.c_humannode,
								   -1, -1, 0);

					X509_NAME_add_entry_by_txt(name, "OU",
								   MBSTRING_ASC, 
								   (unsigned const char*)"Citadel server",
								   -1, -1, 0);

					/* X509_NAME_add_entry_by_txt(name, "CN",
						MBSTRING_ASC, config.c_fqdn, -1, -1, 0);
					*/

					X509_NAME_add_entry_by_txt(name, 
								   "CN",
								   MBSTRING_ASC, 
								   (const unsigned char *)"*", -1, -1, 0);
				
					X509_REQ_set_subject_name(req, name);

					/* Sign the CSR */
					if (!X509_REQ_sign(req, pk, EVP_md5())) {
						CtdlLogPrintf(CTDL_CRIT, "X509_REQ_sign(): error\n");
					}
					else {
						/* Write it to disk. */	
						fp = fopen(file_crpt_file_csr, "w");
						if (fp != NULL) {
							chmod(file_crpt_file_csr, 0600);
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
			CtdlLogPrintf(CTDL_CRIT, "Unable to read private key.\n");
		}
	}



	/*
	 * Generate a self-signed certificate if we don't have one.
	 */
	if (access(file_crpt_file_cer, R_OK) != 0) {
		CtdlLogPrintf(CTDL_INFO, "Generating a self-signed certificate.\n");

		/* Same deal as before: always read the key from disk because
		 * it may or may not have just been generated.
		 */
		fp = fopen(file_crpt_file_key, "r");
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

			fp = fopen(file_crpt_file_csr, "r");
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
						CtdlLogPrintf(CTDL_CRIT, "X509_sign(): error\n");
					}
					else {
						/* Write it to disk. */	
						fp = fopen(file_crpt_file_cer, "w");
						if (fp != NULL) {
							chmod(file_crpt_file_cer, 0600);
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
        SSL_CTX_use_certificate_chain_file(ssl_ctx, file_crpt_file_cer);
        SSL_CTX_use_PrivateKey_file(ssl_ctx, file_crpt_file_key, SSL_FILETYPE_PEM);
        if ( !SSL_CTX_check_private_key(ssl_ctx) ) {
		CtdlLogPrintf(CTDL_CRIT, "Cannot install certificate: %s\n",
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
void client_write_ssl(const char *buf, int nbytes)
{
	int retval;
	int nremain;
	char junk[1];

	nremain = nbytes;

	while (nremain > 0) {
		if (SSL_want_write(CC->ssl)) {
			if ((SSL_read(CC->ssl, junk, 0)) < 1) {
				CtdlLogPrintf(CTDL_DEBUG, "SSL_read in client_write: %s\n", ERR_reason_error_string(ERR_get_error()));
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
			CtdlLogPrintf(CTDL_DEBUG, "SSL_write got error %ld, ret %d\n", errval, retval);
			if (retval == -1)
				CtdlLogPrintf(CTDL_DEBUG, "errno is %d\n", errno);
			endtls();
			client_write(&buf[nbytes - nremain], nremain);
			return;
		}
		nremain -= retval;
	}
}


/*
 * read data from the encrypted layer.
 */
int client_read_sslbuffer(StrBuf *buf, int timeout)
{
	char sbuf[16384]; /* OpenSSL communicates in 16k blocks, so let's speak its native tongue. */
	int rlen;
	char junk[1];
	SSL *pssl = CC->ssl;

	if (pssl == NULL) return(-1);

	while (1) {
		if (SSL_want_read(pssl)) {
			if ((SSL_write(pssl, junk, 0)) < 1) {
				CtdlLogPrintf(CTDL_DEBUG, "SSL_write in client_read\n");
			}
		}
		rlen = SSL_read(pssl, sbuf, sizeof(sbuf));
		if (rlen < 1) {
			long errval;

			errval = SSL_get_error(pssl, rlen);
			if (errval == SSL_ERROR_WANT_READ || errval == SSL_ERROR_WANT_WRITE) {
				sleep(1);
				continue;
			}
			CtdlLogPrintf(CTDL_DEBUG, "SSL_read got error %ld\n", errval);
			endtls();
			return (-1);
		}
		StrBufAppendBufPlain(buf, sbuf, rlen, 0);
		return rlen;
	}
	return (0);
}

int client_readline_sslbuffer(StrBuf *Line, StrBuf *IOBuf, const char **Pos, int timeout)
{
	CitContext *CCC = CC;
	const char *pos = NULL;
	const char *pLF;
	int len, rlen, retlen;
	int nSuccessLess = 0;
	const char *pch = NULL;
	
	retlen = 0;
	if ((Line == NULL) ||
	    (Pos == NULL) ||
	    (IOBuf == NULL))
	{
		if (Pos != NULL)
			*Pos = NULL;
//		*Error = ErrRBLF_PreConditionFailed;
		return -1;
	}

	pos = *Pos;
	if ((StrLength(IOBuf) > 0) && 
	    (pos != NULL) && 
	    (pos < ChrPtr(IOBuf) + StrLength(IOBuf))) 
	{
		pch = pos;
		pch = strchr(pch, '\n');
		
		if (pch == NULL) {
			StrBufAppendBufPlain(Line, pos, 
					     StrLength(IOBuf) - (pos - ChrPtr(IOBuf)), 0);
			FlushStrBuf(IOBuf);
			pos = *Pos = NULL;
		}
		else {
			int n = 0;
			if ((pch > ChrPtr(IOBuf)) && 
			    (*(pch - 1) == '\r')) {
				n = 1;
			}
			StrBufAppendBufPlain(Line, pos, 
					     (pch - pos - n), 0);

			if (StrLength(IOBuf) <= (pch - ChrPtr(IOBuf) + 1)) {
				FlushStrBuf(IOBuf);
				pos = *Pos = NULL;
			}
			else 
				*Pos = pch + 1;
			return StrLength(Line);
		}
	}

	pLF = NULL;
	while ((nSuccessLess < timeout) && 
	       (pLF == NULL) &&
	       (CCC->ssl != NULL)) {

		rlen = client_read_sslbuffer(IOBuf, timeout);
		if (rlen < 1) {
//			*Error = strerror(errno);
//			close(*fd);
//			*fd = -1;
			return -1;
		}
		else if (rlen > 0) {
			pLF = strchr(ChrPtr(IOBuf), '\n');
		}
	}
	*Pos = NULL;
	if (pLF != NULL) {
		pos = ChrPtr(IOBuf);
		len = pLF - pos;
		if (len > 0 && (*(pLF - 1) == '\r') )
			len --;
		StrBufAppendBufPlain(Line, pos, len, 0);
		if (pLF + 1 >= ChrPtr(IOBuf) + StrLength(IOBuf))
		{
			FlushStrBuf(IOBuf);
		}
		else 
			*Pos = pLF + 1;
		return StrLength(Line);
	}
//	*Error = ErrRBLF_NotEnoughSentFromServer;
	return -1;
}



int client_read_sslblob(StrBuf *Target, long bytes, int timeout)
{
	long bufremain;
	long baselen;
	int retval;
	CitContext *CCC = CC;

	baselen = StrLength(Target);

	if (CCC->Pos == NULL)
		CCC->Pos = ChrPtr(CCC->ReadBuf);
	bufremain = StrLength(CCC->ReadBuf) - 
		(CCC->Pos - ChrPtr(CCC->ReadBuf));

	if (bytes < bufremain)
		bufremain = bytes;
	StrBufAppendBufPlain(Target, CCC->Pos, bufremain, 0);
	StrBufCutLeft(CCC->ReadBuf, bufremain);
	CCC->Pos = NULL;

	if (bytes > bufremain) 
	{
		while ((StrLength(CCC->ReadBuf) + StrLength(Target) < bytes + baselen) &&
		       (retval >= 0))
			retval = client_read_sslbuffer(CCC->ReadBuf, timeout);
		if (retval >= 0) {
			StrBufAppendBuf(Target, CCC->ReadBuf, 0); /* todo: Buf > bytes? */
			return 1;
		}
		else {
			return -1;
		}
	}
	else 
		return 1;
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
		CtdlLogPrintf(CTDL_CRIT, "SSL failed: no ssl_ctx exists?\n");
		if (nosup_response != NULL) cprintf("%s", nosup_response);
		return;
	}
	if (!(CC->ssl = SSL_new(ssl_ctx))) {
		CtdlLogPrintf(CTDL_CRIT, "SSL_new failed: %s\n",
				ERR_reason_error_string(ERR_get_error()));
		if (error_response != NULL) cprintf("%s", error_response);
		return;
	}
	if (!(SSL_set_fd(CC->ssl, CC->client_socket))) {
		CtdlLogPrintf(CTDL_CRIT, "SSL_set_fd failed: %s\n",
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
		CtdlLogPrintf(CTDL_CRIT, "SSL_accept failed: retval=%d, errval=%ld, err=%s\n",
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
	CtdlLogPrintf(CTDL_INFO, "SSL/TLS using %s on %s (%d of %d bits)\n",
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

	CtdlLogPrintf(CTDL_INFO, "Ending SSL/TLS\n");
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
