/*
 * mime_parser.c
 *
 * This is a really bad attempt at writing a parser to handle multipart
 * messages -- in the case of WebCit, a form containing uploaded files.
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <ctype.h>
#include <string.h>
#include "mime_parser.h"
#include "webcit.h"
#include "child.h"



void extract_key(char *target, char *source, char *key)
{
	int a, b;

	strcpy(target, source);
	for (a = 0; a < strlen(target); ++a) {
		if ((!strncasecmp(&target[a], key, strlen(key)))
		    && (target[a + strlen(key)] == '=')) {
			strcpy(target, &target[a + strlen(key) + 1]);
			if (target[0] == 34)
				strcpy(target, &target[1]);
			for (b = 0; b < strlen(target); ++b)
				if (target[b] == 34)
					target[b] = 0;
			return;
		}
	}
	strcpy(target, "");
}



/*
 * The very back end for the component handler
 * (This function expects to be fed CONTENT ONLY, no headers)
 */
void do_something_with_it(char *content,
			  int length,
			  char *content_type,
			  char *content_disposition,
			  void (*CallBack)
			   (char *cbname,
			    char *cbfilename,
			    char *cbencoding,
			    void *cbcontent,
			    char *cbtype,
			    size_t cblength)
)
{
	char name[256];
	char filename[256];

	extract_key(name, content_disposition, " name");
	extract_key(filename, content_disposition, "filename");

	/* Nested multipart gets recursively fed back into the parser */
	if (!strncasecmp(content_type, "multipart", 9)) {
		mime_parser(content, length, content_type, CallBack);
	}
/**** OTHERWISE, HERE'S WHERE WE HANDLE THE STUFF!! *****/

	CallBack(name, filename, "", content, content_type, length);

/**** END OF STUFF-HANDLER ****/

}


/*
 * Take a part, figure out its length, and do something with it
 * (This function expects to be fed HEADERS+CONTENT)
 */
void handle_part(char *content,
		 int part_length,
		 char *supplied_content_type,
		 void (*CallBack)
		  (char *cbname,
		   char *cbfilename,
		   char *cbencoding,
		   void *cbcontent,
		   char *cbtype,
		   size_t cblength)
)
{
	char content_type[256];
	char content_disposition[256];
	char *start;
	char buf[512];
	int crlf = 0;		/* set to 1 for crlf-style newlines */
	int actual_length;

	strcpy(content_type, supplied_content_type);

	/* Strip off any leading blank lines. */
	start = content;
	while ((!strncmp(start, "\r", 1)) || (!strncmp(start, "\n", 1))) {
		++start;
		--part_length;
	}

	/* At this point all we have left is the headers and the content. */
	do {
		strcpy(buf, "");
		do {
			buf[strlen(buf) + 1] = 0;
			if (strlen(buf) < ((sizeof buf) - 1)) {
				strncpy(&buf[strlen(buf)], start, 1);
			}
			++start;
			--part_length;
		} while ((buf[strlen(buf) - 1] != 10) && (part_length > 0));
		if (part_length <= 0)
			return;
		buf[strlen(buf) - 1] = 0;
		if (buf[strlen(buf) - 1] == 13) {
			buf[strlen(buf) - 1] = 0;
			crlf = 1;
		}
		if (!strncasecmp(buf, "Content-type: ", 14)) {
			strcpy(content_type, &buf[14]);
		}
		if (!strncasecmp(buf, "Content-disposition: ", 21)) {
			strcpy(content_disposition, &buf[21]);
		}
	} while (strlen(buf) > 0);

	if (crlf)
		actual_length = part_length - 2;
	else
		actual_length = part_length - 1;

	/* Now that we've got this component isolated, what to do with it? */
	do_something_with_it(start, actual_length,
			     content_type, content_disposition, CallBack);

}


/*
 * Break out the components of a multipart message
 * (This function expects to be fed CONTENT ONLY, no headers)
 */


void mime_parser(char *content,
		 int ContentLength,
		 char *ContentType,
		 void (*CallBack)
		  (char *cbname,
		   char *cbfilename,
		   char *cbencoding,
		   void *cbcontent,
		   char *cbtype,
		   size_t cblength)
)
{
	char boundary[256];
	char endary[256];
	int have_boundary = 0;
	int a;
	char *ptr;
	char *beginning;
	int bytes_processed = 0;
	int part_length;

	/* If it's not multipart, don't process it as multipart */
	if (strncasecmp(ContentType, "multipart", 9)) {
		do_something_with_it(content, ContentLength,
				     ContentType, "", CallBack);
		return;
	}
	/* Figure out what the boundary is */
	strcpy(boundary, ContentType);
	for (a = 0; a < strlen(boundary); ++a) {
		if (!strncasecmp(&boundary[a], "boundary=", 9)) {
			boundary[0] = '-';
			boundary[1] = '-';
			strcpy(&boundary[2], &boundary[a + 9]);
			have_boundary = 1;
			a = 0;
		}
		if ((boundary[a] == 13) || (boundary[a] == 10)) {
			boundary[a] = 0;
		}
	}

	/* We can't process multipart messages without a boundary. */
	if (have_boundary == 0)
		return;
	strcpy(endary, boundary);
	strcat(endary, "--");

	ptr = content;

	/* Seek to the beginning of the next boundary */
	while (bytes_processed < ContentLength) {
		/* && (strncasecmp(ptr, boundary, strlen(boundary))) ) { */

		if (strncasecmp(ptr, boundary, strlen(boundary))) {
			++ptr;
			++bytes_processed;
		}
		/* See if we're at the end */
		if (!strncasecmp(ptr, endary, strlen(endary))) {
			return;
		}
		/* Seek to the end of the boundary string */
		if (!strncasecmp(ptr, boundary, strlen(boundary))) {
			while ((bytes_processed < ContentLength)
			       && (strncasecmp(ptr, "\n", 1))) {
				++ptr;
				++bytes_processed;
			}
			beginning = ptr;
			part_length = 0;
			while ((bytes_processed < ContentLength)
			       && (strncasecmp(ptr, boundary, strlen(boundary)))) {
				++ptr;
				++bytes_processed;
				++part_length;
			}
			handle_part(beginning, part_length, "", CallBack);
			/* Back off so we can see the next boundary */
			--ptr;
			--bytes_processed;
		}
	}
}
