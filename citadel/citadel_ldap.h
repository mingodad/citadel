/*
 * Configuration for LDAP authentication.  Most of this stuff gets pulled out of our site config file.
 */

#define SEARCH_STRING	"(&(objectclass=posixAccount)(uid=%s))"

int CtdlTryUserLDAP(char *username, char *found_dn, int found_dn_size, char *fullname, int fullname_size, uid_t *found_uid);
int CtdlTryPasswordLDAP(char *user_dn, char *password);
