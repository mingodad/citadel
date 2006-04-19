/*
 * $Id
 */
/**
 * \defgroup LocaleHeaderParser Parse the browser http locale headers and set the NLS stuff.
 * \ingroup WebcitHttpServer 
 */
/*@{*/
#include "webcit.h"
#include "webserver.h"

#ifdef ENABLE_NLS

#define NUM_LANGS 6 /**< how many different locales do we know? */
#define SEARCH_LANG 20 /**< how many langs should we parse? */

/** actual supported locales */
char *AvailLang[NUM_LANGS] = {
	"C",
	"en_US",
	"de_DE",
	"it_IT",
	"es_ES",
	"en_GB"
};

locale_t wc_locales[NUM_LANGS]; /**< here we keep the parsed stuff */

/** Keep information about one locale */
typedef struct _lang_pref{
	char lang[16];          /**< the language locale string */
	char region[16];        /**< the region locale string */
	long priority;          /**< which priority does it have */
	int availability;       /**< do we know it? */
	int selectedlang;       /**< is this the selected language? */
} LangStruct;

/* \brief parse browser locale header 
 * seems as most browsers just do a one after coma value even if more than 10 locales are available. Sample strings:
 * opera: 
 * Accept-Language: sq;q=1.0,de;q=0.9,as;q=0.8,ar;q=0.7,bn;q=0.6,zh-cn;q=0.5,kn;q=0.4,ch;q=0.3,fo;q=0.2,gn;q=0.1,ce;q=0.1,ie;q=0.1 
 * Firefox 
 * Accept-Language: 'de-de,en-us;q=0.7,en;q=0.3' 
 * Accept-Language: de,en-ph;q=0.8,en-us;q=0.5,de-at;q=0.3 
 * Accept-Language: de,en-us;q=0.9,it;q=0.9,de-de;q=0.8,en-ph;q=0.7,de-at;q=0.7,zh-cn;q=0.6,cy;q=0.5,ar-om;q=0.5,en-tt;q=0.4,xh;q=0.3,nl-be;q=0.3,cs;q=0.2,sv;q=0.1,tk;q=0.1 
 * \param LocaleString the string from the browser http headers
 */

void httplang_to_locale(char *LocaleString)
{
	LangStruct wanted_locales[SEARCH_LANG];
	LangStruct *ls;

	int i = 0;
	int j = 0;
	size_t len = strlen(LocaleString);
	long prio;
	int av;
	int nBest;
	int nParts;
	char *search = (char *) malloc(len);
	
	memcpy(search, LocaleString, len);
	search[len] = '\0';
	nParts=num_tokens(search,',');
	for (i=0; ((i<nParts)&&(i<SEARCH_LANG)); i++)
        {
			char buf[16];
			char sbuf[16];
			char lbuf[16];
			int blen;
			
			ls=&wanted_locales[i];

			extract_token(&buf[0],search, i,',',16);
			/** we are searching, if this list item has something like ;q=n*/
			if (num_tokens(&buf[0],'=')>1) {
				int sbuflen, k;
				extract_token(&sbuf[0],&buf[0], 1,'=',16);
				sbuflen=strlen(&sbuf[0]);
				for (k=0; k<sbuflen; k++) if (sbuf[k]=='.') sbuf[k]='0';
				ls->priority=atol(&sbuf[0]);
			}
			else {
				ls->priority=1000;
			}
			/** get the locale part */
			extract_token(&sbuf[0],&buf[0],0,';',16);
			/** get the lang part, which should be allways there */
			extract_token(&ls->lang[0],&sbuf[0],0,'-',16);
			/** get the area code if any. */
			if (num_tokens(&sbuf[0],'-')>1) {
				extract_token(&ls->region[0],&sbuf[0],1,'-',16);
			}
			else { /** no ara code? use lang code */
				blen=strlen(&ls->lang[0]);
				memcpy(&ls->region[0], ls->lang,blen);
				ls->region[blen]='\0';
			} /** area codes are uppercase */
			blen=strlen(&ls->region[0]);
			for (j=0; j<blen; j++)
				{
					int chars=toupper(ls->region[j]);
					ls->region[j]=(char)chars;/** \todo ?! */
				}
			sprintf(&lbuf[0],"%s_%s",&ls->lang[0],&ls->region[0]);
			
			/** check if we have this lang */
			ls->availability=1;
			ls->selectedlang=-1;
			for (j=0; j<NUM_LANGS; j++) {
				int result;
				/** match against the LANG part */
				result=strcasecmp(&ls->lang[0], AvailLang[j]);
				if ((result<0)&&(result<ls->availability)){
					ls->availability=result;
					ls->selectedlang=j;
				}
				/** match against lang and locale */
				if (0==strcasecmp(&lbuf[0], AvailLang[j])){
					ls->availability=0;
					ls->selectedlang=j;
					j=NUM_LANGS;
				}
			}
        }
	
	prio=0;
	av=-1000;
	nBest=-1;
	for (i=0; ((i<nParts)&&(i<SEARCH_LANG)); i++)
		{
			ls=&wanted_locales[i];
			if ((ls->availability<=0)&& 
				(av<ls->availability)&&
				(prio<ls->priority)&&
				(ls->selectedlang!=-1)){
				nBest=ls->selectedlang;
				av=ls->availability;
				prio=ls->priority;
			}
		}
	if (nBest==-1) /** fall back to C */
		nBest=0;
	WC->selected_language=nBest;
	lprintf(9, "language found: %s\n", AvailLang[WC->selected_language]);
	if (search != NULL) {
		free(search);
	}
}

