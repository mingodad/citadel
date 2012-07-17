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

void do_internet_configuration(CtdlIPC *ipc);
void do_ignet_configuration(CtdlIPC *ipc);
void network_config_management(CtdlIPC *ipc, char *entrytype, char *comment);
void do_filterlist_configuration(CtdlIPC *ipc);
void do_pop3client_configuration(CtdlIPC *ipc);
void do_rssclient_configuration(CtdlIPC *ipc);
void do_system_configuration(CtdlIPC *ipc);

extern char editor_path[PATH_MAX];
extern int enable_status_line;
