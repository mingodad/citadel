/*
 * Copyright (c) 1996-2013 by the citadel.org team
 *
 * This program is open source software.  You can redistribute it and/or
 * modify it under the terms of the GNU General Public License, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


typedef enum _ePrefType{
	PRF_UNSET = 0,
	PRF_STRING = 1,
	PRF_INT = 2,
	PRF_QP_STRING = 3,
	PRF_YESNO = 4
} ePrefType;


typedef void (*PrefEvalFunc)(StrBuf *Preference, long lvalue); 

void _RegisterPreference(const char *Setting, long SettingLen, 
			 const char *PrefStr, 
			 ePrefType Type, 
			 PrefEvalFunc OnLoad, 
			 const char *OnLoadName);

#define RegisterPreference(a, b, c, d) _RegisterPreference(a, sizeof(a) -1, b, c, d, #d)

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
#define get_room_pref_long(a, b, c) get_ROOM_PREFS_LONG(a, sizeof(a) - 1, b, c)
long get_ROOM_PREFS_LONG(const char *key, size_t keylen, long *value, long Default);


#define get_x_pref(a, b) get_ROOM_PREFS(a, sizeof(a) - 1, b, sizeof(b) - 1)
const StrBuf *get_X_PREFS(const char *key, size_t keylen, 
			  const char *xkey, size_t xkeylen);

#define set_x_pref(a, b, c) set_ROOM_PREFS(a, sizeof(a) - 1, b, sizeof(b) - 1, c, d)
void set_X_PREFS(const char *key, size_t keylen, const char *xkey, size_t xkeylen, StrBuf *value, int save_to_server);

int goto_config_room(StrBuf *Buf, folder *room);
