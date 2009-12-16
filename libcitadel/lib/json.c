/*
 * $Id: wildfire.c 6962 2009-01-18 19:33:45Z dothebart $
 */
/**
 * \defgroup Subst Variable substitution type stuff
 * \ingroup CitadelConfig
 */

/*@{*/

#include "sysdep.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "libcitadel.h"


#define JSON_STRING 0
#define JSON_NUM 1
#define JSON_NULL 2
#define JSON_BOOL 3
#define JSON_ARRAY 4
#define JSON_OBJECT 7

struct JsonValue {
	int Type;
	StrBuf *Name;
	StrBuf *Value;
	HashList *SubValues;
};


void DeleteJSONValue(void *vJsonValue)
{
	JsonValue *Val = (JsonValue*) vJsonValue;
	FreeStrBuf(&Val->Name);
	FreeStrBuf(&Val->Value);
	DeleteHash(&Val->SubValues);
	free(Val);
}

JsonValue *NewJsonObject(const char *Key, long keylen)
{
	JsonValue *Ret;

	Ret = (JsonValue*) malloc(sizeof(JsonValue));
	memset(Ret, 0, sizeof(JsonValue));
	Ret->Type = JSON_OBJECT;
	if (Key != NULL)
		Ret->Name = NewStrBufPlain(Key, keylen);
	Ret->SubValues = NewHash(1, NULL);
	return Ret;
}

JsonValue *NewJsonArray(const char *Key, long keylen)
{
	JsonValue *Ret;

	Ret = (JsonValue*) malloc(sizeof(JsonValue));
	memset(Ret, 0, sizeof(JsonValue));
	Ret->Type = JSON_ARRAY;
	if (Key != NULL)
		Ret->Name = NewStrBufPlain(Key, keylen);
	Ret->SubValues = NewHash(1, Flathash);
	return Ret;
}


JsonValue *NewJsonNumber(const char *Key, long keylen, long Number)
{
	JsonValue *Ret;

	Ret = (JsonValue*) malloc(sizeof(JsonValue));
	memset(Ret, 0, sizeof(JsonValue));
	Ret->Type = JSON_NUM;
	if (Key != NULL)
		Ret->Name = NewStrBufPlain(Key, keylen);
	Ret->Value = NewStrBufPlain(NULL, 64);
	StrBufPrintf(Ret->Value, "%ld", Number);
	return Ret;
}



JsonValue *NewJsonBigNumber(const char *Key, long keylen, double Number)
{
	JsonValue *Ret;

	Ret = (JsonValue*) malloc(sizeof(JsonValue));
	memset(Ret, 0, sizeof(JsonValue));
	Ret->Type = JSON_NUM;
	if (Key != NULL)
		Ret->Name = NewStrBufPlain(Key, keylen);
	Ret->Value = NewStrBufPlain(NULL, 128);
	StrBufPrintf(Ret->Value, "%f", Number);
	return Ret;
}

JsonValue *NewJsonString(const char *Key, long keylen, StrBuf *CopyMe)
{
	JsonValue *Ret;

	Ret = (JsonValue*) malloc(sizeof(JsonValue));
	memset(Ret, 0, sizeof(JsonValue));
	Ret->Type = JSON_STRING;
	if (Key != NULL)
		Ret->Name = NewStrBufPlain(Key, keylen);
	Ret->Value = NewStrBufDup(CopyMe);
	return Ret;
}

JsonValue *NewJsonPlainString(const char *Key, long keylen, const char *CopyMe, long len)
{
	JsonValue *Ret;

	Ret = (JsonValue*) malloc(sizeof(JsonValue));
	memset(Ret, 0, sizeof(JsonValue));
	Ret->Type = JSON_STRING;
	if (Key != NULL)
		Ret->Name = NewStrBufPlain(Key, keylen);
	Ret->Value = NewStrBufPlain(CopyMe, len);
	return Ret;
}

