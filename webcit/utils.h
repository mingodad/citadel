void StrEscPuts(const StrBuf *strbuf);
void StrEscputs1(const StrBuf *strbuf, int nbsp, int nolinebreaks);

void urlescputs(const char *);
void hurlescputs(const char *);
long stresc(char *target, long tSize, char *strbuf, int nbsp, int nolinebreaks);
void escputs(const char *strbuf);
