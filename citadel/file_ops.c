/* 
 * $Id$
 *
 * Server functions which handle file transfers and room directories.
 *
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <sys/stat.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <limits.h>
#include "citadel.h"
#include "server.h"
#include "config.h"
#include "file_ops.h"
#include "sysdep_decls.h"
#include "user_ops.h"
#include "support.h"
#include "room_ops.h"
#include "msgbase.h"
#include "tools.h"
#include "citserver.h"

#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif

/*
 * network_talking_to()  --  concurrency checker
 */
int network_talking_to(char *nodename, int operation) {

	static char *nttlist = NULL;
	char *ptr = NULL;
	int i;
	char buf[SIZ];
	int retval = 0;

	begin_critical_section(S_NTTLIST);

	switch(operation) {

		case NTT_ADD:
			if (nttlist == NULL) nttlist = strdup("");
			if (nttlist == NULL) break;
			nttlist = (char *)realloc(nttlist,
				(strlen(nttlist) + strlen(nodename) + 3) );
			strcat(nttlist, "|");
			strcat(nttlist, nodename);
			break;

		case NTT_REMOVE:
			if (nttlist == NULL) break;
			if (IsEmptyStr(nttlist)) break;
			ptr = malloc(strlen(nttlist));
			if (ptr == NULL) break;
			strcpy(ptr, "");
			for (i = 0; i < num_tokens(nttlist, '|'); ++i) {
				extract_token(buf, nttlist, i, '|', sizeof buf);
				if ( (!IsEmptyStr(buf))
				     && (strcasecmp(buf, nodename)) ) {
						strcat(ptr, buf);
						strcat(ptr, "|");
				}
			}
			free(nttlist);
			nttlist = ptr;
			break;

		case NTT_CHECK:
			if (nttlist == NULL) break;
			if (IsEmptyStr(nttlist)) break;
			for (i = 0; i < num_tokens(nttlist, '|'); ++i) {
				extract_token(buf, nttlist, i, '|', sizeof buf);
				if (!strcasecmp(buf, nodename)) ++retval;
			}
			break;
	}

	if (nttlist != NULL) lprintf(CTDL_DEBUG, "nttlist=<%s>\n", nttlist);
	end_critical_section(S_NTTLIST);
	return(retval);
}




/*
 * Server command to delete a file from a room's directory
 */
void cmd_delf(char *filename)
{
	char pathname[64];
	int a;

	if (CtdlAccessCheck(ac_room_aide))
		return;

	if ((CC->room.QRflags & QR_DIRECTORY) == 0) {
		cprintf("%d No directory in this room.\n",
			ERROR + NOT_HERE);
		return;
	}

	if (IsEmptyStr(filename)) {
		cprintf("%d You must specify a file name.\n",
			ERROR + FILE_NOT_FOUND);
		return;
	}
	for (a = 0; a < strlen(filename); ++a) {
		if (filename[a] == '/') {
			filename[a] = '_';
		}
	}
	snprintf(pathname, sizeof pathname,
			 "%s/%s/%s",
			 ctdl_file_dir,
			 CC->room.QRdirname, filename);
	a = unlink(pathname);
	if (a == 0) {
		cprintf("%d File '%s' deleted.\n", CIT_OK, pathname);
	}
	else {
		cprintf("%d File '%s' not found.\n",
			ERROR + FILE_NOT_FOUND, pathname);
	}
}




/*
 * move a file from one room directory to another
 */
