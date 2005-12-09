#define _GNU_SOURCE
#include "webcit.h"
#include "webserver.h"

#ifdef ENABLE_NLS

#define NUM_LANGS 3
char *AvailLang[NUM_LANGS] = {
	"en_US",
	"de_DE",
	"it_IT"
};

locale_t wc_locales[NUM_LANGS];

typedef struct _lang_pref{
	char lang[16];
	char region[16];
	char *match;
	double Priority;
} LangStruct;

/* TODO: we skip the language weightening so far. */
/* Accept-Language: 'de-de,en-us;q=0.7,en;q=0.3' */
/* Accept-Language: de,en-ph;q=0.8,en-us;q=0.5,de-at;q=0.3 */
void httplang_to_locale(char *LocaleString)
{
	char selected_locale[16];
	int i, j;
	char lang[64];
	int num_accept = 0;

	lprintf(9, "languageAccept: %s\n", LocaleString);

	strcpy(selected_locale, "C");
	num_accept = num_tokens(LocaleString, ',');

	for (i=num_accept-1; i>=0; --i) {
		extract_token(lang, LocaleString, i, ',', sizeof lang);

		/* Strip out the weights; we don't use them.  Also convert
		 * hyphens to underscores.
		 */
		for (j=0; j<strlen(lang); ++j) {
			if (lang[j] == '-') lang[j] = '_';
			if (lang[j] == ';') lang[j] = 0;
		}

		for (j=0; j<NUM_LANGS; ++j) {
			if (!strncasecmp(lang, AvailLang[j], strlen(lang))) {
				strcpy(selected_locale, AvailLang[j]);
			}
		}
	}

	lprintf(9, "language found: %s\n", selected_locale);
	set_selected_language(selected_locale);
}


void offer_languages(void) {
	int i;

	wprintf("<select name=\"language\" size=\"1\">\n");

	for (i=0; i < NUM_LANGS; ++i) {
		wprintf("<option %s value=%s>%s</option>\n",
			((WC->selected_language == i) ? "selected" : ""),
			AvailLang[i],
			AvailLang[i]
		);
	}

	wprintf("</select>\n");
}

/*
 * Set the selected language for this session.
 */
void set_selected_language(char *lang) {
	int i;

	for (i=0; i<NUM_LANGS; ++i) {
		if (!strcasecmp(lang, AvailLang[i])) {
			WC->selected_language = i;
		}
	}
}

/*
 * Activate and deactivate the selected language for this session.
 */
void go_selected_language(void) {
	if (WC->selected_language < 0) return;
	uselocale(wc_locales[WC->selected_language]);	/* switch locales */
	textdomain(textdomain(NULL));			/* clear the cache */
}

void stop_selected_language(void) {
	uselocale(LC_GLOBAL_LOCALE);			/* switch locales */
	textdomain(textdomain(NULL));			/* clear the cache */
}


/*
 * Create a locale_t for each available language
 */
void initialize_locales(void) {
	int i;
	locale_t Empty_Locale;
	char buf[32];

	/* create default locale */
	Empty_Locale = newlocale(LC_ALL_MASK, NULL, NULL);

	for (i = 0; i < NUM_LANGS; ++i) {
		sprintf(buf, "%s.UTF8", AvailLang[i]);
		wc_locales[i] = newlocale(LC_MESSAGES_MASK /* |LC_TIME_MASK FIXME */ ,
			buf,
			Empty_Locale
		);
	}
}


#else	/* ENABLE_NLS */

void offer_languages(void) {
	wprintf("English (US)");
}

void set_selected_language(char *lang) {
}

void go_selected_language(void) {
}

void stop_selected_language(void) {
}

#endif	/* ENABLE_NLS */
