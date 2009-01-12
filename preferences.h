

#define PRF_STRING 1
#define PRF_INT 2
#define PRF_QP_STRING 3
#define PRF_YESNO 4



typedef void (*PrefEvalFunc)(StrBuf *Preference, long lvalue); 
void RegisterPreference(const char *Setting, long SettingLen, 
			const char *PrefStr, 
			long Type, 
			PrefEvalFunc OnLoad);


void load_preferences(void);
void save_preferences(void);
#define get_preference(a, b) get_PREFERENCE(a, sizeof(a) - 1, b)
#define get_pref(a, b)       get_PREFERENCE(SKEY(a), b)
int get_PREFERENCE(const char *key, size_t keylen, StrBuf **value);
#define set_preference(a, b, c) set_PREFERENCE(a, sizeof(a) - 1, b, c)
#define set_pref(a, b, c)       set_PREFERENCE(SKEY(a), b, c)
void set_PREFERENCE(const char *key, size_t keylen, StrBuf *value, int save_to_server);

#define get_pref_long(a, b, c) get_PREF_LONG(a, sizeof(a) - 1, b, c)
int get_PREF_LONG(const char *key, size_t keylen, long *value, long Default);
#define set_pref_long(a, b, c) set_PREF_LONG(a, sizeof(a) - 1, b, c)
void set_PREF_LONG(const char *key, size_t keylen, long value, int save_to_server);

#define get_pref_yesno(a, b, c) get_PREF_YESNO(a, sizeof(a) - 1, b, c)
int get_PREF_YESNO(const char *key, size_t keylen, int *value, int Default);
#define set_pref_yesno(a, b, c) set_PREF_YESNO(a, sizeof(a) - 1, b, c)
void set_PREF_YESNO(const char *key, size_t keylen, long value, int save_to_server);

#define get_room_pref(a) get_ROOM_PREFS(a, sizeof(a) - 1)
StrBuf *get_ROOM_PREFS(const char *key, size_t keylen);

#define set_room_pref(a, b, c) set_ROOM_PREFS(a, sizeof(a) - 1, b, c)
void set_ROOM_PREFS(const char *key, size_t keylen, StrBuf *value, int save_to_server);


#define get_x_pref(a, b) get_ROOM_PREFS(a, sizeof(a) - 1, b, sizeof(b) - 1)
const StrBuf *get_X_PREFS(const char *key, size_t keylen, 
			  const char *xkey, size_t xkeylen);

#define set_x_pref(a, b, c) set_ROOM_PREFS(a, sizeof(a) - 1, b, sizeof(b) - 1, c, d)
void set_X_PREFS(const char *key, size_t keylen, const char *xkey, size_t xkeylen, StrBuf *value, int save_to_server);