void cmd_movf(char *cmdbuf)
{
	char filename[PATH_MAX];
	char pathname[PATH_MAX];
	char newpath[PATH_MAX];
	char newroom[ROOMNAMELEN];
	char buf[PATH_MAX];
	int a;
	struct ctdlroom qrbuf;

	extract_token(filename, cmdbuf, 0, '|', sizeof filename);
	extract_token(newroom, cmdbuf, 1, '|', sizeof newroom);

	if (CtdlAccessCheck(ac_room_aide)) return;

	if ((CC->room.QRflags & QR_DIRECTORY) == 0) {
		cprintf("%d No directory in this room.\n",
			ERROR + NOT_HERE);
		return;
	}

	if (IsEmptyStr(filename)) {
		cprintf("%d You must specify a file name.\n",
			ERROR + FILE_NOT_FOUND);
		return;
	}

	for (a = 0; a < strlen(filename); ++a) {
		if (filename[a] == '/') {
			filename[a] = '_';
		}
	}
	snprintf(pathname, sizeof pathname, "./files/%s/%s",
		 CC->room.QRdirname, filename);
	if (access(pathname, 0) != 0) {
		cprintf("%d File '%s' not found.\n",
			ERROR + FILE_NOT_FOUND, pathname);
		return;
	}

	if (getroom(&qrbuf, newroom) != 0) {
		cprintf("%d '%s' does not exist.\n", ERROR + ROOM_NOT_FOUND, newroom);
		return;
	}
	if ((qrbuf.QRflags & QR_DIRECTORY) == 0) {
		cprintf("%d '%s' is not a directory room.\n",
			ERROR + NOT_HERE, qrbuf.QRname);
		return;
	}
	snprintf(newpath, sizeof newpath, "./files/%s/%s", qrbuf.QRdirname,
		 filename);
	if (link(pathname, newpath) != 0) {
		cprintf("%d Couldn't move file: %s\n", ERROR + INTERNAL_ERROR,
			strerror(errno));
		return;
	}
	unlink(pathname);

	/* this is a crude method of copying the file description */
	snprintf(buf, sizeof buf,
		 "cat ./files/%s/filedir |grep %s >>./files/%s/filedir",
		 CC->room.QRdirname, filename, qrbuf.QRdirname);
	system(buf);
	cprintf("%d File '%s' has been moved.\n", CIT_OK, filename);
}


/*
 * send a file over the net
 */
void cmd_netf(char *cmdbuf)
{
	char pathname[256], filename[256], destsys[256], buf[256];
	char outfile[256];
	int a, e;
	time_t now;
	FILE *ofp;
	static int seq = 1;

	extract_token(filename, cmdbuf, 0, '|', sizeof filename);
	extract_token(destsys, cmdbuf, 1, '|', sizeof destsys);

	if (CtdlAccessCheck(ac_room_aide)) return;

	if ((CC->room.QRflags & QR_DIRECTORY) == 0) {
		cprintf("%d No directory in this room.\n",
			ERROR + NOT_HERE);
		return;
	}

	if (IsEmptyStr(filename)) {
		cprintf("%d You must specify a file name.\n",
			ERROR + FILE_NOT_FOUND);
		return;
	}

	for (a = 0; a < strlen(filename); ++a) {
		if (filename[a] == '/') {
			filename[a] = '_';
		}
	}
	snprintf(pathname, sizeof pathname, "./files/%s/%s",
		 CC->room.QRdirname, filename);
	if (access(pathname, 0) != 0) {
		cprintf("%d File '%s' not found.\n",
			ERROR + FILE_NOT_FOUND, pathname);
		return;
	}
	snprintf(buf, sizeof buf, "sysop@%s", destsys);
	e = alias(buf);
	if (e != MES_IGNET) {
		cprintf("%d No such system: '%s'\n",
			ERROR + NO_SUCH_SYSTEM, destsys);
		return;
	}
	snprintf(outfile, sizeof outfile,
			 "%s/nsf.%04lx.%04x",
			 ctdl_netin_dir,
			 (long)getpid(), ++seq);
	ofp = fopen(outfile, "a");
	if (ofp == NULL) {
		cprintf("%d internal error\n", ERROR + INTERNAL_ERROR);
		return;
	}

	putc(255, ofp);
	putc(MES_NORMAL, ofp);
	putc(0, ofp);
	fprintf(ofp, "P%s", CC->user.fullname);
	putc(0, ofp);
	time(&now);
	fprintf(ofp, "T%ld", (long) now);
	putc(0, ofp);
	fprintf(ofp, "A%s", CC->user.fullname);
	putc(0, ofp);
	fprintf(ofp, "O%s", CC->room.QRname);
	putc(0, ofp);
	fprintf(ofp, "N%s", NODENAME);
	putc(0, ofp);
	fprintf(ofp, "D%s", destsys);
	putc(0, ofp);
	fprintf(ofp, "SFILE");
	putc(0, ofp);
	putc('M', ofp);
	fclose(ofp);

	snprintf(buf, sizeof buf,
			 "cd %s/%s; uuencode %s <%s 2>/dev/null >>%s",
			 ctdl_file_dir,
			 /* FIXME: detect uuencode while installation? or inline */
			 CC->room.QRdirname, filename, filename, outfile);
	system(buf);

	ofp = fopen(outfile, "a");
	putc(0, ofp);
	fclose(ofp);

	cprintf("%d File '%s' has been sent to %s.\n", CIT_OK, filename,
		destsys);
	/* FIXME start a network run here. */
	return;
}

