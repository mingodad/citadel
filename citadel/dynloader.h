/* $Id$ */

void DLoader_Init(char *pathname);
int DLoader_Exec_Cmd(char *cmdbuf);
char *Dynamic_Module_Init(void);

void CtdlRegisterLogHook(void (*fcn_ptr)(char *), int loglevel);
void PerformLogHooks(int loglevel, char *logmsg);


void CtdlRegisterSessionHook(void (*fcn_ptr)(void), int EventType);
void PerformSessionHooks(int EventType);

void CtdlRegisterUserHook(void (*fcn_ptr)(char*, long), int EventType);
void PerformUserHooks(char *username, long usernum, int EventType);

void CtdlRegisterXmsgHook(int (*fcn_ptr)(char *, char *, char *), int order);
int PerformXmsgHooks(char *, char *, char *);

void CtdlRegisterMessageHook(int (*handler)(struct CtdlMessage *), int EventType);
int PerformMessageHooks(struct CtdlMessage *, int EventType);

void CtdlRegisterCleanupHook(void (*fcn_ptr)(void));
void CtdlRegisterProtoHook(void (*handler)(char *), char *cmd, char *desc);
void CtdlRegisterServiceHook(int tcp_port,
			char *sockpath,
                        void (*h_greeting_function) (void),
                        void (*h_command_function) (void) ) ;

