/*
 * $Id$
 * 
 * Copyright 2003-2004 Rene Puls <rpuls@gmx.net> and
 *                     Oliver Feiler <kiza@kcore.de>
 *
 * http://kiza.kcore.de/software/snownews/
 * http://home.kcore.de/~kianga/study/c/xmlparse.c
 *
 * xmlparse.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
 

#include <string.h>

#include "config.h"
#include "xmlparse.h"
#include "conversions.h"

int saverestore;
struct newsitem *copy;
struct newsitem *firstcopy;

/* During the parsens one calls, if we meet a <channel> element. 
 * The function returns a new Struct for the new feed. */

void parse_rdf10_channel(struct feed *feed, xmlDocPtr doc, xmlNodePtr node) {
	xmlNodePtr cur;
	
	/* Free everything before we write to it again. */
	free (feed->title);
	free (feed->link);
	free (feed->description);
		
	if (feed->items != NULL) {
		while (feed->items->next_ptr != NULL) {
			feed->items = feed->items->next_ptr;
			free (feed->items->prev_ptr->data->title);
			free (feed->items->prev_ptr->data->link);
			free (feed->items->prev_ptr->data->guid);
			free (feed->items->prev_ptr->data->description);
			free (feed->items->prev_ptr->data);
			free (feed->items->prev_ptr);
		}
		free (feed->items->data->title);
		free (feed->items->data->link);
		free (feed->items->data->guid);
		free (feed->items->data->description);
		free (feed->items->data);
		free (feed->items);
	}
	
	/* At the moment we have still no Items, so set the list to null. */
	feed->items = NULL;
	feed->title = NULL;
	feed->link= NULL;
	feed->description = NULL;
	
	/* Go through all <channel> tags and extract the information */
	for (cur = node; cur != NULL; cur = cur->next) {
		if (cur->type != XML_ELEMENT_NODE)
			continue;
		if (xmlStrcmp(cur->name, "title") == 0) {
			feed->title = xmlNodeListGetString(doc, cur->children, 1);
			CleanupString (feed->title, 1);
			/* Remove trailing newline */
			if (feed->title != NULL) {
				if (strlen(feed->title) > 1) {
					if (feed->title[strlen(feed->title)-1] == '\n')
						feed->title[strlen(feed->title)-1] = '\0';
				}
			}
		}
		else if (xmlStrcmp(cur->name, "link") == 0) {
			feed->link = xmlNodeListGetString(doc, cur->children, 1);
			/* Remove trailing newline */
			if (feed->link != NULL) {
				if (strlen(feed->link) > 1) {
					if (feed->link[strlen(feed->link)-1] == '\n')
						feed->link[strlen(feed->link)-1] = '\0';
				}
			}
		}
		else if (xmlStrcmp(cur->name, "description") == 0) {
			feed->description = xmlNodeListGetString(doc, cur->children, 1);
			CleanupString (feed->description, 0);
		}
	}
}


void parse_rdf20_channel(struct feed *feed, xmlDocPtr doc, xmlNodePtr node)
{
	xmlNodePtr cur;
	
	/* Free everything before we write to it again. */
	free (feed->title);
	free (feed->link);
	free (feed->description);
		
	if (feed->items != NULL) {
		while (feed->items->next_ptr != NULL) {
			feed->items = feed->items->next_ptr;
			free (feed->items->prev_ptr->data->title);
			free (feed->items->prev_ptr->data->link);
			free (feed->items->prev_ptr->data->guid);
			free (feed->items->prev_ptr->data->description);
			free (feed->items->prev_ptr->data);
			free (feed->items->prev_ptr);
		}
		free (feed->items->data->title);
		free (feed->items->data->link);
		free (feed->items->data->guid);
		free (feed->items->data->description);
		free (feed->items->data);
		free (feed->items);
	}
	
	/* Im Augenblick haben wir noch keine Items, also die Liste auf NULL setzen. */
	feed->items = NULL;
	feed->title = NULL;
	feed->link = NULL;
	feed->description = NULL;
	
	/* Alle Tags im <channel> Tag durchgehen und die Informationen extrahieren */
	for (cur = node; cur != NULL; cur = cur->next) {
		if (cur->type != XML_ELEMENT_NODE)
			continue;
		if (xmlStrcmp(cur->name, "title") == 0) {
			feed->title = xmlNodeListGetString(doc, cur->children, 1);
			CleanupString (feed->title, 1);
			/* Remove trailing newline */
			if (feed->title != NULL) {
				if (strlen(feed->title) > 1) {
					if (feed->title[strlen(feed->title)-1] == '\n')
						feed->title[strlen(feed->title)-1] = '\0';
				}
			}
		}
		else if (xmlStrcmp(cur->name, "link") == 0) {
			feed->link = xmlNodeListGetString(doc, cur->children, 1);
			/* Remove trailing newline */
			if (feed->link != NULL) {
				if (strlen(feed->link) > 1) {
					if (feed->link[strlen(feed->link)-1] == '\n')
						feed->link[strlen(feed->link)-1] = '\0';
				}
			}
		}
		else if (xmlStrcmp(cur->name, "description") == 0) {
			feed->description = xmlNodeListGetString(doc, cur->children, 1);
			CleanupString (feed->description, 0);
		} else if (xmlStrcmp(cur->name, "item") == 0) {
			parse_rdf10_item(feed, doc, cur->children);
		}
	}
}

