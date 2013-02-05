/*
 * Copyright (c) 1996-2012 by the citadel.org team
 *
 * This program is open source software.  You can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "webcit.h"
#include "webserver.h"
#define SEARCH_LANG 20		/* how many langs should we parse? */

#ifdef ENABLE_NLS
/* actual supported locales */
const char *AvailLang[] = {
	"C",
	"bg_BG",
	"cs_CZ",
	"en_US",
	"da_DK",
	"de_DE",
	"el_GR",
	"en_GB",
	"es_ES",
	"et_EE",
	"fi_FI",
	"fr_FR",
	"hu_HU",
	"it_IT",
	"nl_NL",
	"pt_BR",
	"ru_RU",
	"zh_CN",
 	"he_IL",
	"kk_KK",
	"ro_RO",
	"sl_SL",
	"tr_TR",
	""
};

const char **AvailLangLoaded;
long nLocalesLoaded = 0;

#ifdef HAVE_USELOCALE
locale_t *wc_locales; /* here we keep the parsed stuff */
#endif

/* Keep information about one locale */
typedef struct _lang_pref {
	char lang[16];          /* the language locale string */
	char region[16];        /* the region locale string */
	long priority;          /* which priority does it have */
	int availability;       /* do we know it? */
	int selectedlang;       /* is this the selected language? */
} LangStruct;

/* parse browser locale header 
 *
 * seems as most browsers just do a one after comma value even if more than 10 locales are available. Sample strings:
 * opera: 
 * Accept-Language: sq;q=1.0,de;q=0.9,as;q=0.8,ar;q=0.7,bn;q=0.6,zh-cn;q=0.5,kn;q=0.4,ch;q=0.3,fo;q=0.2,gn;q=0.1,ce;q=0.1,ie;q=0.1 
 * Firefox 
 * Accept-Language: 'de-de,en-us;q=0.7,en;q=0.3' 
 * Accept-Language: de,en-ph;q=0.8,en-us;q=0.5,de-at;q=0.3 
 * Accept-Language: de,en-us;q=0.9,it;q=0.9,de-de;q=0.8,en-ph;q=0.7,de-at;q=0.7,zh-cn;q=0.6,cy;q=0.5,ar-om;q=0.5,en-tt;q=0.4,xh;q=0.3,nl-be;q=0.3,cs;q=0.2,sv;q=0.1,tk;q=0.1 
 */

void httplang_to_locale(StrBuf *LocaleString, wcsession *sess)
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
	StrBuf *SBuf = NULL;

	nParts = StrBufNum_tokens(LocaleString, ',');
	for (i=0; ((i<nParts) && (i < SEARCH_LANG)); i++)
        {
		char lbuf[32];
		int blen;
			
		if (Buf == NULL) {
			Buf = NewStrBuf();
			SBuf = NewStrBuf();
		}
		else {
			FlushStrBuf(Buf);
			FlushStrBuf(SBuf);
		}

		ls = &wanted_locales[i];

		StrBufExtract_token(Buf, LocaleString, i, ',');
		/* we are searching, if this list item has something like ;q=n*/
		if (StrBufNum_tokens(Buf, '=') > 1) {
			int sbuflen, k;
			StrBufExtract_token(SBuf, Buf, 1, '=');
			sbuflen = StrLength(SBuf);
			for (k = 0; k < sbuflen; k++) {
				if (ChrPtr(SBuf)[k] == '.') {
					StrBufPeek(SBuf, NULL, k, '0');
				}
			}
			ls->priority = StrTol(SBuf);
		}
		else {
			ls->priority = 1000;
		}

		/* get the locale part */
		StrBufExtract_token(SBuf, Buf, 0, ';');

		/* get the lang part, which should be allways there */
		extract_token(&ls->lang[0], 
			      ChrPtr(SBuf), 
			      0, '-', 
			      sizeof(ls->lang));

		/* get the area code if any. */
		if (StrBufNum_tokens(SBuf, '-') > 1) {
			extract_token(&ls->region[0], 
				      ChrPtr(SBuf), 
				      1, '-', 
				      sizeof(ls->region)
			);
		}
		else { /* no ara code? use lang code */
			blen=strlen(&ls->lang[0]);
			memcpy(&ls->region[0], ls->lang, blen);
			ls->region[blen] = '\0';
		}

		/* area codes are uppercase */
		blen = strlen(&ls->region[0]);
		for (j = 0; j < blen; j++)
		{
			int chars;
			chars = toupper(ls->region[j]);
			ls->region[j] = (char)chars; /* todo ? */
		}
		snprintf(&lbuf[0], 
			 sizeof(lbuf), 
			 "%s_%s", 
			 &ls->lang[0], 
			 &ls->region[0]);
			
		/* check if we have this lang */
		ls->availability = 1;
		ls->selectedlang = -1;
		for (j = 0; j < nLocalesLoaded; j++) {
			int result;
			/* match against the LANG part */
			result = strcasecmp(&ls->lang[0], AvailLangLoaded[j]);
			if ((result < 0) && (result < ls->availability)){
				ls->availability = result;
				ls->selectedlang = j;
			}
			/* match against lang and locale */
			if (0 == strcasecmp(&lbuf[0], AvailLangLoaded[j])){
				ls->availability = 0;
				ls->selectedlang = j;
				j = nLocalesLoaded;
			}
		}
        }
	
	prio = 0;
	av = -1000;
	nBest = -1;
	for (i = 0; ((i < nParts) && (i<SEARCH_LANG)); i++) {
		ls = &wanted_locales[i];
		if (	(ls->availability <= 0)
			&& (av < ls->availability)
			&& (prio < ls->priority)
			&& (ls->selectedlang != -1)
		) {
			nBest = ls->selectedlang;
			av = ls->availability;
			prio = ls->priority;
		}
	}
	if (nBest == -1) {
		/* fall back to C */
		nBest=0;
	}
	sess->selected_language = nBest;
	syslog(LOG_DEBUG, "language found: %s", AvailLangLoaded[WC->selected_language]);
	FreeStrBuf(&Buf);
	FreeStrBuf(&SBuf);
}


