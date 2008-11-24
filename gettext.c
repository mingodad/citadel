/*
 * $Id$
 */

#include "webcit.h"
#include "webserver.h"

#ifdef ENABLE_NLS

#define NUM_LANGS 10		/* how many different locales do we know? */
#define SEARCH_LANG 20		/* how many langs should we parse? */

/* actual supported locales */
const char *AvailLang[NUM_LANGS] = {
	"C",
	"en_US",
	"de_DE",
	"it_IT",
	"es_ES",
	"en_GB",
	"da_DK",
	"fr_FR",
	"nl_NL",
	"pt_BR"
};

const char *AvailLangLoaded[NUM_LANGS];
long nLocalesLoaded = 0;

#ifdef HAVE_USELOCALE
locale_t wc_locales[NUM_LANGS]; /**< here we keep the parsed stuff */
#endif

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

void httplang_to_locale(StrBuf *LocaleString)
{
	LangStruct wanted_locales[SEARCH_LANG];
	LangStruct *ls;

	int i = 0;
	int j = 0;
	/* size_t len = strlen(LocaleString); */
	long prio;
	int av;
	int nBest;
	int nParts;
	StrBuf *Buf = NULL;
	StrBuf *SBuf;
	
	nParts=StrBufNum_tokens(LocaleString,',');
	for (i=0; ((i<nParts)&&(i<SEARCH_LANG)); i++)
        {
		char lbuf[16];
		int blen;
			
		if (Buf == NULL) {
			Buf = NewStrBuf();
			SBuf = NewStrBuf();
		}
		else {
			FlushStrBuf(Buf);
			FlushStrBuf(SBuf);
		}

		ls=&wanted_locales[i];

		StrBufExtract_token(Buf,LocaleString, i,',');
		/** we are searching, if this list item has something like ;q=n*/
		if (StrBufNum_tokens(Buf,'=')>1) {
			int sbuflen, k;
			StrBufExtract_token(SBuf,Buf, 1,'=');
			sbuflen=StrLength(SBuf);
			for (k=0; k<sbuflen; k++) 
				if (ChrPtr(SBuf)[k]=='.') 
					StrBufPeek(SBuf, NULL, k, '0');
			ls->priority=StrTol(SBuf);
		}
		else {
			ls->priority=1000;
		}
		/** get the locale part */
		StrBufExtract_token(SBuf ,Buf, 0, ';');
		/** get the lang part, which should be allways there */
		extract_token(&ls->lang[0], ChrPtr(SBuf), 0, '-', 16);
		/** get the area code if any. */
		if (StrBufNum_tokens(SBuf,'-') > 1) {
			extract_token(&ls->region[0],ChrPtr(SBuf),1,'-',16);
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
		for (j=0; j<nLocalesLoaded; j++) {
			int result;
			/** match against the LANG part */
			result=strcasecmp(&ls->lang[0], AvailLangLoaded[j]);
			if ((result<0)&&(result<ls->availability)){
				ls->availability=result;
				ls->selectedlang=j;
			}
			/** match against lang and locale */
			if (0==strcasecmp(&lbuf[0], AvailLangLoaded[j])){
				ls->availability=0;
				ls->selectedlang=j;
				j=nLocalesLoaded;
			}
		}
        }
	
	prio=0;
	av=-1000;
	nBest=-1;
	for (i=0; ((i<nParts)&&(i<SEARCH_LANG)); i++) {
		ls=&wanted_locales[i];
		if ((ls->availability<=0)&& 
		   (av<ls->availability)&&
		   (prio<ls->priority)&&
		   (ls->selectedlang!=-1)) {
			nBest=ls->selectedlang;
			av=ls->availability;
			prio=ls->priority;
		}
	}
	if (nBest == -1) {
		/** fall back to C */
		nBest=0;
	}
	WC->selected_language=nBest;
	lprintf(9, "language found: %s\n", AvailLangLoaded[WC->selected_language]);
	FreeStrBuf(&Buf);
	FreeStrBuf(&SBuf);
}

/* TODO: we skip the language weighting so far. */
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
void offer_languages(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *Context, int ContextType) {
	int i;
#ifndef HAVE_USELOCALE
	char *Lang = getenv("LANG");
	
	if (Lang == NULL)
		Lang = "C";
#endif

	wprintf("<select name=\"language\" id=\"lname\" size=\"1\">\n");

	for (i=0; i < nLocalesLoaded; ++i) {
#ifndef HAVE_USELOCALE
		if (strcmp(AvailLangLoaded[i], Lang) == 0)
#endif
		wprintf("<option %s value=%s>%s</option>\n",
			((WC->selected_language == i) ? "selected" : ""),
			AvailLangLoaded[i],
			AvailLangLoaded[i]
		);
	}

	wprintf("</select>\n");
}

/**
 * \brief Set the selected language for this session.
 * \param lang the locale to set.
 */
void set_selected_language(const char *lang) {
	int i;

#ifdef HAVE_USELOCALE
	for (i=0; i<nLocalesLoaded; ++i) {
		if (!strcasecmp(lang, AvailLangLoaded[i])) {
			WC->selected_language = i;
		}
	}
#endif
}

/**
 * \brief Activate the selected language for this session.
 */
void go_selected_language(void) {
#ifdef HAVE_USELOCALE
	struct wcsession *WCC = WC;
	if (WCC->selected_language < 0) return;
	uselocale(wc_locales[WCC->selected_language]);	/** switch locales */
	textdomain(textdomain(NULL));			/** clear the cache */
#else
	char *language;
	
	language = getenv("LANG");
	setlocale(LC_MESSAGES, language);
#endif
}

/**
 * \brief Deactivate the selected language for this session.
 */
void stop_selected_language(void) {
#ifdef HAVE_USELOCALE
	uselocale(LC_GLOBAL_LOCALE);			/** switch locales */
	textdomain(textdomain(NULL));			/** clear the cache */
#endif
}

void preset_locale(void)
{
#ifndef HAVE_USELOCALE
#ifdef HAVE_GETTEXT
	char *language;
	
	lprintf(9, "Nailing locale to %s\n", getenv("LANG"));
	language = getenv("LANG");
	setlocale(LC_MESSAGES, language);
#endif
#endif
}

#ifdef HAVE_USELOCALE
	locale_t Empty_Locale;
#endif

/**
 * \brief Create a locale_t for each available language
 */
void initialize_locales(void) {
	int i;
	char buf[32];

#ifdef HAVE_USELOCALE
	/* create default locale */
	Empty_Locale = newlocale(LC_ALL_MASK, NULL, NULL);
#endif

	for (i = 0; i < NUM_LANGS; ++i) {
		if (i == 0) {
			sprintf(buf, "%s", AvailLang[i]);	// locale 0 (C) is ascii, not utf-8
		}
		else {
			sprintf(buf, "%s.UTF8", AvailLang[i]);
		}
#ifdef HAVE_USELOCALE
		wc_locales[nLocalesLoaded] = newlocale(
			(LC_MESSAGES_MASK|LC_TIME_MASK),
			buf,
			(((i > 0) && (wc_locales[0] != NULL)) ? wc_locales[0] : Empty_Locale)
		);
		if (wc_locales[nLocalesLoaded] == NULL) {
			lprintf(1, "Error configuring locale for %s: %s\n",
				buf,
				strerror(errno)
			);
		}
		else {
			lprintf(3, "Configured available locale: %s\n", buf);
			AvailLangLoaded[nLocalesLoaded] = AvailLang[i];
			nLocalesLoaded++;
		}
#endif
	}
}


void ShutdownLocale(void)
{
	int i;
#ifdef HAVE_USELOCALE
	for (i = 0; i < nLocalesLoaded; ++i) {
		if (Empty_Locale != wc_locales[i])
			freelocale(wc_locales[i]);
	}
#endif
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

void preset_locale(void)
{
}
#endif	/* ENABLE_NLS */


void TmplGettext(StrBuf *Target, int nTokens, WCTemplateToken *Tokens)
{
	StrBufAppendBufPlain(Target, _(Tokens->Params[0]->Start), -1, 0);
}


/*
 * Returns the language currently in use.
 * This function returns a static string, so don't do anything stupid please.
 */
const char *get_selected_language(void) {
#ifdef ENABLE_NLS
#ifdef HAVE_USELOCALE
	return AvailLang[WC->selected_language];
#else
	return "en"
#endif
#else
	return "en"
#endif
}


