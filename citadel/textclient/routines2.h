/* $Id$ */
void updatels(CtdlIPC *ipc);
void updatelsa(CtdlIPC *ipc);
void movefile(CtdlIPC *ipc);
void deletefile(CtdlIPC *ipc);
void netsendfile(CtdlIPC *ipc);
void entregis(CtdlIPC *ipc);
void subshell(void);
void upload(CtdlIPC *ipc, int c);
void cli_upload(CtdlIPC *ipc);
void validate(CtdlIPC *ipc);
void read_bio(CtdlIPC *ipc);
void cli_image_upload(CtdlIPC *ipc, char *keyname);
int room_prompt(unsigned int qrflags);
int val_user(CtdlIPC *ipc, char *user, int do_validate);