/* This function is called each mark, if we meet on. As parameter it needs the
 * current new feed (new feed struct *), as well as the current XML
 * document-acts and the current element, both comes directly of libxml.
 */

void parse_rdf10_item(struct feed *feed, xmlDocPtr doc, xmlNodePtr node) 
{
	xmlNodePtr cur;
	xmlChar *readstatusstring;

	struct newsitem *item;
	struct newsitem *current;
	
	/* Speicher für ein neues Newsitem reservieren */
	item = malloc(sizeof (struct newsitem));
	item->data = malloc (sizeof (struct newsdata));
	
	item->data->title = NULL;
	item->data->link = NULL;
	item->data->guid = NULL;
	item->data->description = NULL;
	item->data->readstatus = 0;
	item->data->parent = feed;
		
	/* Alle Tags im <item> Tag durchgehen und die Informationen extrahieren.
	   Selbe Vorgehensweise wie in der parse_channel() Funktion */
	for (cur = node; cur != NULL; cur = cur->next) {
		if (cur->type != XML_ELEMENT_NODE)
			continue;
		if (xmlStrcmp(cur->name, "title") == 0) {
			item->data->title = xmlNodeListGetString(doc, cur->children, 1);
			CleanupString (item->data->title, 1);
			/* Remove trailing newline */
			if (item->data->title != NULL) {
				if (strlen(item->data->title) > 1) {
					if (item->data->title[strlen(item->data->title)-1] == '\n')
						item->data->title[strlen(item->data->title)-1] = '\0';
				}
			}
		}
		else if (xmlStrcmp(cur->name, "link") == 0) {
			item->data->link = xmlNodeListGetString(doc, cur->children, 1);
			if (item->data->link == NULL) {
				if (xmlStrcmp(cur->name, "guid") == 0)
					item->data->link = xmlNodeListGetString(doc, cur->children, 1);
			}
			/* Remove trailing newline */
			if (item->data->link != NULL) {
				if (strlen(item->data->link) > 1) {
					if (item->data->link[strlen(item->data->link)-1] == '\n')
						item->data->link[strlen(item->data->link)-1] = '\0';
				}
			}
		}
		else if (xmlStrcmp(cur->name, "guid") == 0) {
			item->data->guid = xmlNodeListGetString(doc, cur->children, 1);
			if (item->data->guid == NULL) {
				if (xmlStrcmp(cur->name, "guid") == 0)
					item->data->guid = xmlNodeListGetString(doc, cur->children, 1);
			}
			/* Remove trailing newline */
			if (item->data->guid != NULL) {
				if (strlen(item->data->guid) > 1) {
					if (item->data->guid[strlen(item->data->guid)-1] == '\n')
						item->data->guid[strlen(item->data->guid)-1] = '\0';
				}
			}
		}
		else if (xmlStrcmp(cur->name, "description") == 0) {
			item->data->description = xmlNodeListGetString(doc, cur->children, 1);
			CleanupString (item->data->description, 0);
		}
		else if (xmlStrcmp(cur->name, "readstatus") == 0) {
			/* Will cause memory leak otherwise, xmlNodeListGetString must be freed. */
			readstatusstring = xmlNodeListGetString(doc, cur->children, 1);
			item->data->readstatus = atoi (readstatusstring);
			xmlFree (readstatusstring);
		}
	}
	
	/* If saverestore == 1, restore readstatus. */
	if (saverestore == 1) {
		for (current = firstcopy; current != NULL; current = current->next_ptr) {
			if ((current->data->link != NULL) && (item->data->link != NULL)) {
				if ((current->data->title != NULL) && (item->data->title != NULL)) {
					if ((strcmp(item->data->link, current->data->link) == 0) &&
						(strcmp(item->data->title, current->data->title) == 0))
						item->data->readstatus = current->data->readstatus;
				} else {
					if (strcmp(item->data->link, current->data->link) == 0)
						item->data->readstatus = current->data->readstatus;
				}
			}
		}
	}

	item->next_ptr = NULL;
	if (feed->items == NULL) {
		item->prev_ptr = NULL;
		feed->items = item;
	} else {
		item->prev_ptr = feed->items;
		while (item->prev_ptr->next_ptr != NULL)
			item->prev_ptr = item->prev_ptr->next_ptr;
		item->prev_ptr->next_ptr = item;
	}
}


/* rrr */

