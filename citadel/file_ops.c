/* $Id$ */
#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
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

void cmd_delf(char *filename)
{
	char pathname[64];
	int a;

	if (CtdlAccessCheck(ac_room_aide)) return;

	if ((CC->quickroom.QRflags & QR_DIRECTORY) == 0) {
		cprintf("%d No directory in this room.\n",ERROR+NOT_HERE);
		return;
		}

	if (strlen(filename)==0) {
		cprintf("%d You must specify a file name.\n",
			ERROR+FILE_NOT_FOUND);
		return;
		}
	for (a=0; a<strlen(filename); ++a)
		if (filename[a]=='/') filename[a] = '_';
	snprintf(pathname,sizeof pathname,"./files/%s/%s",
		 CC->quickroom.QRdirname,filename);
	a=unlink(pathname);
	if (a==0) cprintf("%d File '%s' deleted.\n",OK,pathname);
	else cprintf("%d File '%s' not found.\n",ERROR+FILE_NOT_FOUND,pathname);
	}




/*
 * move a file from one room directory to another
 */
void cmd_movf(char *cmdbuf)
{
	char filename[256];
	char pathname[256];
	char newpath[256];
	char newroom[256];
	char buf[256];
	int a;
	struct quickroom qrbuf;

	extract(filename,cmdbuf,0);
	extract(newroom,cmdbuf,1);

	if (CtdlAccessCheck(ac_room_aide)) return;

	if ((CC->quickroom.QRflags & QR_DIRECTORY) == 0) {
		cprintf("%d No directory in this room.\n",ERROR+NOT_HERE);
		return;
		}

	if (strlen(filename)==0) {
		cprintf("%d You must specify a file name.\n",
			ERROR+FILE_NOT_FOUND);
		return;
		}

	for (a=0; a<strlen(filename); ++a)
		if (filename[a]=='/') filename[a] = '_';
	snprintf(pathname,sizeof pathname,"./files/%s/%s",
		 CC->quickroom.QRdirname,filename);
	if (access(pathname,0)!=0) {
		cprintf("%d File '%s' not found.\n",
			ERROR+FILE_NOT_FOUND,pathname);
		return;
		}

	if (getroom(&qrbuf, newroom)!=0) {
		cprintf("%d '%s' does not exist.\n",
			ERROR, newroom);
		return;
		}
	if ((qrbuf.QRflags & QR_DIRECTORY) == 0) {
		cprintf("%d '%s' is not a directory room.\n",
			ERROR+NOT_HERE,qrbuf.QRname);
		return;
		}
	snprintf(newpath,sizeof newpath,"./files/%s/%s",qrbuf.QRdirname,
		 filename);
	if (link(pathname,newpath)!=0) {
		cprintf("%d Couldn't move file: %s\n",ERROR,strerror(errno));
		return;
		}
	unlink(pathname);

	/* this is a crude method of copying the file description */
	snprintf(buf, sizeof buf,
		"cat ./files/%s/filedir |grep %s >>./files/%s/filedir",
		CC->quickroom.QRdirname,
		filename,
		qrbuf.QRdirname);
	system(buf);
	cprintf("%d File '%s' has been moved.\n",OK,filename);
	}


/*
 * send a file over the net
 */