/*
 * This code is common to all commands which open a file for downloading,
 * regardless of whether it's a file from the directory, an image, a network
 * spool file, a MIME attachment, etc.
 * It examines the file and displays the OK result code and some information
 * about the file.  NOTE: this stuff is Unix dependent.
 */
void OpenCmdResult(char *filename, char *mime_type)
{
	struct stat statbuf;
	time_t modtime;
	long filesize;

	fstat(fileno(CC->download_fp), &statbuf);
	filesize = (long) statbuf.st_size;
	modtime = (time_t) statbuf.st_mtime;

	cprintf("%d %ld|%ld|%s|%s\n",
		CIT_OK, filesize, (long)modtime, filename, mime_type);
}


/*
 * open a file for downloading
 */
void cmd_open(char *cmdbuf)
{
	char filename[256];
	char pathname[PATH_MAX];
	int a;

	extract_token(filename, cmdbuf, 0, '|', sizeof filename);

	if (CtdlAccessCheck(ac_logged_in)) return;

	if ((CC->room.QRflags & QR_DIRECTORY) == 0) {
		cprintf("%d No directory in this room.\n",
			ERROR + NOT_HERE);
		return;
	}

	if (IsEmptyStr(filename)) {
		cprintf("%d You must specify a file name.\n",
			ERROR + FILE_NOT_FOUND);
		return;
	}

	if (CC->download_fp != NULL) {
		cprintf("%d You already have a download file open.\n",
			ERROR + RESOURCE_BUSY);
		return;
	}

	for (a = 0; a < strlen(filename); ++a) {
		if (filename[a] == '/') {
			filename[a] = '_';
		}
	}

	snprintf(pathname, sizeof pathname,
			 "%s/%s/%s",
			 ctdl_file_dir,
			 CC->room.QRdirname, filename);
	CC->download_fp = fopen(pathname, "r");

	if (CC->download_fp == NULL) {
		cprintf("%d cannot open %s: %s\n",
			ERROR + INTERNAL_ERROR, pathname, strerror(errno));
		return;
	}

	OpenCmdResult(filename, "application/octet-stream");
}

/*
 * open an image file
 */
void cmd_oimg(char *cmdbuf)
{
	char filename[256];
	char pathname[PATH_MAX];
	struct ctdluser usbuf;
	char which_user[USERNAME_SIZE];
	int which_floor;
	int a;

	extract_token(filename, cmdbuf, 0, '|', sizeof filename);

	if (IsEmptyStr(filename)) {
		cprintf("%d You must specify a file name.\n",
			ERROR + FILE_NOT_FOUND);
		return;
	}

	if (CC->download_fp != NULL) {
		cprintf("%d You already have a download file open.\n",
			ERROR + RESOURCE_BUSY);
		return;
	}

	if (!strcasecmp(filename, "_userpic_")) {
		extract_token(which_user, cmdbuf, 1, '|', sizeof which_user);
		if (getuser(&usbuf, which_user) != 0) {
			cprintf("%d No such user.\n",
				ERROR + NO_SUCH_USER);
			return;
		}
		snprintf(pathname, sizeof pathname, 
				 "%s/%ld.gif",
				 ctdl_usrpic_dir,
				 usbuf.usernum);
	} else if (!strcasecmp(filename, "_floorpic_")) {
		which_floor = extract_int(cmdbuf, 1);
		snprintf(pathname, sizeof pathname,
				 "%s/floor.%d.gif",
				 ctdl_image_dir, which_floor);
	} else if (!strcasecmp(filename, "_roompic_")) {
		assoc_file_name(pathname, sizeof pathname, &CC->room, ctdl_image_dir);
	} else {
		for (a = 0; a < strlen(filename); ++a) {
			filename[a] = tolower(filename[a]);
			if (filename[a] == '/') {
				filename[a] = '_';
			}
		}
		snprintf(pathname, sizeof pathname,
				 "%s/%s.gif",
				 ctdl_image_dir,
				 filename);
	}

	CC->download_fp = fopen(pathname, "rb");
	if (CC->download_fp == NULL) {
		cprintf("%d Cannot open %s: %s\n",
			ERROR + FILE_NOT_FOUND, pathname, strerror(errno));
		return;
	}

	OpenCmdResult(pathname, "image/gif");
}

