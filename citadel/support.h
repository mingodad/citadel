/* $Id$ */
void strproc (char *string);
int getstring (FILE *fp, char *string);
int pattern2 (char *search, char *patn);
void mesg_locate (char *targ, size_t n, const char *searchfor,
		  int numdirs, const char * const *dirs);