/*
 * show the language chooser on the login dialog
 * depending on the browser locale change the sequence of the 
 * language chooser.
 */
void tmplput_offer_languages(StrBuf *Target, WCTemplputParams *TP)
{
	int i;
#ifndef HAVE_USELOCALE
	char *Lang = getenv("LANG");
	
	if (Lang == NULL)
		Lang = "C";
#endif

	if (nLocalesLoaded == 1) {
		wc_printf("<p>%s</p>", AvailLangLoaded[0]);
		return;
	}

	wc_printf("<select name=\"language\" id=\"lname\" size=\"1\" onChange=\"switch_to_lang($('lname').value);\">\n");

	for (i=0; i < nLocalesLoaded; ++i) {
#ifndef HAVE_USELOCALE
		if (strcmp(AvailLangLoaded[i], Lang) == 0)
#endif
		wc_printf("<option %s value=%s>%s</option>\n",
			((WC->selected_language == i) ? "selected" : ""),
			AvailLangLoaded[i],
			AvailLangLoaded[i]
		);
	}

	wc_printf("</select>\n");
}

/*
 * Set the selected language for this session.
 */
void set_selected_language(const char *lang) {
#ifdef HAVE_USELOCALE
	int i;
	for (i = 0; i<nLocalesLoaded; ++i) {
		if (!strcasecmp(lang, AvailLangLoaded[i])) {
			WC->selected_language = i;
			break;
		}
	}
#endif
}

/*
 * Activate the selected language for this session.
 */
void go_selected_language(void) {
#ifdef HAVE_USELOCALE
	wcsession *WCC = WC;
	if (WCC->selected_language < 0) return;
	uselocale(wc_locales[WCC->selected_language]);	/* switch locales */
	textdomain(textdomain(NULL));			/* clear the cache */
#else
	char *language;
	
	language = getenv("LANG");
	setlocale(LC_MESSAGES, language);
#endif
}

/*
 * Deactivate the selected language for this session.
 */
void stop_selected_language(void) {
#ifdef HAVE_USELOCALE
	uselocale(LC_GLOBAL_LOCALE);			/* switch locales */
	textdomain(textdomain(NULL));			/* clear the cache */
#endif
}

#ifdef HAVE_USELOCALE
	locale_t Empty_Locale;
#endif

/*
 * Create a locale_t for each available language
 */
