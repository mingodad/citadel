#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#include <fcntl.h>

#include <unistd.h>
#include <stddef.h>


#include "../lib/libcitadel.h"

/* shamelesly copied from msgbase.h */
struct ma_info {
	int is_ma;		/* Set to 1 if we are using this stuff */
	int freeze;		/* Freeze the replacement chain because we're
				 * digging through a subsection */
	int did_print;		/* One alternative has been displayed */
	char chosen_part[128];	/* Which part of a m/a did we choose? */
	const char *printme;
	int chosen_pref;	/* Chosen part preference level (lower is better) */
	int use_fo_hooks;	/* Use fixed output hooks */
	int dont_decode;        /* should we call the decoder or not? */
};


/*
 * Callback function for mime parser that simply lists the part
 */
static void list_this_part(char *name, 
			   char *filename, 
			   char *partnum, 
			   char *disp,
			   void *content, 
			   char *cbtype, 
			   char *cbcharset, 
			   size_t length, 
			   char *encoding,
			   char *cbid, 
			   void *cbuserdata)
{
	struct ma_info *ma;
	
	ma = (struct ma_info *)cbuserdata;
	if (ma->is_ma == 0) {
		printf("part=%s|%s|%s|%s|%s|%ld|%s|%s\n",
			name, 
			filename, 
			partnum, 
			disp, 
			cbtype, 
			(long)length, 
			cbid, 
			cbcharset);
	}
}

/* 
 * Callback function for multipart prefix
 */
static void list_this_pref(char *name, 
			   char *filename, 
			   char *partnum, 
			   char *disp,
			   void *content, 
			   char *cbtype, 
			   char *cbcharset, 
			   size_t length, 
			   char *encoding,
			   char *cbid, 
			   void *cbuserdata)
{
	struct ma_info *ma;
	
	ma = (struct ma_info *)cbuserdata;
	if (!strcasecmp(cbtype, "multipart/alternative")) {
		++ma->is_ma;
	}

	if (ma->is_ma == 0) {
		printf("pref=%s|%s\n", partnum, cbtype);
	}
}

/* 
 * Callback function for multipart sufffix
 */
static void list_this_suff(char *name, 
			   char *filename, 
			   char *partnum, 
			   char *disp,
			   void *content, 
			   char *cbtype, 
			   char *cbcharset, 
			   size_t length, 
			   char *encoding,
			   char *cbid, 
			   void *cbuserdata)
{
	struct ma_info *ma;
	
	ma = (struct ma_info *)cbuserdata;
	if (ma->is_ma == 0) {
		printf("suff=%s|%s\n", partnum, cbtype);
	}
	if (!strcasecmp(cbtype, "multipart/alternative")) {
		--ma->is_ma;
	}
}


/*
 * Callback function for mime parser that opens a section for downloading
 */
static void mime_download(char *name, 
			  char *filename, 
			  char *partnum, 
			  char *disp,
			  void *content, 
			  char *cbtype, 
			  char *cbcharset, 
			  size_t length,
			  char *encoding, 
			  char *cbid, 
			  void *cbuserdata)
{
	int rc = 0;

	/* Silently go away if there's already a download open. */

	struct ma_info *ma;
	
	ma = (struct ma_info *)cbuserdata;

	if ((!IsEmptyStr(partnum) && (!strcasecmp(ma->printme, partnum)))) {
		char *decoded = NULL;
		size_t bytes_decoded;
		rc = mime_decode_now (content, 
				      length,
				      encoding,
				      &decoded,
				      &bytes_decoded);
		if (rc < 0) {
			printf("failed to decode content\n");
			return;
		}
		if (rc == 0){
			rc = write(STDOUT_FILENO, content, length);
		}
		else {
			rc = write(STDOUT_FILENO, decoded, bytes_decoded);
			free(decoded);
		}
	}
}



/*
 * Callback function for mime parser that outputs a section all at once.
 * We can specify the desired section by part number *or* content-id.
 * /
void mime_spew_section(char *name, 
		       char *filename, 
		       char *partnum, 
		       char *disp,
		       void *content, 
		       char *cbtype, 
		       char *cbcharset, 
		       size_t length,
		       char *encoding, 
		       char *cbid, 
		       void *cbuserdata)
{
	int *found_it = (int *)cbuserdata;

	if (
		(!IsEmptyStr(partnum) && (!strcasecmp(CC->download_desired_section, partnum)))
	||	(!IsEmptyStr(cbid) && (!strcasecmp(CC->download_desired_section, cbid)))
	) {
		*found_it = 1;
		printf("%d %d|-1|%s|%s|%s\n",
			BINARY_FOLLOWS,
			(int)length,
			filename,
			cbtype,
			cbcharset
		);
		fwrite(STDOUT, content, length);
	}
}

*/




int main(int argc, char* argv[])
{
	char a;
	int fd;
	char *filename = NULL;
	struct stat statbuf;
	const char *Err;

	StrBuf *MimeBuf;
	long MimeLen;
	char *MimeStr;
	struct ma_info ma;
	int do_proto = 0;
	int dont_decode = 1;

	setvbuf(stdout, NULL, _IONBF, 0);
	memset(&ma, 0, sizeof(struct ma_info));

	while ((a = getopt(argc, argv, "dpf:P:")) != EOF)
	{
		switch (a) {
		case 'f':
			filename = optarg;
			break;
		case 'p':
			do_proto = 1;
			break;
		case 'd':
			dont_decode = 0;
			break;
		case 'P':
			ma.printme = optarg;
		}
	}
	StartLibCitadel(8);

	if (filename == NULL) {
		printf("Filename requried! -f\n");
		return 1;
	}
	fd = open(filename, 0);
	if (fd < 0) {
		printf("Error opening file [%s] %d [%s]\n", filename, errno, strerror(errno));
		return 1;
	}
	if (fstat(fd, &statbuf) == -1) {
		printf("Error stating file [%s] %d [%s]\n", filename, errno, strerror(errno));
		return 1;
	}
	MimeBuf = NewStrBufPlain(NULL, statbuf.st_size + 1);
	if (StrBufReadBLOB(MimeBuf, &fd, 1, statbuf.st_size, &Err) < 0) {
		printf("Error reading file [%s] %d [%s] [%s]\n", filename, errno, strerror(errno), Err);
		FreeStrBuf(&MimeBuf);
		return 1;
	}
	MimeLen = StrLength(MimeBuf);
	MimeStr = SmashStrBuf(&MimeBuf);

	if (ma.printme == NULL)
		mime_parser(MimeStr, MimeStr + MimeLen,
			    (do_proto ? *list_this_part : NULL),
			    (do_proto ? *list_this_pref : NULL),
			    (do_proto ? *list_this_suff : NULL),
			    (void *)&ma, dont_decode);
	else 
		mime_parser(MimeStr, MimeStr + MimeLen,
			    *mime_download, NULL, NULL, (void *)&ma, dont_decode);

	free(MimeStr);
	return 0;
}
