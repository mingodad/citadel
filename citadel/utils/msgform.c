/*
 * This is simply a filter that converts Citadel binary message format
 * to readable, formatted output.
 * 
 * If the -q (quiet or qwk) flag is used, only the message text prints, and
 * then it stops at the end of the first message it prints.
 * 
 * This utility isn't very useful anymore.
 *
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <libcitadel.h>

int qwk = 0;

int fpgetfield(FILE * fp, char *string);
int fmout(int width, FILE * fp);



int main(int argc, char **argv)
{
	struct tm tm;
	int a, b, e, aflag;
	char bbb[1024];
	char subject[1024];
	FILE *fp;
	time_t now;

	if (argc == 2)
		if (!strcmp(argv[1], "-q"))
			qwk = 1;
	fp = stdin;
	if (argc == 2)
		if (strcmp(argv[1], "-q")) {
			fp = fopen(argv[1], "r");
			if (fp == NULL) {
				fprintf(stderr, "%s: cannot open %s: %s\n",
					argv[0], argv[1], strerror(errno));
				exit(errno);
			}
		}

TOP:	do {
		e = getc(fp);
		if (e < 0)
			exit(0);
	} while (e != 255);
	strcpy(subject, "");
	getc(fp);
	aflag = getc(fp);
	if (qwk == 0)
		printf(" ");

	do {
		b = getc(fp);
		if (b == 'M') {
			if (qwk == 0) {
				printf("\n");
				if (!IsEmptyStr(subject))
					printf("Subject: %s\n", subject);
			}
			if (aflag != 1)
				fmout(80, fp);
			else
				while (a = getc(fp), a > 0) {
					if (a == 13)
						putc(10, stdout);
					else
						putc(a, stdout);
				}
		}
		if ((b != 'M') && (b > 0))
			fpgetfield(fp, bbb);
		if (b == 'U')
			strcpy(subject, bbb);
		if (qwk == 0) {
			if (b == 'A')
				printf("from %s ", bbb);
			if (b == 'N')
				printf("@%s ", bbb);
			if (b == 'O')
				printf("in %s> ", bbb);
			if (b == 'R')
				printf("to %s ", bbb);
			if (b == 'T') {
				now = atol(bbb);
				localtime_r(&now, &tm);
				strcpy(bbb, asctime(&tm));
				bbb[strlen(bbb) - 1] = 0;
				printf("%s ", &bbb[4]);
			}
		}
	} while ((b != 'M') && (b > 0));
	if (qwk == 0)
		printf("\n");
	if (qwk == 1)
		exit(0);
	goto TOP;
}

int fpgetfield(FILE * fp, char *string)

{				/* level-2 break out next null-terminated string */
	int a, b;
	strcpy(string, "");
	a = 0;
	do {
		b = getc(fp);
		if (b < 1) {
			string[a] = 0;
			return (0);
		}
		string[a] = b;
		++a;
	} while (b != 0);
	return (0);
}

int fmout(int width, FILE * fp)
{
	int a, b, c;
	int real = 0;
	int old = 0;
	char aaa[140];

	strcpy(aaa, "");
	old = 255;
	c = 1;			/* c is the current pos */
FMTA:	old = real;
	a = getc(fp);
	real = a;
	if (a <= 0)
		goto FMTEND;

	if (((a == 13) || (a == 10)) && (old != 13) && (old != 10))
		a = 32;
	if (((old == 13) || (old == 10)) && (isspace(real))) {
		printf("\n");
		c = 1;
	}
	if (a > 126)
		goto FMTA;

	if (a > 32) {
		if (((strlen(aaa) + c) > (width - 5))
		    && (strlen(aaa) > (width - 5))) {
			printf("\n%s", aaa);
			c = strlen(aaa);
			aaa[0] = 0;
		}
		b = strlen(aaa);
		aaa[b] = a;
		aaa[b + 1] = 0;
	}
	if (a == 32) {
		if ((strlen(aaa) + c) > (width - 5)) {
			printf("\n");
			c = 1;
		}
		printf("%s ", aaa);
		++c;
		c = c + strlen(aaa);
		strcpy(aaa, "");
		goto FMTA;
	}
	if ((a == 13) || (a == 10)) {
		printf("%s\n", aaa);
		c = 1;
		strcpy(aaa, "");
		goto FMTA;
	}
	goto FMTA;

FMTEND:	printf("\n");
	return (0);
}