void cmd_netf(char *cmdbuf)
{
	char pathname[256],filename[256],destsys[256],buf[256],outfile[256];
	int a,e;
	time_t now;
	FILE *ofp;

	extract(filename,cmdbuf,0);
	extract(destsys,cmdbuf,1);

	if (CtdlAccessCheck(ac_room_aide)) return;

	if ((CC->quickroom.QRflags & QR_DIRECTORY) == 0) {
		cprintf("%d No directory in this room.\n",ERROR+NOT_HERE);
		return;
		}

	if (strlen(filename)==0) {
		cprintf("%d You must specify a file name.\n",
			ERROR+FILE_NOT_FOUND);
		return;
		}

	for (a=0; a<strlen(filename); ++a)
		if (filename[a]=='/') filename[a] = '_';
	snprintf(pathname,sizeof pathname,"./files/%s/%s",
		 CC->quickroom.QRdirname,filename);
	if (access(pathname,0)!=0) {
		cprintf("%d File '%s' not found.\n",
			ERROR+FILE_NOT_FOUND,pathname);
		return;
		}
	snprintf(buf,sizeof buf,"sysop@%s",destsys);
	e=alias(buf);
	if (e!=MES_BINARY) {
		cprintf("%d No such system: '%s'\n",
			ERROR+NO_SUCH_SYSTEM,destsys);
		return;
		}
	snprintf(outfile,sizeof outfile,"%s/network/spoolin/nsf.%d",BBSDIR,
		 getpid());
	ofp=fopen(outfile,"a");
	if (ofp==NULL) {
		cprintf("%d internal error\n",ERROR);
		return;
		}

	putc(255,ofp);
	putc(MES_NORMAL,ofp);
	putc(0,ofp);
	fprintf(ofp,"Pcit%ld",CC->usersupp.usernum); putc(0,ofp);
	time(&now);
	fprintf(ofp,"T%ld",(long)now); putc(0,ofp);
	fprintf(ofp,"A%s",CC->usersupp.fullname); putc(0,ofp);
	fprintf(ofp,"O%s",CC->quickroom.QRname); putc(0,ofp);
	fprintf(ofp,"N%s",NODENAME); putc(0,ofp);
	fprintf(ofp,"D%s",destsys); putc(0,ofp);
	fprintf(ofp,"SFILE"); putc(0,ofp);
	putc('M',ofp);
	fclose(ofp);

	snprintf(buf,sizeof buf,
		"cd ./files/%s; uuencode %s <%s 2>/dev/null >>%s",
		CC->quickroom.QRdirname,filename,filename,outfile);
	system(buf);

	ofp = fopen(outfile,"a");
	putc(0,ofp);
	fclose(ofp);

	cprintf("%d File '%s' has been sent to %s.\n",OK,filename,destsys);
	system("nohup ./netproc -i >/dev/null 2>&1 &");
	return;
	}

/*
 * This code is common to all commands which open a file for downloading.
 * It examines the file and displays the OK result code and some information
 * about the file.  NOTE: this stuff is Unix dependent.
 */
void OpenCmdResult(char *filename, char *mime_type) {
	struct stat statbuf;
	time_t modtime;
	long filesize;

	fstat(fileno(CC->download_fp), &statbuf);
	filesize = (long) statbuf.st_size;
	modtime = (time_t) statbuf.st_mtime;

	cprintf("%d %ld|%ld|%s|%s\n",
		OK, filesize, modtime, filename, mime_type);
}


/*
 * open a file for downloading
 */
