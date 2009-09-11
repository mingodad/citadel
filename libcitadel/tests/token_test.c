
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libcitadel.h>


int main(int argc, char **argv) {
	StrBuf *setstr;
	StrBuf *lostr;
	StrBuf *histr;
	StrBuf *vset;
	const char *pvset;
	int i = 0;

	// char *teststring = "40:24524,24662,24673,27869:27935,28393,28426,31247:31258,31731,31749,31761,31778,31782,31801:31803,31813,31904,31915,33708,33935,34619,34672,34720:34723,34766,34835,37594,38854,39235,39942,40030,40142,40520,40815,40907,41201,41578,41781,41954,42292,43110,43565,43801,43998,44180,44241,44295,44401,44561,44635,44798,44861,44946,45022,45137:45148,45166,45179,45707,47114,47141:47157,47194,47314,47349,47386,47489,47496,47534:47543,54460,54601,54637:54652";
	char *teststring = "one,two,three";
	setstr = NewStrBuf();
	vset = NewStrBufPlain(teststring, -1);

	while (StrBufExtract_NextToken(setstr, vset, &pvset, ',')) {
		printf("Token: '%s'\n", ChrPtr(setstr));
	}

	exit(0);
}
