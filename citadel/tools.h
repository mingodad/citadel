/* $Id$ */
char *safestrncpy(char *dest, const char *src, size_t n);
int num_parms (char *source);
void extract (char *dest, char *source, int parmnum);
int extract_int (char *source, int parmnum);
long int extract_long (char *source, long int parmnum);