void cmd_open(char *cmdbuf)
{
	char filename[256];
	char pathname[256];
	int a;

	extract(filename,cmdbuf,0);

	if (CtdlAccessCheck(ac_logged_in)) return;

	if ((CC->quickroom.QRflags & QR_DIRECTORY) == 0) {
		cprintf("%d No directory in this room.\n",ERROR+NOT_HERE);
		return;
		}

	if (strlen(filename)==0) {
		cprintf("%d You must specify a file name.\n",
			ERROR+FILE_NOT_FOUND);
		return;
		}

	if (CC->download_fp != NULL) {
		cprintf("%d You already have a download file open.\n",ERROR);
		return;
		}

	for (a=0; a<strlen(filename); ++a)
		if (filename[a]=='/') filename[a] = '_';

	snprintf(pathname,sizeof pathname,
		 "./files/%s/%s",CC->quickroom.QRdirname,filename);
	CC->download_fp = fopen(pathname,"r");

	if (CC->download_fp==NULL) {
		cprintf("%d cannot open %s: %s\n",
			ERROR,pathname,strerror(errno));
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
	char pathname[256];
	struct usersupp usbuf;
	char which_user[32];
	int which_floor;
	int a;

	extract(filename,cmdbuf,0);

	if (strlen(filename)==0) {
		cprintf("%d You must specify a file name.\n",
			ERROR+FILE_NOT_FOUND);
		return;
		}

	if (CC->download_fp != NULL) {
		cprintf("%d You already have a download file open.\n",ERROR);
		return;
		}

	if (!strcasecmp(filename, "_userpic_")) {
		extract(which_user, cmdbuf, 1);
		if (getuser(&usbuf, which_user) != 0) {
			cprintf("%d No such user.\n", ERROR+NO_SUCH_USER);
			return;
			}
		snprintf(pathname, sizeof pathname, "./userpics/%ld.gif",
			 usbuf.usernum);
		}
	else if (!strcasecmp(filename, "_floorpic_")) {
		which_floor = extract_int(cmdbuf, 1);
		snprintf(pathname, sizeof pathname, "./images/floor.%d.gif",
			 which_floor);
		}
	else if (!strcasecmp(filename, "_roompic_")) {
		assoc_file_name(pathname, &CC->quickroom, "images");
		}
	else {
		for (a=0; a<strlen(filename); ++a) {
			filename[a] = tolower(filename[a]);
			if (filename[a]=='/') filename[a] = '_';
			}
		snprintf(pathname,sizeof pathname,"./images/%s.gif",filename);
		}
	
	CC->download_fp = fopen(pathname,"r");
	if (CC->download_fp == NULL) {
		cprintf("%d Cannot open %s: %s\n",
			ERROR+FILE_NOT_FOUND,pathname,strerror(errno));
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

	extract(CC->upl_file,cmdbuf,0);
	extract(CC->upl_comment,cmdbuf,1);

	if (CtdlAccessCheck(ac_logged_in)) return;

	if ((CC->quickroom.QRflags & QR_DIRECTORY) == 0) {
		cprintf("%d No directory in this room.\n",ERROR+NOT_HERE);
		return;
		}

	if (strlen(CC->upl_file)==0) {
		cprintf("%d You must specify a file name.\n",
			ERROR+FILE_NOT_FOUND);
		return;
		}

	if (CC->upload_fp != NULL) {
		cprintf("%d You already have a upload file open.\n",ERROR);
		return;
		}

	for (a=0; a<strlen(CC->upl_file); ++a)
		if (CC->upl_file[a]=='/') CC->upl_file[a] = '_';
	snprintf(CC->upl_path,sizeof CC->upl_path,"./files/%s/%s",
		 CC->quickroom.QRdirname,CC->upl_file);
	snprintf(CC->upl_filedir,sizeof CC->upl_filedir,"./files/%s/filedir",
		 CC->quickroom.QRdirname);
	
	CC->upload_fp = fopen(CC->upl_path,"r");
	if (CC->upload_fp != NULL) {
		fclose(CC->upload_fp);
		CC->upload_fp = NULL;
		cprintf("%d '%s' already exists\n",
			ERROR+ALREADY_EXISTS,CC->upl_path);
		return;
		}

	CC->upload_fp = fopen(CC->upl_path,"wb");
	if (CC->upload_fp == NULL) {
		cprintf("%d Cannot open %s: %s\n",
			ERROR,CC->upl_path,strerror(errno));
		return;
		}
	cprintf("%d Ok\n",OK);
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
		cprintf("%d Usage error.\n", ERROR);
		return;
		}

	is_this_for_real = extract_int(cmdbuf,0);	
	extract(basenm, cmdbuf, 1);
	if (CC->upload_fp != NULL) {
		cprintf("%d You already have an upload file open.\n", ERROR);
		return;
		}

	strcpy(CC->upl_path, "");

	for (a=0; a<strlen(basenm); ++a) {
		basenm[a] = tolower(basenm[a]);
		if (basenm[a]=='/') basenm[a] = '_';
		}

	if (CC->usersupp.axlevel >= 6) {
		snprintf(CC->upl_path, sizeof CC->upl_path, "./images/%s",
			 basenm);
		}

	if (!strcasecmp(basenm, "_userpic_")) {
		snprintf(CC->upl_path, sizeof CC->upl_path,
			 "./userpics/%ld.gif", CC->usersupp.usernum);
		}

	if ( (!strcasecmp(basenm, "_floorpic_")) && (CC->usersupp.axlevel >= 6) ) {
		which_floor = extract_int(cmdbuf, 2);
		snprintf(CC->upl_path, sizeof CC->upl_path,
			 "./images/floor.%d.gif", which_floor);
		}

	if ( (!strcasecmp(basenm, "_roompic_")) && (is_room_aide()) ) {
		assoc_file_name(CC->upl_path, &CC->quickroom, "images");
		}

	if (strlen(CC->upl_path) == 0) {
		cprintf("%d Higher access required.\n",
			ERROR+HIGHER_ACCESS_REQUIRED);
		return;
		}

	if (is_this_for_real == 0) {
		cprintf("%d Ok to send image\n", OK);
		return;
		}

	CC->upload_fp = fopen(CC->upl_path,"wb");
	if (CC->upload_fp == NULL) {
		cprintf("%d Cannot open %s: %s\n",
			ERROR,CC->upl_path,strerror(errno));
		return;
		}
	cprintf("%d Ok\n",OK);
	CC->upload_type = UPL_IMAGE;
	}


/*
 * close the download file
 */
void cmd_clos(void) {
	char buf[256];
	
	if (CC->download_fp == NULL) {
		cprintf("%d You don't have a download file open.\n",ERROR);
		return;
		}

	fclose(CC->download_fp);
	CC->download_fp = NULL;

	if (CC->dl_is_net == 1) {
		CC->dl_is_net = 0;
		snprintf(buf,sizeof buf,"%s/network/spoolout/%s",BBSDIR,
			 CC->net_node);
		unlink(buf);
		}

	cprintf("%d Ok\n",OK);
	}


/*
 * abort and upload
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
		cprintf("%d You don't have an upload file open.\n",ERROR);
		return;
	}

	fclose(CC->upload_fp);
	CC->upload_fp = NULL;

	if ((!strcasecmp(cmd,"1")) && (CC->upload_type != UPL_FILE)) {
		CC->upload_type = UPL_FILE;
		cprintf("%d Upload completed.\n", OK);

		if (CC->upload_type == UPL_NET) {
			if (fork()==0) {
				execlp("./netproc", "netproc", "-i", NULL);
				exit(errno);
			}
		}

		return;
	}

	if (!strcasecmp(cmd,"1")) {
		cprintf("%d File '%s' saved.\n",OK,CC->upl_path);
		fp = fopen(CC->upl_filedir,"a");
		if (fp==NULL) fp=fopen(CC->upl_filedir,"w");
		if (fp!=NULL) {
			fprintf(fp,"%s %s\n",CC->upl_file,CC->upl_comment);
			fclose(fp);
		}

		/* put together an upload notice */
		sprintf(upload_notice,
			"NEW UPLOAD: '%s'\n %s\n",
			CC->upl_file,CC->upl_comment);
		quickie_message(CC->curr_user, NULL, CC->quickroom.QRname,
				upload_notice);
	}
	else {
		abort_upl(CC);
		cprintf("%d File '%s' aborted.\n",OK,CC->upl_path);
	}
}



