void strproc (char *string);
int num_parms (char *source);
void extract (char *dest, char *source, int parmnum);
int extract_int (char *source, int parmnum);
long int extract_long (char *source, long int parmnum);
int getstring (FILE *fp, char *string);
int pattern2 (char *search, char *patn);
void mesg_locate (char *targ, char *searchfor, int numdirs, char **dirs);