/* TODO: we skip the language weightening so far. */
/* Accept-Language: 'de-de,en-us;q=0.7,en;q=0.3' */
/* Accept-Language: de,en-ph;q=0.8,en-us;q=0.5,de-at;q=0.3 */
//void httplang_to_locale(char *LocaleString)
//{
//	char selected_locale[16];
//	int i, j;
//	char lang[64];
//	int num_accept = 0;
//
//	lprintf(9, "languageAccept: %s\n", LocaleString);
//
//	strcpy(selected_locale, "C");
//	num_accept = num_tokens(LocaleString, ',');
//
//	for (i=num_accept-1; i>=0; --i) {
//		extract_token(lang, LocaleString, i, ',', sizeof lang);
//
//		/* Strip out the weights; we don't use them.  Also convert
//		 * hyphens to underscores.
//		 */
//		for (j=0; j<strlen(lang); ++j) {
//			if (lang[j] == '-') lang[j] = '_';
//			if (lang[j] == ';') lang[j] = 0;
//		}
//
//		for (j=0; j<NUM_LANGS; ++j) {
//			if (!strncasecmp(lang, AvailLang[j], strlen(lang))) {
//				strcpy(selected_locale, AvailLang[j]);
//			}
//		}
//	}
//
//	lprintf(9, "language found: %s\n", selected_locale);
//	set_selected_language(selected_locale);
//}


/**
 * \brief show the language chooser on the login dialog
 * depending on the browser locale change the sequence of the 
 * language chooser.
 */
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

/**
 * \brief Set the selected language for this session.
 * \param lang the locale to set.
 */
void set_selected_language(char *lang) {
	int i;

	for (i=0; i<NUM_LANGS; ++i) {
		if (!strcasecmp(lang, AvailLang[i])) {
			WC->selected_language = i;
		}
	}
}

/**
 * \brief Activate the selected language for this session.
 */
void go_selected_language(void) {
	if (WC->selected_language < 0) return;
	uselocale(wc_locales[WC->selected_language]);	/** switch locales */
	textdomain(textdomain(NULL));			/** clear the cache */
}

/**
 * \brief Deactivate the selected language for this session.
 */
void stop_selected_language(void) {
	uselocale(LC_GLOBAL_LOCALE);			/** switch locales */
	textdomain(textdomain(NULL));			/** clear the cache */
}


/**
 * \brief Create a locale_t for each available language
 */
void initialize_locales(void) {
	int i;
	locale_t Empty_Locale;
	char buf[32];

	/* create default locale */
	Empty_Locale = newlocale(LC_ALL_MASK, NULL, NULL);

	for (i = 0; i < NUM_LANGS; ++i) {
		if (i == 0) {
			sprintf(buf, "%s", AvailLang[i]);	// locale 0 (C) is ascii, not utf-8
		}
		else {
			sprintf(buf, "%s.UTF8", AvailLang[i]);
		}
		wc_locales[i] = newlocale(
			(LC_MESSAGES_MASK|LC_TIME_MASK),
			buf,
			(((i > 0) && (wc_locales[0] != NULL)) ? wc_locales[0] : Empty_Locale)
		);
		if (wc_locales[i] == NULL) {
			lprintf(1, "Error configuring locale for %s: %s\n",
				buf,
				strerror(errno)
			);
		}
		else {
			lprintf(3, "Configured available locale: %s\n", buf);
		}
	}
}


#else	/* ENABLE_NLS */
/** \brief dummy for non NLS enabled systems */
void offer_languages(void) {
	wprintf("English (US)");
}

/** \brief dummy for non NLS enabled systems */
void set_selected_language(char *lang) {
}

/** \brief dummy for non NLS enabled systems */
void go_selected_language(void) {
}

/** \brief dummy for non NLS enabled systems */
void stop_selected_language(void) {
}

#endif	/* ENABLE_NLS */


/*@}*/