void initialize_locales(void) {
	int nLocales;
	int i;
	char buf[32];
	char *language = NULL;

	nLocales = 0; 
	while (!IsEmptyStr(AvailLang[nLocales]))
		nLocales++;

	language = getenv("WEBCIT_LANG");
	if ((language) && (!IsEmptyStr(language)) && (strcmp(language, "UNLIMITED") != 0)) {
		syslog(LOG_INFO, "Nailing locale to %s", language);
 	}
	else language = NULL;

	AvailLangLoaded = malloc (sizeof(char*) * nLocales);
	memset(AvailLangLoaded, 0, sizeof(char*) * nLocales);
#ifdef HAVE_USELOCALE
	wc_locales = malloc (sizeof(locale_t) * nLocales);
	memset(wc_locales,0, sizeof(locale_t) * nLocales);
	/* create default locale */
	Empty_Locale = newlocale(LC_ALL_MASK, NULL, NULL);
#endif

	for (i = 0; i < nLocales; ++i) {
		if ((language != NULL) && (strcmp(AvailLang[i], language) != 0))
			continue;
		if (i == 0) {
			sprintf(buf, "%s", AvailLang[i]);	/* locale 0 (C) is ascii, not utf-8 */
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
			syslog(LOG_NOTICE, "locale for %s disabled: %s", buf, strerror(errno));
		}
		else {
			syslog(LOG_INFO, "Found locale: %s", buf);
			AvailLangLoaded[nLocalesLoaded] = AvailLang[i];
			nLocalesLoaded++;
		}
#else
		if ((language != NULL) && (strcmp(language, AvailLang[i]) == 0)) {
			setenv("LANG", buf, 1);
			AvailLangLoaded[nLocalesLoaded] = AvailLang[i];
			setlocale(LC_MESSAGES, AvailLang[i]);
			nLocalesLoaded++;
		}
		else if (nLocalesLoaded == 0) {
			setenv("LANG", buf, 1);
			AvailLangLoaded[nLocalesLoaded] = AvailLang[i];
			nLocalesLoaded++;
		}
#endif
	}
	if ((language != NULL) && (nLocalesLoaded == 0)) {
		syslog(LOG_WARNING, "Your selected locale [%s] isn't available on your system. falling back to C", language);
#ifdef HAVE_USELOCALE
		wc_locales[0] = newlocale(
			(LC_MESSAGES_MASK|LC_TIME_MASK),
			AvailLang[0],
			Empty_Locale
		);
#else
		setlocale(LC_MESSAGES, AvailLang[0]);
		setenv("LANG", AvailLang[0], 1);
#endif
		AvailLangLoaded[0] = AvailLang[0];
		nLocalesLoaded = 1;
	}

#ifdef ENABLE_NLS
	setlocale(LC_ALL, "");
	syslog(LOG_DEBUG, "Text domain: %s", textdomain("webcit"));
	syslog(LOG_DEBUG, "Text domain Charset: %s", bind_textdomain_codeset("webcit", "UTF8"));
	syslog(LOG_DEBUG, "Message catalog directory: %s", bindtextdomain(textdomain(NULL), LOCALEDIR"/locale"));
#endif
}


void 
ServerShutdownModule_GETTEXT
(void)
{
#ifdef HAVE_USELOCALE
	int i;
	for (i = 0; i < nLocalesLoaded; ++i) {
		if (Empty_Locale != wc_locales[i]) {
			freelocale(wc_locales[i]);
		}
	}
	free(wc_locales);
#endif
	free(AvailLangLoaded);
}

#else	/* ENABLE_NLS */
const char *AvailLang[] = {
	"C",
	""
};

/* dummy for non NLS enabled systems */
void 
ServerShutdownModule_GETTEXT
(void)
{
}


void tmplput_offer_languages(StrBuf *Target, WCTemplputParams *TP)
{
	wc_printf("English (US)");
}

/* dummy for non NLS enabled systems */
void set_selected_language(const char *lang) {
}

/* dummy for non NLS enabled systems */
void go_selected_language(void) {
}

/* dummy for non NLS enabled systems */
void stop_selected_language(void) {
}

void initialize_locales(void) {
}

#endif	/* ENABLE_NLS */


void TmplGettext(StrBuf *Target, WCTemplputParams *TP)
{
	StrBufAppendBufPlain(Target, _(TP->Tokens->Params[0]->Start), -1, 0);
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
	return "en";
#endif
#else
	return "en";
#endif
}


void Header_HandleAcceptLanguage(StrBuf *Line, ParsedHttpHdrs *hdr)
{
	hdr->HR.browser_language = Line;
}


void 
InitModule_GETTEXT
(void)
{
	initialize_locales();
	
	RegisterHeaderHandler(HKEY("ACCEPT-LANGUAGE"), 
			      Header_HandleAcceptLanguage);
			      
	RegisterNamespace("LANG:SELECT", 0, 0, 
			  tmplput_offer_languages, NULL, CTX_NONE);
}


void
SessionNewModule_GETTEXT
(wcsession *sess)
{
#ifdef ENABLE_NLS
	if (	(sess != NULL)
		&& (!sess->Hdr->HR.Static)
		&& (sess->Hdr->HR.browser_language != NULL)
	) {
		httplang_to_locale(sess->Hdr->HR.browser_language, sess);
	}
#endif
}

void
SessionAttachModule_GETTEXT
(wcsession *sess)
{
#ifdef ENABLE_NLS
	go_selected_language();					/* set locale */
#endif
}

void 
SessionDestroyModule_GETTEXT
(wcsession *sess)
{
#ifdef ENABLE_NLS
	stop_selected_language();				/* unset locale */
#endif
}
