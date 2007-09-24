/* $Id: routines2.h 5148 2007-05-08 15:40:16Z ajc $ */
void do_internet_configuration(CtdlIPC *ipc);
void do_ignet_configuration(CtdlIPC *ipc);
void network_config_management(CtdlIPC *ipc, char *entrytype, char *comment);
void do_filterlist_configuration(CtdlIPC *ipc);
void do_pop3client_configuration(CtdlIPC *ipc);
void do_system_configuration(CtdlIPC *ipc);
