/* $Id$ */
void cmd_delf (char *filename);
void cmd_movf (char *cmdbuf);
void cmd_netf (char *cmdbuf);
void OpenCmdResult (char *, char *);
void cmd_open (char *cmdbuf);
void cmd_oimg (char *cmdbuf);
void cmd_uopn (char *cmdbuf);
void cmd_uimg (char *cmdbuf);
void cmd_clos (void);
void abort_upl (struct CitContext *who);
void cmd_ucls (char *cmd);
void cmd_read (char *cmdbuf);
void cmd_writ (char *cmdbuf);
void cmd_netp (char *cmdbuf);
void cmd_ndop (char *cmdbuf);
void cmd_nuop (char *cmdbuf);
