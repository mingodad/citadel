/*
 * vcard_edit.c
 *
 * Handles editing of vCard objects.
 *
 * $Id$
 */


#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <limits.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <stdarg.h>
#include <pthread.h>
#include <signal.h>
#include "webcit.h"
#include "vcard.h"



void edit_vcard(void) {
	char buf[SIZ];
	char *serialized_vcard = NULL;
	size_t total_len = 0;
	size_t bytes = 0;
	size_t thisblock = 0;
	struct vCard *v;
	int i;
	char *prop;

	output_headers(1);
	sprintf(buf, "OPNA %s|%s", bstr("msgnum"), bstr("partnum") );
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0] != '2') {
		wDumpContent(1);
		return;
	}

	total_len = atoi(&buf[4]);
	serialized_vcard = malloc(total_len + 1);
	while (bytes < total_len) {
		thisblock = 4000;
		if ((total_len - bytes) < thisblock) thisblock = total_len - bytes;
		sprintf(buf, "READ %d|%d", bytes, thisblock);
		serv_puts(buf);
		serv_gets(buf);
		if (buf[0] == '6') {
			thisblock = atoi(&buf[4]);
			serv_read(&serialized_vcard[bytes], thisblock);
			bytes += thisblock;
		}
		else {
			wprintf("Error: %s<BR>\n", &buf[4]);
		}
	}

	serv_puts("CLOS");
	serv_gets(buf);
	serialized_vcard[total_len + 1] = 0;

	v = vcard_load(serialized_vcard);
	free(serialized_vcard);

	wprintf("<BLINK>   FIXME    </BLINK><BR><BR>\n"
		"This needs to be implemented as an editable form.<BR><BR>\n");

	i = 0;
	while (prop = vcard_get_prop(v, "", 0, i++), prop != NULL) {
		escputs(prop);
		wprintf("<BR>\n");
	}
	
	vcard_free(v);
	 
	wDumpContent(1);
}
