#ifdef ENABLE_NLS
void initialize_locales(void);
void ShutdownLocale(void);

#define NUM_LANGS 10		/* how many different locales do we know? */
#define SEARCH_LANG 20		/* how many langs should we parse? */

/* actual supported locales */
extern const char *AvailLang[NUM_LANGS];
#else 
#define NUM_LANGS 1		/* how many different locales do we know? */
#endif
void TmplGettext(StrBuf *Target, WCTemplputParams *TP);
void offer_languages(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType);
void set_selected_language(const char *);
void go_selected_language(void);
void stop_selected_language(void);
void httplang_to_locale(StrBuf *LocaleString);