/*
 * read from the download file
 */
void cmd_read(char *cmdbuf)
{
	long start_pos;
	int bytes;
	char buf[4096];

	start_pos = extract_long(cmdbuf,0);
	bytes = extract_int(cmdbuf,1);
	
	if (CC->download_fp == NULL) {
		cprintf("%d You don't have a download file open.\n",ERROR);
		return;
		}

	if (bytes > 4096) {
		cprintf("%d You may not read more than 4096 bytes.\n",ERROR);
		return;
		}

	fseek(CC->download_fp,start_pos,0);
	fread(buf,bytes,1,CC->download_fp);
	cprintf("%d %d\n",BINARY_FOLLOWS,bytes);
	client_write(buf, bytes);
	}



/*
 * write to the upload file
 */
void cmd_writ(char *cmdbuf)
{
	int bytes;
	char buf[4096];

	bytes = extract_int(cmdbuf,0);
	
	if (CC->upload_fp == NULL) {
		cprintf("%d You don't have an upload file open.\n",ERROR);
		return;
		}

	if (bytes > 4096) {
		cprintf("%d You may not write more than 4096 bytes.\n",ERROR);
		return;
		}

	cprintf("%d %d\n",SEND_BINARY,bytes);
	client_read(buf, bytes);
	fwrite(buf,bytes,1,CC->upload_fp);
	}



