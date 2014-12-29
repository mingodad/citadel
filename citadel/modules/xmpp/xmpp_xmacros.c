#include <stdio.h>
#include <expat.h>
#include <libcitadel.h>
#include "ctdl_module.h"
#include "serv_xmpp.h"

void *GetToken_iq(void)
{
	return &XMPP->IQ;
}
void *GetToken_piq(void)
{
	return NULL;
}

void *GetToken_message(void)
{
	return &XMPP->Message;
}

/// TODO
#define CALLBACK(NAME);

#define STRPROP(STRUCTNAME, NAME)					\
	if (StrLength(pdata->NAME) > 0)					\
	{								\
	XPut(#NAME, sizeof(#NAME) - 1);					\
	XPut("=\"", 2);							\
	XPutProp(SKEY(pdata->NAME));					\
	XPut("\" ", 2);							\
	}

#define PAYLOAD(STRUCTNAME, NAME)					\
	XPrint(#NAME, sizeof(#NAME) -1,					\
	       XCLOSED,							\
	       TYPE_BODYSTR, SKEY(pdata->NAME),				\
	       TYPE_ARGEND);

#define THENAMESPACE(STRUCTNAME, NAME)					\
	XPut(#NAME, sizeof(#NAME) - 1);					\
	XPut("=\"", 2);							\
	XPutProp(NAMESPACE_##STRUCTNAME,				\
		 sizeof(NAMESPACE_##STRUCTNAME)-1);			\
	XPut("\" ", 2);							

#define TOKEN(NAME, STRUCT)						\
	void serialize_##NAME(TheToken_##NAME *pdata, int Close)	\
	{								\
	XPUT("<");							\
	XPut(#NAME, sizeof(#NAME));					\
	XPUT(" ");							\
	STRUCT ;							\
	XPUT(">");							\
	if (Close)							\
	{								\
		XPut("</", 2);						\
		XPut(#NAME, sizeof(#NAME));				\
		XPut(">", 1);						\
	}								\
	}

#define SUBTOKEN(NAME, NS, STOKEN, STRUCT)				\
	void serialize_##NAME##NS##STOKEN(				\
		theSubToken_##NAME##NS##STOKEN *pdata, int Close)	\
	{								\
	XPUT("<");							\
	XPut(#NAME, sizeof(#NAME));					\
	XPUT(" ");							\
	STRUCT ;							\
	XPUT(">");							\
	if (Close)							\
	{								\
		XPut("</", 2);						\
		XPut(#NAME, sizeof(#NAME));				\
		XPut(">", 1);						\
	}								\
	}

#include "token.def"
#undef STRPROP
#undef PAYLOAD
#undef TOKEN
#undef SUBTOKEN

#define STRPROP(STRUCTNAME, NAME)					\
	FreeStrBuf(&pdata->NAME);

#define PAYLOAD(STRUCTNAME, NAME)					\
	FreeStrBuf(&pdata->NAME);

#define TOKEN(NAME, STRUCT)						\
	void free_buf_##NAME(TheToken_##NAME *pdata)			\
	{								\
		STRUCT ;						\
	}

#define SUBTOKEN(NAME, NS, STOKEN, STRUCT) void free_buf__##NAME##NS##STOKEN \
	(theSubToken_##NAME##NS##STOKEN *pdata){			\
		STRUCT ;						\
	}

#include "token.def"
#undef STRPROP
#undef PAYLOAD
#undef TOKEN
#undef SUBTOKEN

#define TOKEN(NAME, STRUCT)						\
	void free_##NAME(TheToken_##NAME *pdata)			\
	{								\
		free_buf_##NAME(pdata);					\
		free(pdata);						\
	}

#define SUBTOKEN(NAME, NS, STOKEN, STRUCT) void free__##NAME##NS##STOKEN \
	(theSubToken_##NAME##NS##STOKEN *pdata)				\
	{								\
		free_buf__##NAME##NS##STOKEN(pdata);			\
		free(pdata);						\
	}

#include "token.def"
#undef STRPROP
#undef TOKEN
#undef SUBTOKEN



CTDL_MODULE_INIT(xmpp_xmacros)
{
	if (!threading) {
#define STRPROP(TOKENNAME, PROPERTYNAME)				\
		long offset##PROPERTYNAME =				\
			offsetof(TheToken_##TOKENNAME, PROPERTYNAME);	\
		XMPP_RegisterTokenProperty(				\
			NAMESPACE_##TOKENNAME,				\
			sizeof(NAMESPACE_##TOKENNAME)-1,		\
			#TOKENNAME, sizeof(#TOKENNAME)-1,		\
			#PROPERTYNAME, sizeof(#PROPERTYNAME)-1,		\
			GetToken_##TOKENNAME,				\
			offset##PROPERTYNAME);
#define PAYLOAD(TOKENNAME, PROPERTYNAME)				\
		long offset##PROPERTYNAME =				\
			offsetof(TheToken_##TOKENNAME, PROPERTYNAME);	\
		XMPP_RegisterTokenProperty(				\
			NAMESPACE_##TOKENNAME,				\
			sizeof(NAMESPACE_##TOKENNAME)-1,		\
			#TOKENNAME, sizeof(#TOKENNAME)-1,		\
			NULL, 0,					\
			GetToken_##TOKENNAME,				\
			offset##PROPERTYNAME);
#define TOKEN(NAME, STRUCT) STRUCT
#define SUBTOKEN(NAME, NS, STOKEN, STRUCT) STRUCT
#include "token.def"
#undef STRPROP
#undef TOKEN
	}

	/* return our module name for the log */
	return "xmpp_xmacros";
}