JsonValue *NewJsonNull(const char *Key, long keylen)
{
	JsonValue *Ret;

	Ret = (JsonValue*) malloc(sizeof(JsonValue));
	memset(Ret, 0, sizeof(JsonValue));
	Ret->Type = JSON_NULL;
	if (Key != NULL)
		Ret->Name = NewStrBufPlain(Key, keylen);
	Ret->Value = NewStrBufPlain(HKEY("nulll"));
	return Ret;
}

JsonValue *NewJsonBool(const char *Key, long keylen, int value)
{
	JsonValue *Ret;

	Ret = (JsonValue*) malloc(sizeof(JsonValue));
	memset(Ret, 0, sizeof(JsonValue));
	Ret->Type = JSON_BOOL;
	if (Key != NULL)
		Ret->Name = NewStrBufPlain(Key, keylen);
	if (value)
		Ret->Value = NewStrBufPlain(HKEY("true"));
	else
		Ret->Value = NewStrBufPlain(HKEY("false"));
	return Ret;
}

void JsonArrayAppend(JsonValue *Array, JsonValue *Val)
{
	long n;
	if (Array->Type != JSON_ARRAY)
		return; /* todo assert! */

	n = GetCount(Array->SubValues);
	Put(Array->SubValues, (const char*) &n, sizeof(n), Val, DeleteJSONValue);
}

void JsonObjectAppend(JsonValue *Array, JsonValue *Val)
{
	if ((Array->Type != JSON_OBJECT) || (Val->Name == NULL))
		return; /* todo assert! */

	Put(Array->SubValues, SKEY(Val->Name), Val, DeleteJSONValue);
}





void SerializeJson(StrBuf *Target, JsonValue *Val, int FreeVal)
{
	void *vValue, *vPrevious;
	JsonValue *SubVal;
	HashPos *It;
	const char *Key;
	long keylen;


	switch (Val->Type) {
	case JSON_STRING:
		StrBufAppendBufPlain(Target, HKEY("\""), 0);
		StrECMAEscAppend(Target, Val->Value, NULL);
		StrBufAppendBufPlain(Target, HKEY("\""), 0);
		break;
	case JSON_NUM:
		StrBufAppendBuf(Target, Val->Value, 0);
		break;
	case JSON_BOOL:
		StrBufAppendBuf(Target, Val->Value, 0);
		break;
	case JSON_NULL:
		StrBufAppendBuf(Target, Val->Value, 0);
		break;
	case JSON_ARRAY:
		vPrevious = NULL;
		StrBufAppendBufPlain(Target, HKEY("["), 0);
		It = GetNewHashPos(Val->SubValues, 0);
		while (GetNextHashPos(Val->SubValues, 
				      It,
				      &keylen, &Key, 
				      &vValue)){
			if (vPrevious != NULL) 
				StrBufAppendBufPlain(Target, HKEY(","), 0);

			SubVal = (JsonValue*) vValue;
			SerializeJson(Target, SubVal, 0);
			vPrevious = vValue;
		}
		StrBufAppendBufPlain(Target, HKEY("]"), 0);
		DeleteHashPos(&It);
		break;
	case JSON_OBJECT:
		vPrevious = NULL;
		StrBufAppendBufPlain(Target, HKEY("{"), 0);
		It = GetNewHashPos(Val->SubValues, 0);
		while (GetNextHashPos(Val->SubValues, 
				      It,
				      &keylen, &Key, 
				      &vValue)){
			SubVal = (JsonValue*) vValue;

			if (vPrevious != NULL) {
				StrBufAppendBufPlain(Target, HKEY(","), 0);
			}
			StrBufAppendBufPlain(Target, HKEY("\""), 0);
			StrBufAppendBuf(Target, SubVal->Name, 0);
			StrBufAppendBufPlain(Target, HKEY("\":"), 0);

			SerializeJson(Target, SubVal, 0);
			vPrevious = vValue;
		}
		StrBufAppendBufPlain(Target, HKEY("}"), 0);
		DeleteHashPos(&It);
		break;
	}
	if(FreeVal) {
		DeleteJSONValue(Val);
	}
}