/*
 * cmd_netp() - identify as network poll session
 */
void cmd_netp(char *cmdbuf)
{
	char buf[256];
	
	extract(buf,cmdbuf,1);
	if (strcasecmp(buf,config.c_net_password)) {
		cprintf("%d authentication failed\n",ERROR);
		return;
		}
	extract(CC->net_node,cmdbuf,0);
	cprintf("%d authenticated as network node '%s'\n",OK,CC->net_node);
	}

/*
 * cmd_ndop() - open a network spool file for downloading
 */
void cmd_ndop(char *cmdbuf)
{
	char pathname[256];
	struct stat statbuf;

	if (strlen(CC->net_node)==0) {
		cprintf("%d Not authenticated as a network node.\n",
			ERROR+NOT_LOGGED_IN);
		return;
		}

	if (CC->download_fp != NULL) {
		cprintf("%d You already have a download file open.\n",ERROR);
		return;
		}

	snprintf(pathname,sizeof pathname,"%s/network/spoolout/%s",BBSDIR,
		 CC->net_node);

	/* first open the file in append mode in order to create a
	 * zero-length file if it doesn't already exist 
	 */
	CC->download_fp = fopen(pathname,"a");
	if (CC->download_fp != NULL) fclose(CC->download_fp);

	/* now open it */
	CC->download_fp = fopen(pathname,"r");
	if (CC->download_fp==NULL) {
		cprintf("%d cannot open %s: %s\n",
			ERROR,pathname,strerror(errno));
		return;
		}


	/* set this flag so other routines know that the download file
	 * currently open is a network spool file 
	 */
	CC->dl_is_net = 1;

	stat(pathname,&statbuf);
	cprintf("%d %ld\n",OK,statbuf.st_size);
	}

/*
 * cmd_nuop() - open a network spool file for uploading
 */
void cmd_nuop(char *cmdbuf)
{
	if (strlen(CC->net_node)==0) {
		cprintf("%d Not authenticated as a network node.\n",
			ERROR+NOT_LOGGED_IN);
		return;
		}

	if (CC->upload_fp != NULL) {
		cprintf("%d You already have an upload file open.\n",ERROR);
		return;
		}

	snprintf(CC->upl_path,sizeof CC->upl_path,"%s/network/spoolin/%s.%d",
		BBSDIR,CC->net_node,getpid());

	CC->upload_fp = fopen(CC->upl_path,"r");
	if (CC->upload_fp != NULL) {
		fclose(CC->upload_fp);
		CC->upload_fp = NULL;
		cprintf("%d '%s' already exists\n",
			ERROR+ALREADY_EXISTS,CC->upl_path);
		return;
		}

	CC->upload_fp = fopen(CC->upl_path,"w");
	if (CC->upload_fp == NULL) {
		cprintf("%d Cannot open %s: %s\n",
			ERROR,CC->upl_path,strerror(errno));
		return;
		}

	CC->upload_type = UPL_NET;
	cprintf("%d Ok\n",OK);
	}
