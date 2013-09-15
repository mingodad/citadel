#include <stdio.h>

void strproc (char *string);
int getstring (FILE *fp, char *string);
void mesg_locate (char *targ, size_t n, const char *searchfor,
		  int numdirs, const char * const *dirs);