/*
 * open a file for uploading
 */
void cmd_uopn(char *cmdbuf)
{
	int a;

	extract_token(CC->upl_file, cmdbuf, 0, '|', sizeof CC->upl_file);
	extract_token(CC->upl_comment, cmdbuf, 1, '|', sizeof CC->upl_comment);

	if (CtdlAccessCheck(ac_logged_in)) return;

	if ((CC->room.QRflags & QR_DIRECTORY) == 0) {
		cprintf("%d No directory in this room.\n",
			ERROR + NOT_HERE);
		return;
	}

	if (IsEmptyStr(CC->upl_file)) {
		cprintf("%d You must specify a file name.\n",
			ERROR + FILE_NOT_FOUND);
		return;
	}

	if (CC->upload_fp != NULL) {
		cprintf("%d You already have a upload file open.\n",
			ERROR + RESOURCE_BUSY);
		return;
	}

	for (a = 0; a < strlen(CC->upl_file); ++a) {
		if (CC->upl_file[a] == '/') {
			CC->upl_file[a] = '_';
		}
	}
	snprintf(CC->upl_path, sizeof CC->upl_path, 
			 "%s/%s/%s",
			 ctdl_file_dir,
			 CC->room.QRdirname, CC->upl_file);
	snprintf(CC->upl_filedir, sizeof CC->upl_filedir,
			 "%s/%s/filedir", 
			 ctdl_file_dir,
			 CC->room.QRdirname);

	CC->upload_fp = fopen(CC->upl_path, "r");
	if (CC->upload_fp != NULL) {
		fclose(CC->upload_fp);
		CC->upload_fp = NULL;
		cprintf("%d '%s' already exists\n",
			ERROR + ALREADY_EXISTS, CC->upl_path);
		return;
	}

	CC->upload_fp = fopen(CC->upl_path, "wb");
	if (CC->upload_fp == NULL) {
		cprintf("%d Cannot open %s: %s\n",
			ERROR + INTERNAL_ERROR, CC->upl_path, strerror(errno));
		return;
	}
	cprintf("%d Ok\n", CIT_OK);
}



/*
 * open an image file for uploading
 */
