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
#include "webcit.h"
#include "child.h"

void mime_parser(char *content, int ContentLength, char *ContentType) {
	char boundary[256];



	}
