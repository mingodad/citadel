/*
 * $Id:  $
 */
/**
 *
 * \defgroup Wiki Wiki; Functions pertaining to rooms with a wiki view
 *
 */

/*@{*/
#include "webcit.h"



/** 
 * \brief Convert a string to something suitable as a wiki index
 *
 * \param s The string to be converted.
 */
void str_wiki_index(char *s)
{
	int i;

	if (s == NULL) return;

	/* First remove all non-alphanumeric characters */
	for (i=0; i<strlen(s); ++i) {
		if (!isalnum(s[i])) {
			strcpy(&s[i], &s[i+1]);
		}
	}

	/* Then make everything lower case */
	for (i=0; i<strlen(s); ++i) {
		s[i] = tolower(s[i]);
	}
}

/**
 * \brief Display a specific page from a wiki room
 *
 * \param roomname The name of the room containing the wiki
 * \param pagename The index of the page being requested
 */
void display_wiki_page(char *roomname, char *pagename)
{
	output_headers(1, 1, 1, 0, 0, 0);

	wprintf("roomname=%s<br>pagename=%s<br>\n", roomname, pagename);

	wDumpContent(1);
}


/** @} */