void cmd_uimg(char *cmdbuf)
{
	int is_this_for_real;
	char basenm[256];
	int which_floor;
	int a;

	if (num_parms(cmdbuf) < 2) {
		cprintf("%d Usage error.\n", ERROR + ILLEGAL_VALUE);
		return;
	}

	is_this_for_real = extract_int(cmdbuf, 0);
	extract_token(basenm, cmdbuf, 1, '|', sizeof basenm);
	if (CC->upload_fp != NULL) {
		cprintf("%d You already have an upload file open.\n",
			ERROR + RESOURCE_BUSY);
		return;
	}

	strcpy(CC->upl_path, "");

	for (a = 0; a < strlen(basenm); ++a) {
		basenm[a] = tolower(basenm[a]);
		if (basenm[a] == '/') {
			basenm[a] = '_';
		}
	}

	if (CC->user.axlevel >= 6) {
		snprintf(CC->upl_path, sizeof CC->upl_path, 
				 "%s/%s",
				 ctdl_image_dir,
				 basenm);
	}

	if (!strcasecmp(basenm, "_userpic_")) {
		snprintf(CC->upl_path, sizeof CC->upl_path,
				 "%s/%ld.gif",
				 ctdl_usrpic_dir,
				 CC->user.usernum);
	}

	if ((!strcasecmp(basenm, "_floorpic_"))
	    && (CC->user.axlevel >= 6)) {
		which_floor = extract_int(cmdbuf, 2);
		snprintf(CC->upl_path, sizeof CC->upl_path,
				 "%s/floor.%d.gif",
				 ctdl_image_dir,
				 which_floor);
	}

	if ((!strcasecmp(basenm, "_roompic_")) && (is_room_aide())) {
		assoc_file_name(CC->upl_path, sizeof CC->upl_path, &CC->room, ctdl_image_dir);
	}

	if (IsEmptyStr(CC->upl_path)) {
		cprintf("%d Higher access required.\n",
			ERROR + HIGHER_ACCESS_REQUIRED);
		return;
	}

	if (is_this_for_real == 0) {
		cprintf("%d Ok to send image\n", CIT_OK);
		return;
	}

	CC->upload_fp = fopen(CC->upl_path, "wb");
	if (CC->upload_fp == NULL) {
		cprintf("%d Cannot open %s: %s\n",
			ERROR + INTERNAL_ERROR, CC->upl_path, strerror(errno));
		return;
	}
	cprintf("%d Ok\n", CIT_OK);
	CC->upload_type = UPL_IMAGE;
}


/*
 * close the download file
 */
void cmd_clos(void)
{
	char buf[256];

	if (CC->download_fp == NULL) {
		cprintf("%d You don't have a download file open.\n",
			ERROR + RESOURCE_NOT_OPEN);
		return;
	}

	fclose(CC->download_fp);
	CC->download_fp = NULL;

	if (CC->dl_is_net == 1) {
		CC->dl_is_net = 0;
		snprintf(buf, sizeof buf, 
				 "%s/%s",
				 ctdl_netout_dir,
				 CC->net_node);
		unlink(buf);
	}

	cprintf("%d Ok\n", CIT_OK);
}


/*
 * abort an upload
 */
void abort_upl(struct CitContext *who)
{
	if (who->upload_fp != NULL) {
		fclose(who->upload_fp);
		who->upload_fp = NULL;
		unlink(CC->upl_path);
	}
}



/*
 * close the upload file
 */
void cmd_ucls(char *cmd)
{
	FILE *fp;
	char upload_notice[512];

	if (CC->upload_fp == NULL) {
		cprintf("%d You don't have an upload file open.\n", ERROR + RESOURCE_NOT_OPEN);
		return;
	}

	fclose(CC->upload_fp);
	CC->upload_fp = NULL;

	if ((!strcasecmp(cmd, "1")) && (CC->upload_type != UPL_FILE)) {
		CC->upload_type = UPL_FILE;
		cprintf("%d Upload completed.\n", CIT_OK);

		/* FIXME ... here we need to trigger a network run */

		return;
	}

	if (!strcasecmp(cmd, "1")) {
		cprintf("%d File '%s' saved.\n", CIT_OK, CC->upl_path);
		fp = fopen(CC->upl_filedir, "a");
		if (fp == NULL) {
			fp = fopen(CC->upl_filedir, "w");
		}
		if (fp != NULL) {
			fprintf(fp, "%s %s\n", CC->upl_file,
				CC->upl_comment);
			fclose(fp);
		}

		/* put together an upload notice */
		snprintf(upload_notice, sizeof upload_notice,
			"NEW UPLOAD: '%s'\n %s\n",
			CC->upl_file, CC->upl_comment);
		quickie_message(CC->curr_user, NULL, NULL, CC->room.QRname,
				upload_notice, 0, NULL);
	} else {
		abort_upl(CC);
		cprintf("%d File '%s' aborted.\n", CIT_OK, CC->upl_path);
	}
}



/*
 * read from the download file
 */
