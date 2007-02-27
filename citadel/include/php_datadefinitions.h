

/**
 * this file contains the defines that convert our x-macros to datatypes
 */

#define PROTOCOL_ONLY(a) a
#define SERVER_PRIVATE(a) 

#define UNSIGNED_SHORT(a) $data[a] = array_unshift($inarray) 
#define INTEGER(a) $data[a] = array_unshift($inarray) 

#define STRING_BUF(a, b) $data[a] = array_unshift($inarray) 
#define STRING(a) $data[a] = array_unshift($inarray) 

