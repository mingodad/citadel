/*
 * $Id$
 *
 * vCard implementation for Citadel
 *
 * Copyright (C) 1999 by Art Cancro
 * This code is freely redistributable under the terms of the GNU General
 * Public License.  All other rights reserved.
 */


#define CTDL_VCARD_MAGIC	0xa1f9

/*
 * This data structure represents a vCard object currently in memory.
 */
struct vCard {
	int magic;
	int numprops;
	struct vCardProp {
		char *name;
		char *value;
	} *prop;
};


struct vCard *vcard_new(void);
struct vCard *vcard_load(char *);
void vcard_free(struct vCard *);
void vcard_set_prop(struct vCard *v, char *name, char *value, int append);
char *vcard_get_prop(struct vCard *v, char *propname, int is_partial,
			int instance, int return_propname);
char *vcard_serialize(struct vCard *);
