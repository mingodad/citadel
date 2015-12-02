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

/* URL / Mime Post parsing -> paramhandling.c */
void upload_handler(char *name, char *filename, char *partnum, char *disp,
		    void *content, char *cbtype, char *cbcharset,
		    size_t length, char *encoding, char *cbid, void *userdata);

void ParseURLParams(StrBuf *url);


/* These may return NULL if not foud */
#define sbstr(a) SBstr(a, sizeof(a) - 1)
const StrBuf *SBstr(const char *key, size_t keylen);

#define xbstr(a, b) (char*) XBstr(a, sizeof(a) - 1, b)
const char *XBstr(const char *key, size_t keylen, size_t *len);
const char *XBSTR(const char *key, size_t *len);

#define lbstr(a) LBstr(a, sizeof(a) - 1)
long LBstr(const char *key, size_t keylen);

#define ibstr(a) IBstr(a, sizeof(a) - 1)
#define ibcstr(a) IBstr(a.Key, a.len)
int IBstr(const char *key, size_t keylen);
int IBSTR(const char *key);

#define havebstr(a) HaveBstr(a, sizeof(a) - 1)
int HaveBstr(const char *key, size_t keylen);

#define yesbstr(a) YesBstr(a, sizeof(a) - 1)
int YesBstr(const char *key, size_t keylen);
int YESBSTR(const char *key);

HashList* getSubStruct(const char *key, size_t keylen);

/* These may return NULL if not foud */
#define ssubbstr(s, a) SSubBstr(s, a, sizeof(a) - 1)
const StrBuf *SSubBstr(HashList *sub, const char *key, size_t keylen);

#define xsubbstr(s, a, b) (char*) XSubBstr(s, a, sizeof(a) - 1, b)
const char *XSubBstr(HashList *sub, const char *key, size_t keylen, size_t *len);

#define lsubbstr(s, a) LSubBstr(s, a, sizeof(a) - 1)
long LSubBstr(HashList *sub, const char *key, size_t keylen);

#define isubbstr(s, a) ISubBstr(s, a, sizeof(a) - 1)
#define isubbcstr(s, a) ISubBstr(s, a.Key, a.len)
int ISubBstr(HashList *sub, const char *key, size_t keylen);

#define havesubbstr(s, a) HaveSubBstr(s, a, sizeof(a) - 1)
int HaveSubBstr(HashList *sub, const char *key, size_t keylen);

#define yessubbstr(s, a) YesSubBstr(s, a, sizeof(a) - 1)
int YesSubBstr(HashList *sub, const char *key, size_t keylen);




/* TODO: get rid of the non-const-typecast */
#define bstr(a) (char*) Bstr(a, sizeof(a) - 1)
const char *BSTR(const char *key);
const char *Bstr(const char *key, size_t keylen);
/* if you want to ease some parts by just parametring yourself... */
#define putbstr(a, b) PutBstr(a, sizeof(a) - 1, b)
void PutBstr(const char *key, long keylen, StrBuf *Value);

#define putlbstr(a, b) PutlBstr(a, sizeof(a) - 1, b)
void PutlBstr(const char *key, long keylen, long Value);

