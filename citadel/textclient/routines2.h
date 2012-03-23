/*
 * Copyright (c) 1987-2012 by the citadel.org team
 *
 *  This program is open source software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */
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
