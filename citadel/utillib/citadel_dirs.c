/*
 * citadel_dirs.c : calculate pathnames for various files used in the Citadel system
 *
 * Copyright (c) 1987-2014 by the citadel.org team
 *
 *  This program is open source software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <errno.h>
#include <libcitadel.h>
#include "citadel.h"

/* our directories... */
char ctdl_home_directory[PATH_MAX] = "";
char ctdl_bio_dir[PATH_MAX]="bio";
char ctdl_bb_dir[PATH_MAX]="bitbucket";
char ctdl_data_dir[PATH_MAX]="data";
char ctdl_dspam_dir[PATH_MAX]="dspam";
char ctdl_file_dir[PATH_MAX]="files";
char ctdl_hlp_dir[PATH_MAX]="help";
char ctdl_shared_dir[PATH_MAX]="";
char ctdl_image_dir[PATH_MAX]="images";
char ctdl_info_dir[PATH_MAX]="info";
char ctdl_key_dir[PATH_MAX]=SSL_DIR;
char ctdl_message_dir[PATH_MAX]="messages";
char ctdl_usrpic_dir[PATH_MAX]="userpics";
char ctdl_bbsbase_dir[PATH_MAX]="";
char ctdl_etc_dir[PATH_MAX]="";
char ctdl_autoetc_dir[PATH_MAX]="";
/* attention! this may be non volatile on some oses */
char ctdl_run_dir[PATH_MAX]="";
char ctdl_spool_dir[PATH_MAX]="network";
char ctdl_netout_dir[PATH_MAX]="network/spoolout";
char ctdl_netin_dir[PATH_MAX]="network/spoolin";
char ctdl_netdigest_dir[PATH_MAX]="network/digest";
char ctdl_nettmp_dir[PATH_MAX]="network/spooltmp";
char ctdl_netcfg_dir[PATH_MAX]="netconfigs";
char ctdl_utilbin_dir[PATH_MAX]="";
char ctdl_sbin_dir[PATH_MAX]="";
char ctdl_bin_dir[PATH_MAX]="";

/* some of our files, that are needed in several places */
char file_citadel_control[PATH_MAX]="";
char file_citadel_config[PATH_MAX]="";
char file_citadel_urlshorteners[PATH_MAX]="";
char file_lmtp_socket[PATH_MAX]="";
char file_lmtp_unfiltered_socket[PATH_MAX]="";
char file_arcq[PATH_MAX]="";
char file_citadel_socket[PATH_MAX]="";
char file_citadel_admin_socket[PATH_MAX]="";
char file_mail_aliases[PATH_MAX]="";
char file_pid_file[PATH_MAX]="";
char file_pid_paniclog[PATH_MAX]="";
char file_crpt_file_key[PATH_MAX]="";
char file_crpt_file_csr[PATH_MAX]="";
char file_crpt_file_cer[PATH_MAX]="";
char file_chkpwd[PATH_MAX]="";
char file_base64[PATH_MAX]="";
char file_guesstimezone[PATH_MAX]="";
char file_funambol_msg[PATH_MAX] = "";
char file_dpsam_conf[PATH_MAX] = "";
char file_dspam_log[PATH_MAX] = "";





#define COMPUTE_DIRECTORY(SUBDIR) memcpy(dirbuffer,SUBDIR, sizeof dirbuffer);\
	snprintf(SUBDIR,sizeof SUBDIR,  "%s%s%s%s%s%s%s", \
			 (home&!relh)?ctdl_home_directory:basedir, \
             ((basedir!=ctdldir)&(home&!relh))?basedir:"/", \
             ((basedir!=ctdldir)&(home&!relh))?"/":"", \
			 relhome, \
             (relhome[0]!='\0')?"/":"",\
			 dirbuffer,\
			 (dirbuffer[0]!='\0')?"/":"");

#define DBG_PRINT(A) if (dbg==1) fprintf (stderr,"%s : %s \n", #A, A)


