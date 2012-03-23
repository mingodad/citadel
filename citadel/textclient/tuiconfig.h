void do_internet_configuration(CtdlIPC *ipc);
void do_ignet_configuration(CtdlIPC *ipc);
void network_config_management(CtdlIPC *ipc, char *entrytype, char *comment);
void do_filterlist_configuration(CtdlIPC *ipc);
void do_pop3client_configuration(CtdlIPC *ipc);
void do_rssclient_configuration(CtdlIPC *ipc);
void do_system_configuration(CtdlIPC *ipc);

extern char editor_path[PATH_MAX];
