#define _GNU_SOURCE
#include "webcit.h"
#include "webserver.h"

#ifdef ENABLE_NLS

static const char *AvailLang[] = {
	"en_US",
	"de_DE",
	"it_IT"
};

typedef struct _lang_pref{
	char lang[16];
	char region[16];
	char *match;
	double Priority;
} LangStruct;

/* TODO: we skip the language weightening so far. */
/* Accept-Language: 'de-de,en-us;q=0.7,en;q=0.3' */
/* Accept-Language: de,en-ph;q=0.8,en-us;q=0.5,de-at;q=0.3 */
void httplang_to_locale(const char *LocaleString)
{
	char *locale = "C";
	LangStruct wanted_locales[20];
	int i = 0;
	int j = 0;
	size_t len = strlen(LocaleString);
	int nFound = 0;
	int nParts;
	const int nAvail = 1; /* Number of members in AvailLang */
	char *search = (char *) malloc(len);
	locale_t my_Locale;
	locale_t my_Empty_Locale;

	memcpy(search, LocaleString, len);
	search[len] = '\0';
	nParts=num_tokens(search,',');
	for (i=0; ((i<nParts)&&(i<10)); i++)
	{
		char buf[16];
		char sbuf[16];
		int blen;

		extract_token(&buf[0],search, 0,',',16);
		/* we are searching, if this list item has something like ;q=n*/
		if (num_tokens(&buf[0],'=')>1) {
			extract_token(&sbuf[0],&buf[0], 1,'=',16);
			wanted_locales[i].Priority=atof(&sbuf[0]);
		}
		else {
			wanted_locales[i].Priority=1.0;
		}
		/* get the locale part */
		extract_token(&sbuf[0],&buf[0],0,';',16);
		/* get the lang part, which should be allways there */
		extract_token(&wanted_locales[i].lang[0],&sbuf[0],0,'-',16);
		/* get the area code if any. */
		if (num_tokens(&sbuf[0],'-')>1)	{
			extract_token(&wanted_locales[i].region[0],&sbuf[0],1,'-',16);
		}
		else { /* no ara code? use lang code */
			blen=strlen(&wanted_locales[i].lang[0]);
			memcpy(&wanted_locales[i].region[0], wanted_locales[i].lang,blen);
			wanted_locales[i].region[blen]='\0';
		} /* area codes are uppercase */
		blen=strlen(&wanted_locales[i].region[0]);
		for (j=0; j<blen; j++)
			{
				int chars=toupper(wanted_locales[i].region[j]);
				wanted_locales[i].region[j]=(char)chars;/*todo ?! */
			}
	}

	/* todo: weight  */
	if (nFound > 0) {
		for (i = 0; i <= nFound; i++) {
			for (j = 0; j < nAvail; j++) {
				int ret = strncasecmp(&wanted_locales[i].lang[0],
									  AvailLang[j],
									  strlen(&wanted_locales[i].lang[0]));
				if (!ret) {
					locale = (char *) AvailLang[j];	//wanted_locales[i];
					i = nFound + 1;
					j = nAvail + 1;
					continue;
				}
				
			}
		}
	}

	len = strlen(locale);
	memcpy(search, locale, len);
	memcpy(&search[len], ".UTF8", 5);
	search[len + 5] = '\0';
	my_Empty_Locale = newlocale(LC_ALL_MASK, NULL, NULL);	/* create default locale */
	my_Locale = newlocale(LC_MESSAGES_MASK /*|LC_TIME_MASK FIXME */ ,
			      search, my_Empty_Locale);

	uselocale(my_Locale);
	//      freelocale(my_Locale);
	//      freelocale(my_Empty_Locale);
	free(search);
}

#endif


/*
	// the web browser sends '-', we need '_' 

		for (i = 0; i < len; i++)
		if (search[i] == '-')
			search[i] = '_';

	nFound=i;
	nParts=num_tokens(search,',');
	if (nParts>0)
		{
			extract_token(prefers,search, 1,';',len);
			extract_token(langs,search, 0,';',len);
		}
	else
		{
			free(prefers);
			prefers=NULL;
			memcpy(search, len);
			search[len] = '\0';
		}
	i = 0;
	while ( !done && (nFound < 10)) {
		if ((search[i] == ',') || (search[i] == ';') || (search[i] == '\0'))
		{
			if ((search[i] == ';') || (search[i] == '\0'))
				done = 1;
			search[i] = '\0';
			wanted_locales[nFound] = (char *) &search[j];
			j = i + 1;
			nFound++;
		}

		i++;
	}
*/
