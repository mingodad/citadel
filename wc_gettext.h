#ifdef ENABLE_NLS
void initialize_locales(void);
void ShutdownLocale(void);
#endif
void TmplGettext(StrBuf *Target, int nTokens, WCTemplateToken *Token);
void offer_languages(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType);
void set_selected_language(const char *);
void go_selected_language(void);
void stop_selected_language(void);
void preset_locale(void);
void httplang_to_locale(StrBuf *LocaleString);
