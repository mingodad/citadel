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

void edituser(CtdlIPC *ipc, int cmd);
void interr(int errnum);
int struncmp(char *lstr, char *rstr, int len);
int pattern(char *search, char *patn);
void enter_config(CtdlIPC* ipc, int mode);
void locate_host(CtdlIPC* ipc, char *hbuf);
void misc_server_cmd(CtdlIPC *ipc, char *cmd);
int nukedir(char *dirname);
void strproc(char *string);
void back(int spaces);
void progress(CtdlIPC* ipc, unsigned long curr, unsigned long cmax);
int set_attr(CtdlIPC *ipc, unsigned int sval, char *prompt, unsigned int sbit, int backwards);
