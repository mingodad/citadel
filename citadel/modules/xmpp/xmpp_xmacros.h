
/*
 * define the structures for one token each
 * typename: TheToken_<Tokenname>
 */
#define PAYLOAD(STRUCTNAME, NAME) StrBuf *NAME;int encoding_##NAME;
#define STRPROP(STRUCTNAME, NAME) StrBuf *NAME;
#define TOKEN(NAME, STRUCT) typedef struct __##NAME	\
	STRUCT						\
	TheToken_##NAME;
#include "token.def"
#undef STRPROP
#undef PAYLOAD
#undef TOKEN


/*
 * forward declarations for freeing the members of one struct instance
 # name: free_buf_<Tokenname>
 */

#define TOKEN(NAME, STRUCT)						\
	void free_buf_##NAME(TheToken_##NAME *pdata);
#include "token.def"
#undef STRPROP
#undef PAYLOAD
#undef TOKEN

/*
 * forward declarations, freeing structs and member. 
 * name: free_<Tokenname>
 */
#define TOKEN(NAME, STRUCT)						\
	void free_##NAME(TheToken_##NAME *pdata);

#include "token.def"
#undef STRPROP
#undef PAYLOAD
#undef TOKEN