void cmd_read(char *cmdbuf)
{
	long start_pos;
	size_t bytes;
	size_t actual_bytes;
	char *buf = NULL;

	start_pos = extract_long(cmdbuf, 0);
	bytes = extract_int(cmdbuf, 1);

	if (CC->download_fp == NULL) {
		cprintf("%d You don't have a download file open.\n",
			ERROR + RESOURCE_NOT_OPEN);
		return;
	}

	if (bytes > 100000) bytes = 100000;
	buf = malloc(bytes + 1);

	fseek(CC->download_fp, start_pos, 0);
	actual_bytes = fread(buf, 1, bytes, CC->download_fp);
	cprintf("%d %d\n", BINARY_FOLLOWS, (int)actual_bytes);
	client_write(buf, actual_bytes);
	free(buf);
}



/*
 * write to the upload file
 */
void cmd_writ(char *cmdbuf)
{
	int bytes;
	char *buf;

	unbuffer_output();

	bytes = extract_int(cmdbuf, 0);

	if (CC->upload_fp == NULL) {
		cprintf("%d You don't have an upload file open.\n", ERROR + RESOURCE_NOT_OPEN);
		return;
	}

	if (bytes > 100000) {
		cprintf("%d You may not write more than 100000 bytes.\n",
			ERROR + TOO_BIG);
		return;
	}

	cprintf("%d %d\n", SEND_BINARY, bytes);
	buf = malloc(bytes + 1);
	client_read(buf, bytes);
	fwrite(buf, bytes, 1, CC->upload_fp);
	free(buf);
}




/*
 * cmd_ndop() - open a network spool file for downloading
 */
void cmd_ndop(char *cmdbuf)
{
	char pathname[256];
	struct stat statbuf;

	if (IsEmptyStr(CC->net_node)) {
		cprintf("%d Not authenticated as a network node.\n",
			ERROR + NOT_LOGGED_IN);
		return;
	}

	if (CC->download_fp != NULL) {
		cprintf("%d You already have a download file open.\n",
			ERROR + RESOURCE_BUSY);
		return;
	}

	snprintf(pathname, sizeof pathname, 
			 "%s/%s",
			 ctdl_netout_dir,
			 CC->net_node);

	/* first open the file in append mode in order to create a
	 * zero-length file if it doesn't already exist 
	 */
	CC->download_fp = fopen(pathname, "a");
	if (CC->download_fp != NULL)
		fclose(CC->download_fp);

	/* now open it */
	CC->download_fp = fopen(pathname, "r");
	if (CC->download_fp == NULL) {
		cprintf("%d cannot open %s: %s\n",
			ERROR + INTERNAL_ERROR, pathname, strerror(errno));
		return;
	}


	/* set this flag so other routines know that the download file
	 * currently open is a network spool file 
	 */
	CC->dl_is_net = 1;

	stat(pathname, &statbuf);
	cprintf("%d %ld\n", CIT_OK, (long)statbuf.st_size);
}

/*
 * cmd_nuop() - open a network spool file for uploading
 */
void cmd_nuop(char *cmdbuf)
{
	static int seq = 1;

	if (IsEmptyStr(CC->net_node)) {
		cprintf("%d Not authenticated as a network node.\n",
			ERROR + NOT_LOGGED_IN);
		return;
	}

	if (CC->upload_fp != NULL) {
		cprintf("%d You already have an upload file open.\n",
			ERROR + RESOURCE_BUSY);
		return;
	}

	snprintf(CC->upl_path, sizeof CC->upl_path,
			 "%s/%s.%04lx.%04x",
			 ctdl_netin_dir,
			 CC->net_node, 
			 (long)getpid(), 
			 ++seq);

	CC->upload_fp = fopen(CC->upl_path, "r");
	if (CC->upload_fp != NULL) {
		fclose(CC->upload_fp);
		CC->upload_fp = NULL;
		cprintf("%d '%s' already exists\n",
			ERROR + ALREADY_EXISTS, CC->upl_path);
		return;
	}

	CC->upload_fp = fopen(CC->upl_path, "w");
	if (CC->upload_fp == NULL) {
		cprintf("%d Cannot open %s: %s\n",
			ERROR + INTERNAL_ERROR, CC->upl_path, strerror(errno));
		return;
	}

	CC->upload_type = UPL_NET;
	cprintf("%d Ok\n", CIT_OK);
}
