#ifndef MD5_H
#define MD5_H

struct MD5Context {
	u_int32_t buf[4];
	u_int32_t bits[2];
	unsigned char in[64];
};

void MD5Init(struct MD5Context *context);
void MD5Update(struct MD5Context *context, unsigned char const *buf,
	       unsigned len);
void MD5Final(unsigned char digest[16], struct MD5Context *context);
void MD5Transform(u_int32_t buf[4], u_int32_t const in[16]);
char *make_apop_string(char *realpass, char *nonce, u_char *buffer);

/*
 * This is needed to make RSAREF happy on some MS-DOS compilers.
 */
typedef struct MD5Context MD5_CTX;

#define MD5_DIGEST_LEN		16
#define MD5_HEXSTRING_SIZE	33

#endif /* !MD5_H */
