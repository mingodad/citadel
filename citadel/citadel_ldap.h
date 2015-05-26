/*
 * Configuration for LDAP authentication.  Most of this stuff gets pulled out of our site config file.
 *
 * Copyright (c) 1987-2012 by the citadel.org team
 *
 *  This program is open source software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

int CtdlTryUserLDAP(char *username, char *found_dn, int found_dn_size, char *fullname, int fullname_size, uid_t *found_uid, int lookup_based_on_uid);
int CtdlTryPasswordLDAP(char *user_dn, const char *password);
int Ctdl_LDAP_to_vCard(char *ldap_dn, struct vCard *v);