void calc_dirs_n_files(int relh, int home, const char *relhome, char  *ctdldir, int dbg)
{
	const char* basedir = "";
	char dirbuffer[PATH_MAX] = "";

	/*
	 * Ok, we keep our binaries either in the citadel base dir,
	 * or in /usr/sbin / /usr/bin
	 */
	StripSlashes(ctdldir, 1);
#ifdef HAVE_ETC_DIR
	snprintf(ctdl_sbin_dir, sizeof ctdl_sbin_dir, "/usr/sbin/");
	snprintf(ctdl_bin_dir, sizeof ctdl_bin_dir, "/usr/bin/");
#else
	snprintf(ctdl_sbin_dir, sizeof ctdl_sbin_dir, ctdldir);
	snprintf(ctdl_bin_dir, sizeof ctdl_bin_dir, ctdldir);
#endif
	StripSlashes(ctdl_sbin_dir, 1);
	StripSlashes(ctdl_bin_dir, 1);

#ifndef HAVE_AUTO_ETC_DIR
	basedir=ctdldir;
#else
	basedir=AUTO_ETC_DIR;
#endif
	COMPUTE_DIRECTORY(ctdl_autoetc_dir);
	StripSlashes(ctdl_autoetc_dir, 1);

#ifndef HAVE_ETC_DIR
	basedir=ctdldir;
#else
	basedir=ETC_DIR;
#endif
	COMPUTE_DIRECTORY(ctdl_netcfg_dir);
	COMPUTE_DIRECTORY(ctdl_etc_dir);
	StripSlashes(ctdl_netcfg_dir, 1);
	StripSlashes(ctdl_etc_dir, 1);

#ifndef HAVE_UTILBIN_DIR
	basedir=ctdldir;
#else
	basedir=UTILBIN_DIR;
#endif
	COMPUTE_DIRECTORY(ctdl_utilbin_dir);
	StripSlashes(ctdl_utilbin_dir, 1);

#ifndef HAVE_RUN_DIR
	basedir=ctdldir;
#else
	basedir=RUN_DIR;
#endif
	COMPUTE_DIRECTORY(ctdl_run_dir);
	StripSlashes(ctdl_run_dir, 1);

#ifndef HAVE_STATICDATA_DIR
	basedir=ctdldir;
#else
	basedir=STATICDATA_DIR;
#endif
	COMPUTE_DIRECTORY(ctdl_message_dir);
	StripSlashes(ctdl_message_dir, 1);

#ifndef HAVE_HELP_DIR
	basedir=ctdldir;
#else
	basedir=HELP_DIR;
#endif
	COMPUTE_DIRECTORY(ctdl_hlp_dir);
	StripSlashes(ctdl_hlp_dir, 1);
	COMPUTE_DIRECTORY(ctdl_shared_dir);
	StripSlashes(ctdl_shared_dir, 1);

#ifndef HAVE_DATA_DIR
	basedir=ctdldir;
#else
	basedir=DATA_DIR;
#endif
	COMPUTE_DIRECTORY(ctdl_bio_dir);
	COMPUTE_DIRECTORY(ctdl_bb_dir);
	COMPUTE_DIRECTORY(ctdl_data_dir);
	COMPUTE_DIRECTORY(ctdl_dspam_dir);
	COMPUTE_DIRECTORY(ctdl_file_dir);
	COMPUTE_DIRECTORY(ctdl_image_dir);
	COMPUTE_DIRECTORY(ctdl_info_dir);
	COMPUTE_DIRECTORY(ctdl_usrpic_dir);
	COMPUTE_DIRECTORY(ctdl_bbsbase_dir);

	StripSlashes(ctdl_bio_dir, 1);
	StripSlashes(ctdl_bb_dir, 1);
	StripSlashes(ctdl_data_dir, 1);
	StripSlashes(ctdl_dspam_dir, 1);
	StripSlashes(ctdl_file_dir, 1);
	StripSlashes(ctdl_image_dir, 1);
	StripSlashes(ctdl_info_dir, 1);
	StripSlashes(ctdl_usrpic_dir, 1);
	StripSlashes(ctdl_bbsbase_dir, 1);

#ifndef HAVE_SPOOL_DIR
	basedir=ctdldir;
#else
	basedir=SPOOL_DIR;
#endif
	COMPUTE_DIRECTORY(ctdl_spool_dir);
	COMPUTE_DIRECTORY(ctdl_netout_dir);
	COMPUTE_DIRECTORY(ctdl_netin_dir);
	COMPUTE_DIRECTORY(ctdl_netdigest_dir);
	COMPUTE_DIRECTORY(ctdl_nettmp_dir);

	StripSlashes(ctdl_spool_dir, 1);
	StripSlashes(ctdl_netout_dir, 1);
	StripSlashes(ctdl_netin_dir, 1);
	StripSlashes(ctdl_netdigest_dir, 1);
	StripSlashes(ctdl_nettmp_dir, 1);

	/* ok, now we know the dirs, calc some commonly used files */

	snprintf(file_arcq, 
			 sizeof file_arcq,
			 "%srefcount_adjustments.dat",
			 ctdl_autoetc_dir);
	StripSlashes(file_arcq, 0);
	snprintf(file_citadel_control, 
			 sizeof file_citadel_control,
			 "%scitadel.control",
			 ctdl_autoetc_dir
			 );
	StripSlashes(file_citadel_control, 0);
	snprintf(file_citadel_config, 
			 sizeof file_citadel_config,
			 "%scitadel.config",
			 ctdl_autoetc_dir);
	StripSlashes(file_citadel_config, 0);
	snprintf(file_citadel_urlshorteners, 
			 sizeof file_citadel_urlshorteners,
			 "%scitadel_urlshorteners.rc",
			 ctdl_etc_dir);
	StripSlashes(file_citadel_urlshorteners, 0);
	snprintf(file_lmtp_socket, 
			 sizeof file_lmtp_socket,
			 "%slmtp.socket",
			 ctdl_run_dir);
	StripSlashes(file_lmtp_socket, 0);
	snprintf(file_lmtp_unfiltered_socket, 
			 sizeof file_lmtp_socket,
			 "%slmtp-unfiltered.socket",
			 ctdl_run_dir);
	StripSlashes(file_lmtp_unfiltered_socket, 0);
	snprintf(file_citadel_socket, 
			 sizeof file_citadel_socket,
				"%scitadel.socket",
			 ctdl_run_dir);
	StripSlashes(file_citadel_socket, 0);
	snprintf(file_citadel_admin_socket, 
			 sizeof file_citadel_admin_socket,
				"%scitadel-admin.socket",
			 ctdl_run_dir);
	StripSlashes(file_citadel_admin_socket, 0);
	snprintf(file_pid_file, 
		 sizeof file_pid_file,
		 "%scitadel.pid",
		 ctdl_run_dir);
	StripSlashes(file_pid_file, 0);
	snprintf(file_pid_paniclog, 
		 sizeof file_pid_paniclog, 
		 "%spanic.log",
		 ctdl_home_directory);
	StripSlashes(file_pid_paniclog, 0);
	snprintf(file_crpt_file_key,
		 sizeof file_crpt_file_key, 
		 "%s/citadel.key",
		 ctdl_key_dir);
	StripSlashes(file_crpt_file_key, 0);
	snprintf(file_crpt_file_csr,
		 sizeof file_crpt_file_csr, 
		 "%s/citadel.csr",
		 ctdl_key_dir);
	StripSlashes(file_crpt_file_csr, 0);
	snprintf(file_crpt_file_cer,
		 sizeof file_crpt_file_cer, 
		 "%s/citadel.cer",
		 ctdl_key_dir);
	StripSlashes(file_crpt_file_cer, 0);
	snprintf(file_chkpwd,
		 sizeof file_chkpwd, 
		 "%schkpwd",
		 ctdl_utilbin_dir);
	StripSlashes(file_chkpwd, 0);
	snprintf(file_base64,
		 sizeof file_base64,
		 "%sbase64",
		 ctdl_utilbin_dir);
	StripSlashes(file_base64, 0);
	snprintf(file_guesstimezone,
		 sizeof file_guesstimezone,
		 "%sguesstimezone.sh",
		 ctdl_utilbin_dir);

	snprintf(file_dpsam_conf,
		 sizeof file_dpsam_conf,
		 "%sdspam.conf",
		 ctdl_etc_dir);
	StripSlashes(file_dpsam_conf, 0);
	snprintf(file_dspam_log, 
		 sizeof file_dspam_log, 
		 "%sdspam.log",
		 ctdl_home_directory);
	StripSlashes(file_dspam_log, 0);
	/* 
	 * DIRTY HACK FOLLOWS! due to configs in the network dir in the 
	 * legacy installations, we need to calculate ifdeffed here.
	 */
	snprintf(file_mail_aliases, 
		 sizeof file_mail_aliases,
		 "%smail.aliases",
#ifdef HAVE_ETC_DIR
		 ctdl_etc_dir
#else
		 ctdl_spool_dir
#endif
		);
	StripSlashes(file_mail_aliases, 0);
        snprintf(file_funambol_msg,
                sizeof file_funambol_msg,
                "%sfunambol_newmail_soap.xml",
                ctdl_shared_dir);
	StripSlashes(file_funambol_msg, 0);

	DBG_PRINT(ctdl_bio_dir);
	DBG_PRINT(ctdl_bb_dir);
	DBG_PRINT(ctdl_data_dir);
	DBG_PRINT(ctdl_dspam_dir);
	DBG_PRINT(ctdl_file_dir);
	DBG_PRINT(ctdl_hlp_dir);
	DBG_PRINT(ctdl_image_dir);
	DBG_PRINT(ctdl_info_dir);
	DBG_PRINT(ctdl_key_dir);
	DBG_PRINT(ctdl_message_dir);
	DBG_PRINT(ctdl_usrpic_dir);
	DBG_PRINT(ctdl_etc_dir);
	DBG_PRINT(ctdl_run_dir);
	DBG_PRINT(ctdl_spool_dir);
	DBG_PRINT(ctdl_netout_dir);
	DBG_PRINT(ctdl_netin_dir);
	DBG_PRINT(ctdl_netdigest_dir);
	DBG_PRINT(ctdl_nettmp_dir);
	DBG_PRINT(ctdl_netcfg_dir);
	DBG_PRINT(ctdl_bbsbase_dir);
	DBG_PRINT(ctdl_sbin_dir);
	DBG_PRINT(ctdl_bin_dir);
	DBG_PRINT(ctdl_utilbin_dir);
	DBG_PRINT(file_citadel_control);
	DBG_PRINT(file_citadel_config);
	DBG_PRINT(file_lmtp_socket);
	DBG_PRINT(file_lmtp_unfiltered_socket);
	DBG_PRINT(file_arcq);
	DBG_PRINT(file_citadel_socket);
	DBG_PRINT(file_mail_aliases);
	DBG_PRINT(file_pid_file);
	DBG_PRINT(file_pid_paniclog);
	DBG_PRINT(file_crpt_file_key);
	DBG_PRINT(file_crpt_file_csr);
	DBG_PRINT(file_crpt_file_cer);
	DBG_PRINT(file_chkpwd);
	DBG_PRINT(file_base64);
	DBG_PRINT(file_guesstimezone);
	DBG_PRINT(file_funambol_msg);
}


