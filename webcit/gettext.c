#define _GNU_SOURCE
#include "webcit.h"
#include "webserver.h"

#ifdef ENABLE_NLS

static const char *AvailLang[] = {
	"de_DE"
};

/* TODO: we skip the language weightening so far. */
/* Accept-Language: 'de-de,en-us;q=0.7,en;q=0.3' */
void httplang_to_locale(const char *LocaleString)
{
	char *locale = "C";
	char *wanted_locales[10];
	int i = 0;
	int j = 0;
	size_t len = strlen(LocaleString);
	int nFound = 0;
	int nAvail = 1;
	char *search = (char *) malloc(len);
	int done = 0;
	//      char *mo;
	//      char *webcitdir = WEBCITDIR;
	locale_t my_Locale;
	locale_t my_Empty_Locale;
	memcpy(search, LocaleString, len);
	search[len + 1] = '\0';
	len = strlen(search);
	/* the web browser sends '-', we need '_' */
	for (i = 0; i < len; i++)
		if (search[i] == '-')
			search[i] = '_';
	i = 0;
	while ((search[i] != '\0') && !done && (nFound < 10)) {
		if ((search[i] == ',') || (search[i] == ';'))
		{
			if (search[i] == ';')
				done = 1;
			search[i] = '\0';
			wanted_locales[nFound] = (char *) &search[j];
			j = i + 1;
			nFound++;
		}

		i++;
	}
	/* todo: weight  */

	for (i = 0; i <= nFound; i++) {
		for (j = 0; j < nAvail; j++) {
			int ret = strncasecmp(wanted_locales[i],
					      AvailLang[j],
					      strlen(wanted_locales[i]));
			if (!ret) {
				locale = (char *) AvailLang[j];	//wanted_locales[i];
				i = nFound + 1;
				j = nAvail + 1;
				continue;
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
