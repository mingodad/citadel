/*
 * $Id$
 *
 * vCard implementation for Citadel/UX
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


struct vCard *new_vcard(void);
struct vCard *load_vcard(char *);
void free_vcard(struct vCard *);
void set_prop(struct vCard *v, char *name, char *value);
char *serialize_vcard(struct vCard *);
