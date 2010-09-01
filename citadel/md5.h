
#ifndef MD5_H
#define MD5_H

#include "sysdep.h"
#include "typesize.h"

struct MD5Context {
	cit_uint32_t buf[4];
	cit_uint32_t bits[2];
	cit_uint32_t in[16];
};

void MD5Init(struct MD5Context *context);
void MD5Update(struct MD5Context *context, unsigned char const *buf,
	       unsigned len);
void MD5Final(unsigned char digest[16], struct MD5Context *context);
void MD5Transform(cit_uint32_t buf[4], cit_uint32_t const in[16]);
char *make_apop_string(char *realpass, char *nonce, char *buffer, size_t n);

/*
 * This is needed to make RSAREF happy on some MS-DOS compilers.
 */
#ifndef HAVE_OPENSSL
typedef struct MD5Context MD5_CTX;
#endif

#define MD5_DIGEST_LEN		16
#define MD5_HEXSTRING_SIZE	33

#endif /* !MD5_H */
