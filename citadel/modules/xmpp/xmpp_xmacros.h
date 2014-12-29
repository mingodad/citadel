
/*
 * define the structures for one token each
 * typename: TheToken_<Tokenname>
 */

#define XMPPARGS char *token, long len
typedef void (*XMPP_callback)(XMPPARGS);
#define CALLBACK(NAME)  XMPP_callback CB_##NAME; 
#define PAYLOAD(STRUCTNAME, NAME) StrBuf *NAME;int encoding_##NAME;
#define STRPROP(STRUCTNAME, NAME) StrBuf *NAME;
#define TOKEN(NAME, STRUCT) typedef struct __##NAME	\
	STRUCT						\
	TheToken_##NAME;
#define SUBTOKEN(NAME, NS, STOKEN, STRUCT) typedef struct __##NAME##NS##STOKEN \
	STRUCT							       \
	theSubToken_##NAME##NS##STOKEN;

#include "token.def"

#undef STRPROP
#undef PAYLOAD
#undef TOKEN
#undef SUBTOKEN


/*
 * forward declarations for freeing the members of one struct instance
 # name: free_buf_<Tokenname>
 */

#define SUBTOKEN(NAME, NS, STOKEN, STRUCT) void free_buf__##NAME##NS##STOKEN \
	(theSubToken_##NAME##NS##STOKEN *pdata);
#define TOKEN(NAME, STRUCT)						\
	void free_buf_##NAME(TheToken_##NAME *pdata);
#include "token.def"
#undef STRPROP
#undef PAYLOAD
#undef TOKEN
#undef SUBTOKEN

/*
 * forward declarations, freeing structs and member. 
 * name: free_<Tokenname>
 */
#define SUBTOKEN(NAME, NS, STOKEN, STRUCT) void free__##NAME##NS##STOKEN \
	(theSubToken_##NAME##NS##STOKEN *pdata);
#define TOKEN(NAME, STRUCT)						\
	void free_##NAME(TheToken_##NAME *pdata);

#include "token.def"
#undef STRPROP
#undef PAYLOAD
#undef TOKEN
#undef SUBTOKEN

#undef CALLBACK