/*
 * Generate an associated file name for a room
 */
size_t assoc_file_name(char *buf, size_t n,
		     struct ctdlroom *qrbuf, const char *prefix)
{
	return snprintf(buf, n, "%s%ld", prefix, qrbuf->QRnumber);
}

void remove_digest_file(struct ctdlroom *room)
{
	char buf[PATH_MAX];

	snprintf(buf, PATH_MAX, "%s/%ld.eml", 
		 ctdl_netdigest_dir,
		 room->QRnumber);
	StripSlashes(buf, 0);
	fprintf(stderr, "----> %s \n", buf);
	unlink(buf);
}

FILE *create_digest_file(struct ctdlroom *room)
{
	char buf[PATH_MAX];
	FILE *fp;

	snprintf(buf, PATH_MAX, "%s/%ld.eml", 
		 ctdl_netdigest_dir,
		 room->QRnumber);
	StripSlashes(buf, 0);
	fprintf(stderr, "----> %s \n", buf);
	
	fp = fopen(buf, "w+");
	if (fp == NULL) {

	}
	return fp;
}


int create_dir(char *which, long ACCESS, long UID, long GID)
{
	int rv;
	rv = mkdir(which, ACCESS);
	if ((rv == -1) && (errno == EEXIST))
		return rv;
	rv = chmod(which, ACCESS);
	if (rv == -1)
		return rv;
	rv = chown(which, UID, GID);
	return rv;
}

