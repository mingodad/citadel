/*
 * $Id$
 *
 * vCard implementation for Citadel/UX
 *
 * Copyright (C) 1999 by Art Cancro
 * This code is freely redistributable under the terms of the GNU General
 * Public License.  All other rights reserved.
 */


#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#define CTDL_VCARD_MAGIC	0xa1f9

/*
 * This data structure represents a vCard object currently in memory.
 */
struct vCard {
	int magic;
	int numprops;
	struct {
		char *name;
		char *value;
	} *prop;
};




/* 
 * Constructor (empty vCard)
 */
struct vCard *new_vcard() {
	struct vCard *v;

	v = (struct vCard *) malloc(sizeof(struct vCard));
	if (v == NULL) return v;

	v->magic = CTDL_VCARD_MAGIC;
	v->numprops = 0;
	v->prop = NULL;

	return v;
}



/*
 * Destructor
 */
void free_vcard(struct vCard *v) {
	int i;

	if (v->magic != CTDL_VCARD_MAGIC) return;	/* Self-check */
	
	if (v->numprops) for (i=0; i<(v->numprops); ++i) {
		free(v->prop[i].name);
		free(v->prop[i].value);
	}

	if (v->prop != NULL) free(v->prop);
	
	memset(v, 0, sizeof(struct vCard));
}
