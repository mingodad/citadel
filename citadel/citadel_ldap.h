/*
 * 
 */

#define CTDL_LDAP_HOST	"ldaptest.xand.com"
#define CTDL_LDAP_PORT	LDAP_PORT		/* defined as 389 */
#define BASE_DN		"dc=xand,dc=com"
#define BIND_DN		NULL			/* "cn=Manager,dc=xand,dc=com" for authenticated bind */
#define BIND_PW		NULL			/* put pw here for authenticated bind */
#define SEARCH_STRING	"(&(objectclass=posixAccount)(uid=%s))"

int CtdlTryUserLDAP(char *username, char *found_dn, int found_dn_size, char *fullname, int fullname_size);
int CtdlTryPasswordLDAP(char *user_dn, char *password);