int DeXML (struct feed *cur_ptr) {
	xmlDocPtr doc;
	xmlNodePtr cur;
	struct newsitem *cur_item;
	
	if (cur_ptr->feed == NULL)
		return -1;
	
	saverestore = 0;
	/* Wenn cur_ptr->items != NULL dann können wir uns item->readstatus
	   zwischenspeichern. */
	if (cur_ptr->items != NULL) {
		saverestore = 1;
	
		firstcopy = NULL;
		
		/* Copy current newsitem struct. */	
		for (cur_item = cur_ptr->items; cur_item != NULL; cur_item = cur_item->next_ptr) {
			copy = malloc (sizeof(struct newsitem));
			copy->data = malloc (sizeof (struct newsdata));
			copy->data->title = NULL;
			copy->data->link = NULL;
			copy->data->guid = NULL;
			copy->data->description = NULL;
			copy->data->readstatus = cur_item->data->readstatus;
			if (cur_item->data->link != NULL)
				copy->data->link = strdup (cur_item->data->link);
			if (cur_item->data->title != NULL)
				copy->data->title = strdup (cur_item->data->title);
			
			copy->next_ptr = NULL;
			if (firstcopy == NULL) {
				copy->prev_ptr = NULL;
				firstcopy = copy;
			} else {
				copy->prev_ptr = firstcopy;
				while (copy->prev_ptr->next_ptr != NULL)
					copy->prev_ptr = copy->prev_ptr->next_ptr;
				copy->prev_ptr->next_ptr = copy;
			}
		}
	}
	
	/* xmlRecoverMemory:
	   parse an XML in-memory document and build a tree.
       In case the document is not Well Formed, a tree is built anyway. */
	doc = xmlRecoverMemory(cur_ptr->feed, strlen(cur_ptr->feed));
	
	if (doc == NULL)
		return 2;
	
	/* Das Root-Element finden (in unserem Fall sollte es "<RDF:RDF>" heißen.
	   Dabei wird das RDF: Prefix fürs Erste ignoriert, bis der Jaguar
	   herausfindet, wie man das genau auslesen kann (jau). */
	cur = xmlDocGetRootElement(doc);
	
	if (cur == NULL) {
		xmlFreeDoc (doc);
		return 2;
	}
	
	/* Überprüfen, ob das Element auch wirklich <RDF> heißt */
	if (xmlStrcmp(cur->name, "RDF") == 0) {
	
		/* Jetzt gehen wir alle Elemente im Dokument durch. Diese Schleife
		   selbst läuft jedoch nur durch die Elemente auf höchster Ebene
		   (bei HTML wären das nur HEAD und BODY), wandert also nicht die 
		   gesamte Struktur nach unten durch. Dafür sind die Funktionen zuständig, 
		   die wir dann in der Schleife selbst aufrufen. */
		for (cur = cur->children; cur != NULL; cur = cur->next) {
			if (cur->type != XML_ELEMENT_NODE)
				continue;
			if (xmlStrcmp(cur->name, "channel") == 0)
				parse_rdf10_channel(cur_ptr, doc, cur->children);
			if (xmlStrcmp(cur->name, "item") == 0)
				parse_rdf10_item(cur_ptr, doc, cur->children);
			/* Last-Modified is only used when reading from internal feeds (disk cache). */
			if (xmlStrcmp(cur->name, "lastmodified") == 0)
				cur_ptr->lastmodified = xmlNodeListGetString(doc, cur->children, 1);
		}
	} else if (xmlStrcmp(cur->name, "rss") == 0) {
		for (cur = cur->children; cur != NULL; cur = cur->next) {
			if (cur->type != XML_ELEMENT_NODE)
				continue;
			if (xmlStrcmp(cur->name, "channel") == 0)
				parse_rdf20_channel(cur_ptr, doc, cur->children);
		}
	} else {
		xmlFreeDoc(doc);
		return 3;
	}

	xmlFreeDoc(doc);
	
	if (saverestore == 1) {
		/* free struct newsitem *copy. */
		while (firstcopy->next_ptr != NULL) {
			firstcopy = firstcopy->next_ptr;
			free (firstcopy->prev_ptr->data->link);
			free (firstcopy->prev_ptr->data->guid);
			free (firstcopy->prev_ptr->data->title);
			free (firstcopy->prev_ptr->data);
			free (firstcopy->prev_ptr);
		}
		free (firstcopy->data->link);
		free (firstcopy->data->guid);
		free (firstcopy->data->title);
		free (firstcopy->data);
		free (firstcopy);
	}
	
	if (cur_ptr->original != NULL)
		free (cur_ptr->original);

	/* Set -> title to something if it's a NULL pointer to avoid crash with strdup below. */
	if (cur_ptr->title == NULL)
		cur_ptr->title = strdup (cur_ptr->feedurl);
	cur_ptr->original = strdup (cur_ptr->title);
	
	return 0;
}
