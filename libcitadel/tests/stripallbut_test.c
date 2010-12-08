#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "../lib/libcitadel.h"

main() {
	int i;
	long l;

	char *teststrings[] = {
		"Nmm <fghjk> tyutyu",
		"Sdd <> ghhjjkk",
		"<>",
		"",
		"Sdd <",
		"Df Ghjkl>",
		">< bggt",
		"FSDFSDFSDFSDF<><><>bggt",
		"FSDFSDFSDF<><eoeo><>bggt",
		"Abc<QWER>",
		"><",
		"Zxcv<dy<<lkj>>>"
	};

	char *strippedstrings[] = {
		"fghjk",
		"",
		"",
		"",
		"Sdd <",
		"Df Ghjkl>",
		">< bggt",
		"FSDFSDFSDFSDF<><><>bggt",
		"FSDFSDFSDF<><eoeo><>bggt",
		"QWER",
		"><",
		"lkj"
	};

	long strippedlens[] = {
		5,
		0,
		0,
		0,
		5,
		9,
		7,
		23,
		24,
		4,
		2,
		3
	};

	char foo[128];

	for (i=0; i<(sizeof(teststrings) / sizeof (char *)); ++i) {
		printf("Testing stripallbut()\n");
		strcpy(foo, teststrings[i]);
		l = stripallbut(foo, '<', '>');

		assert(!strcmp(foo, strippedstrings[i]));
		assert(strlen(foo) == strippedlens[i]);
	}

	exit(0);
}
