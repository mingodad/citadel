#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>


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

#include <errno.h>



#include "citadel.h"

/* our directories... */
char ctdl_home_directory[PATH_MAX] = "";
char ctdl_bio_dir[PATH_MAX]="bio";
char ctdl_bb_dir[PATH_MAX]="bitbucket";
char ctdl_data_dir[PATH_MAX]="data";
char ctdl_dspam_dir[PATH_MAX]="dspam";
char ctdl_file_dir[PATH_MAX]="files";
char ctdl_hlp_dir[PATH_MAX]="help";
char ctdl_image_dir[PATH_MAX]="images";
char ctdl_info_dir[PATH_MAX]="info";
char ctdl_key_dir[PATH_MAX]=SSL_DIR;
char ctdl_message_dir[PATH_MAX]="messages";
char ctdl_usrpic_dir[PATH_MAX]="userpics";
char ctdl_bbsbase_dir[PATH_MAX]="";
char ctdl_etc_dir[PATH_MAX]="";
/* attention! this may be non volatile on some oses */
char ctdl_run_dir[PATH_MAX]="";
char ctdl_spool_dir[PATH_MAX]="network";
char ctdl_netout_dir[PATH_MAX]="network/spoolout";
char ctdl_netin_dir[PATH_MAX]="network/spoolin";
char ctdl_netcfg_dir[PATH_MAX]="netconfigs";
char ctdl_utilbin_dir[PATH_MAX]="";
char ctdl_sbin_dir[PATH_MAX]="";
char ctdl_bin_dir[PATH_MAX]="";

/* some of our files, that are needed in several places */
char file_citadel_control[PATH_MAX]="";
char file_citadel_rc[PATH_MAX]="";
char file_citadel_config[PATH_MAX]="";
char file_lmtp_socket[PATH_MAX]="";
char file_lmtp_unfiltered_socket[PATH_MAX]="";
char file_arcq[PATH_MAX]="";
char file_citadel_socket[PATH_MAX]="";
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


void calc_dirs_n_files(int relh, int home, const char *relhome, const char  *ctdldir, int dbg)
{
	const char* basedir = "";
	char dirbuffer[PATH_MAX] = "";

	/*
	 * Ok, we keep our binaries either in the citadel base dir,
	 * or in /usr/sbin / /usr/bin
	 */
#ifdef HAVE_ETC_DIR
	snprintf(ctdl_sbin_dir, sizeof ctdl_sbin_dir, "/usr/sbin/");
	snprintf(ctdl_bin_dir, sizeof ctdl_bin_dir, "/usr/bin/");
#else
	snprintf(ctdl_sbin_dir, sizeof ctdl_sbin_dir, ctdldir);
	snprintf(ctdl_bin_dir, sizeof ctdl_bin_dir, ctdldir);
#endif

#ifndef HAVE_ETC_DIR
	basedir=ctdldir;
#else
	basedir=ETC_DIR;
#endif
	COMPUTE_DIRECTORY(ctdl_netcfg_dir);
	COMPUTE_DIRECTORY(ctdl_etc_dir);

#ifndef HAVE_UTILBIN_DIR
	basedir=ctdldir;
#else
	basedir=UTILBIN_DIR;
#endif
	COMPUTE_DIRECTORY(ctdl_utilbin_dir);

#ifndef HAVE_RUN_DIR
	basedir=ctdldir;
#else
	basedir=RUN_DIR;
#endif
	COMPUTE_DIRECTORY(ctdl_run_dir);

#ifndef HAVE_STATICDATA_DIR
	basedir=ctdldir;
#else
	basedir=STATICDATA_DIR;
#endif
	COMPUTE_DIRECTORY(ctdl_message_dir);

#ifndef HAVE_HELP_DIR
	basedir=ctdldir;
#else
	basedir=HELP_DIR;
#endif
	COMPUTE_DIRECTORY(ctdl_hlp_dir);

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
#ifndef HAVE_SPOOL_DIR
	basedir=ctdldir;
#else
	basedir=SPOOL_DIR;
#endif
	COMPUTE_DIRECTORY(ctdl_spool_dir);
	COMPUTE_DIRECTORY(ctdl_netout_dir);
	COMPUTE_DIRECTORY(ctdl_netin_dir);

	/* ok, now we know the dirs, calc some commonly used files */

	snprintf(file_arcq, 
			 sizeof file_arcq,
			 "%srefcount_adjustments.dat",
			 ctdl_etc_dir);

	snprintf(file_citadel_control, 
			 sizeof file_citadel_control,
			 "%scitadel.control",
			 ctdl_etc_dir
			 );

	snprintf(file_citadel_config, 
			 sizeof file_citadel_config,
			 "%scitadel.config",
			 ctdl_etc_dir);

	snprintf(file_citadel_rc, 
			 sizeof file_citadel_rc,
			 "%scitadel.rc",
			 ctdl_etc_dir);

	snprintf(file_lmtp_socket, 
			 sizeof file_lmtp_socket,
			 "%slmtp.socket",
			 ctdl_run_dir);

	snprintf(file_lmtp_unfiltered_socket, 
			 sizeof file_lmtp_socket,
			 "%slmtp-unfiltered.socket",
			 ctdl_run_dir);

	snprintf(file_citadel_socket, 
			 sizeof file_citadel_socket,
				"%scitadel.socket",
			 ctdl_run_dir);
	snprintf(file_pid_file, 
		 sizeof file_pid_file,
		 "%scitadel.pid",
		 ctdl_run_dir);
	snprintf(file_pid_paniclog, 
		 sizeof file_pid_paniclog, 
		 "%spanic.log",
		 ctdl_home_directory);
	snprintf(file_crpt_file_key,
		 sizeof file_crpt_file_key, 
		 "%s/citadel.key",
		 ctdl_key_dir);
	snprintf(file_crpt_file_csr,
		 sizeof file_crpt_file_csr, 
		 "%s/citadel.csr",
		 ctdl_key_dir);
	snprintf(file_crpt_file_cer,
		 sizeof file_crpt_file_cer, 
		 "%s/citadel.cer",
		 ctdl_key_dir);

	snprintf(file_chkpwd,
		 sizeof file_chkpwd, 
		 "%schkpwd",
		 ctdl_utilbin_dir);

	snprintf(file_base64,
		 sizeof file_base64,
		 "%sbase64",
		 ctdl_utilbin_dir);

	snprintf(file_guesstimezone,
		 sizeof file_guesstimezone,
		 "%sguesstimezone.sh",
		 ctdl_utilbin_dir);

	snprintf(file_dpsam_conf,
		 sizeof file_dpsam_conf,
		 "%sdspam.conf",
		 ctdl_etc_dir);
	snprintf(file_dspam_log, 
		 sizeof file_dspam_log, 
		 "%sdspam.log",
		 ctdl_home_directory);
	
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
                
        snprintf(file_funambol_msg,
                sizeof file_funambol_msg,
                "%sfunambol_newmail_soap.xml",
                ctdl_spool_dir);
        
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
	DBG_PRINT(ctdl_netcfg_dir);
	DBG_PRINT(ctdl_bbsbase_dir);
	DBG_PRINT(ctdl_sbin_dir);
	DBG_PRINT(ctdl_bin_dir);
	DBG_PRINT(ctdl_utilbin_dir);
	DBG_PRINT(file_citadel_control);
	DBG_PRINT(file_citadel_rc);
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
void assoc_file_name(char *buf, size_t n,
		     struct ctdlroom *qrbuf, const char *prefix)
{
	snprintf(buf, n, "%s%ld", prefix, qrbuf->QRnumber);
}

