/*
 * $Id$
 *
 * Configuration for LDAP authentication.  Most of this stuff gets pulled out of our site config file.
 *
 * Copyright (c) 1987-2009 by the citadel.org team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

int CtdlTryUserLDAP(char *username, char *found_dn, int found_dn_size, char *fullname, int fullname_size, uid_t *found_uid);
int CtdlTryPasswordLDAP(char *user_dn, const char *password);
int Ctdl_LDAP_to_vCard(char *ldap_dn, struct vCard *v);