int create_run_directories(long UID, long GID)
{
	int rv;

	rv = create_dir(ctdl_info_dir    , S_IRUSR|S_IWUSR|S_IXUSR, UID, -1);
	if (rv != -1)
		rv = create_dir(ctdl_bio_dir     , S_IRUSR|S_IWUSR|S_IXUSR, UID, -1);
	if (rv != -1)
		rv = create_dir(ctdl_usrpic_dir  , S_IRUSR|S_IWUSR|S_IXUSR, UID, -1);
	if (rv != -1)
		rv = create_dir(ctdl_message_dir , S_IRUSR|S_IWUSR|S_IXUSR, UID, -1);
	if (rv != -1)
		rv = create_dir(ctdl_hlp_dir     , S_IRUSR|S_IWUSR|S_IXUSR, UID, -1);
	if (rv != -1)
		rv = create_dir(ctdl_image_dir   , S_IRUSR|S_IWUSR|S_IXUSR, UID, -1);
	if (rv != -1)
		rv = create_dir(ctdl_bb_dir      , S_IRUSR|S_IWUSR|S_IXUSR, UID, -1);
	if (rv != -1)
		rv = create_dir(ctdl_file_dir    , S_IRUSR|S_IWUSR|S_IXUSR, UID, -1);
	if (rv != -1)
		rv = create_dir(ctdl_netcfg_dir  , S_IRUSR|S_IWUSR|S_IXUSR, UID, -1);
	if (rv != -1)
		rv = create_dir(ctdl_key_dir     , S_IRUSR|S_IWUSR|S_IXUSR, UID, -1);
	if (rv != -1)
		rv = create_dir(ctdl_run_dir     , S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH, UID, GID);
	return rv;
}
