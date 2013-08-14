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
const StrBuf *SBSTR(const char *key);
const StrBuf *SBstr(const char *key, size_t keylen);

#define xbstr(a, b) (char*) XBstr(a, sizeof(a) - 1, b)
const char *XBstr(const char *key, size_t keylen, size_t *len);
const char *XBSTR(const char *key, size_t *len);

#define lbstr(a) LBstr(a, sizeof(a) - 1)
long LBstr(const char *key, size_t keylen);
long LBSTR(const char *key);

#define ibstr(a) IBstr(a, sizeof(a) - 1)
#define ibcstr(a) IBstr(a.Key, a.len)
int IBstr(const char *key, size_t keylen);
int IBSTR(const char *key);

#define havebstr(a) HaveBstr(a, sizeof(a) - 1)
int HaveBstr(const char *key, size_t keylen);
int HAVEBSTR(const char *key);

#define yesbstr(a) YesBstr(a, sizeof(a) - 1)
int YesBstr(const char *key, size_t keylen);
int YESBSTR(const char *key);

/* TODO: get rid of the non-const-typecast */
#define bstr(a) (char*) Bstr(a, sizeof(a) - 1)
const char *BSTR(const char *key);
const char *Bstr(const char *key, size_t keylen);
/* if you want to ease some parts by just parametring yourself... */
#define putbstr(a, b) PutBstr(a, sizeof(a) - 1, b)
void PutBstr(const char *key, long keylen, StrBuf *Value);

#define putlbstr(a, b) PutlBstr(a, sizeof(a) - 1, b)
void PutlBstr(const char *key, long keylen, long Value);

