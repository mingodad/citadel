#ifndef __CITADEL_DIRS_H
#define __CITADEL_DIRS_H

#include <limits.h>


extern char ctdl_home_directory[PATH_MAX];


/* all our directories */
extern char ctdl_bio_dir[PATH_MAX];
extern char ctdl_bb_dir[PATH_MAX];
extern char ctdl_data_dir[PATH_MAX];
extern char ctdl_dspam_dir[PATH_MAX];
extern char ctdl_file_dir[PATH_MAX];
extern char ctdl_hlp_dir[PATH_MAX];
extern char ctdl_shared_dir[PATH_MAX];
extern char ctdl_image_dir[PATH_MAX];
extern char ctdl_info_dir[PATH_MAX];
extern char ctdl_key_dir[PATH_MAX];
extern char ctdl_message_dir[PATH_MAX];
extern char ctdl_usrpic_dir[PATH_MAX];
extern char ctdl_etc_dir[PATH_MAX];
extern char ctdl_autoetc_dir[PATH_MAX];
extern char ctdl_run_dir[PATH_MAX];
extern char ctdl_spool_dir[PATH_MAX];
extern char ctdl_netout_dir[PATH_MAX];
extern char ctdl_netin_dir[PATH_MAX];
extern char ctdl_nettmp_dir[PATH_MAX];
extern char ctdl_netcfg_dir[PATH_MAX];
extern char ctdl_bbsbase_dir[PATH_MAX];
extern char ctdl_sbin_dir[PATH_MAX];
extern char ctdl_bin_dir[PATH_MAX];
extern char ctdl_utilbin_dir[PATH_MAX];



/* some of the frequently used files */
extern char file_citadel_control[PATH_MAX];
extern char file_citadel_config[PATH_MAX];
extern char file_citadel_urlshorteners[PATH_MAX];
extern char file_lmtp_socket[PATH_MAX];
extern char file_lmtp_unfiltered_socket[PATH_MAX];
extern char file_arcq[PATH_MAX];
extern char file_citadel_socket[PATH_MAX];
extern char file_citadel_admin_socket[PATH_MAX];
extern char file_mail_aliases[PATH_MAX];
extern char file_pid_file[PATH_MAX];
extern char file_pid_paniclog[PATH_MAX];
extern char file_crpt_file_key[PATH_MAX];
extern char file_crpt_file_csr[PATH_MAX];
extern char file_crpt_file_cer[PATH_MAX];
extern char file_chkpwd[PATH_MAX];
extern char file_base64[PATH_MAX];
extern char file_guesstimezone[PATH_MAX];
extern char file_dpsam_conf[PATH_MAX];
extern char file_dspam_log[PATH_MAX];

extern char file_funambol_msg[PATH_MAX];

extern void calc_dirs_n_files(int relh, int home, const char *relhome, char  *ctdldir, int dbg);


extern void create_run_directories(long UID, long GUID);

extern size_t assoc_file_name(char *buf, 
			    size_t n,
			    struct ctdlroom *qrbuf, 
			    const char *prefix);

#endif /* __CITADEL_DIRS_H */
