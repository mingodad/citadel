/* $Id$ */
char *safestrncpy(char *dest, const char *src, size_t n);
int num_parms (char *source);
void extract_token(char *dest, char *source, int parmnum, char separator);
int extract_int (char *source, int parmnum);
long int extract_long (char *source, long int parmnum);
void encode_base64(char *dest, char *source);
void decode_base64(char *dest, char *source);


#define extract(dest,source,parmnum)	extract_token(dest,source,parmnum,'|')
