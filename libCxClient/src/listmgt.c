/**
 ** libCxClient - Citadel/UX Extensible Client API
 ** Copyright (c) 2000, Flaming Sword Productions
 ** Copyright (c) 2001, The Citadel/UX Consortium
 ** All Rights Reserved
 **
 ** Module: listmgt.o
 ** Date: 2000-10-15
 ** Last Revision: 2000-10-15
 ** Description: Brian's Linked-list Manager
 ** CVS: $Id$
 **
 ** Based loosely upon the ideas expressed in Jesse Sweetland's
 ** linked-list code.
 **/
#include	<stdio.h>
#include	<stdlib.h>
#include	<stdarg.h>
#include	<string.h>
#include	<CxClient.h>
#include	"autoconf.h"

/**
 ** CxLiInsert(): Insert a new string into the linked-list.
 **
 ** [Expects]
 **  (CXLIST)li: The list we're inserting into.
 **  (char *)s: The string (or data) we are inserting into @li.
 **
 ** [Returns]
 **  li
 **/
CXLIST		CxLlInsert(CXLIST li, char *s) {
CXLIST		p,new = 0;
int		loop = 0;

	DPF((DFA,"List @0x%08x",li));
	DPF((DFA,"Inserting \"%s\"",s));

	for(loop = 0; loop < 5; loop++ ) {
		DPF((DFA,"malloc safety loop, iteration %d",loop));
		if((new = (CXLIST) CxMalloc( sizeof( CXLIST ) ))) break;
	}

	if(!new) return(li);

	new->data = (char *) CxMalloc( strlen( s ) +1 );
	strcpy(new->data, s);
	new->next = NULL;

	if(li) {
		p = li;
		while( p->next ) p = p->next;
		p->next = new;

	} else {
		li = new;
	}

	return(li);
}

/**
 ** CxLlRemove(): Remove the n'th item from the linked-list. [SKEL]
 **
 ** [Expects]
 **  (CXLIST)li: The list we are altering.
 **  (int)d: The item number to remove.
 **
 ** [Returns]
 **  li
 **/
CXLIST		CxLlRemove(CXLIST li, unsigned int d) {
	return(li);
}

/**
 ** CxLlFlush(): Flush all of a list's memory.  (Erases the entire
 ** list.)
 **
 ** [Expects]
 **  (CXLIST)li: The list to be nuked.
 **
 ** [Returns]
 **  NULL
 **/
CXLIST		CxLlFlush(CXLIST li) {
CXLIST		t,p;

	DPF((DFA,"Clearing list @0x%08x",li));
	p = li;
	while ( p ) {
		t = p;
		CxFree(p->data);
		p = p->next;
		CxFree(t);
	}

	/**
	 ** This function should _ALWAYS_ eliminate the list...
	 ** Therefore it's not necessary to return @li.
	 **/
	return(NULL);
}
