/* $Id$ */
void DLoader_Init(char *pathname);
int DLoader_Exec_Cmd(char *cmdbuf);
void CtdlRegisterLogHook(void (*fcn_ptr)(char *), int loglevel);
void CtdlRegisterCleanupHook(void (*fcn_ptr)(void));
void CtdlRegisterSessionHook(void (*fcn_ptr)(void), int EventType);
void PerformLogHooks(int loglevel, char *logmsg);
void PerformSessionHooks(int EventType);
void PerformUserHooks(char *username, long usernum, int EventType);
int PerformXmsgHooks(char *, char *, char *);
void CtdlRegisterProtoHook(void (*handler)(char *), char *cmd, char *desc);
void CtdlRegisterUserHook(void (*fcn_ptr)(char*, long), int EventType);
void CtdlRegisterXmsgHook(int (*fcn_ptr)(char *, char *, char *), int order);
void CtdlRegisterMessageHook(int (*handler)(struct CtdlMessage *), int EventType);
void CtdlRegisterServiceHook(int tcp_port,
                        void (*h_greeting_function) (void),
                        void (*h_command_function) (void) ) ;
int PerformMessageHooks(struct CtdlMessage *, int EventType);
char *Dynamic_Module_Init(void);
