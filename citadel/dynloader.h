/* $Id$ */

#ifndef DYNLOADER_H
#define DYNLOADER_H

#include "server.h"

void DLoader_Init(char *pathname);
int DLoader_Exec_Cmd(char *cmdbuf);
char *Dynamic_Module_Init(void);

void CtdlRegisterLogHook(void (*fcn_ptr)(char *), int loglevel);
void CtdlUnregisterLogHook(void (*fcn_ptr)(char *), int loglevel);
void PerformLogHooks(int loglevel, char *logmsg);

void CtdlRegisterSessionHook(void (*fcn_ptr)(void), int EventType);
void CtdlUnregisterSessionHook(void (*fcn_ptr)(void), int EventType);
void PerformSessionHooks(int EventType);

void CtdlRegisterUserHook(void (*fcn_ptr)(char*, long), int EventType);
void CtdlUnregisterUserHook(void (*fcn_ptr)(char*, long), int EventType);
void PerformUserHooks(char *username, long usernum, int EventType);

void CtdlRegisterXmsgHook(int (*fcn_ptr)(char *, char *, char *), int order);
void CtdlUnregisterXmsgHook(int (*fcn_ptr)(char *, char *, char *), int order);
int PerformXmsgHooks(char *, char *, char *);


void CtdlRegisterMessageHook(int (*handler)(struct CtdlMessage *),
							int EventType);
void CtdlUnregisterMessageHook(int (*handler)(struct CtdlMessage *),
							int EventType);
int PerformMessageHooks(struct CtdlMessage *, int EventType);


void CtdlRegisterNetprocHook(int (*handler)(struct CtdlMessage *, char *) );
void CtdlUnregisterNetprocHook(int (*handler)(struct CtdlMessage *, char *) );
int PerformNetprocHooks(struct CtdlMessage *, char *);

void CtdlRegisterDeleteHook(int (*handler)(char *, long) );
void CtdlUnregisterDeleteHook(int (*handler)(char *, long) );
void PerformDeleteHooks(char *, long);


void CtdlRegisterCleanupHook(void (*fcn_ptr)(void));
void CtdlUnregisterCleanupHook(void (*fcn_ptr)(void));
void CtdlRegisterProtoHook(void (*handler)(char *), char *cmd, char *desc);
void CtdlUnregisterProtoHook(void (*handler)(char *), char *cmd);
void CtdlRegisterServiceHook(int tcp_port,
			char *sockpath,
                        void (*h_greeting_function) (void),
                        void (*h_command_function) (void) ) ;
void CtdlUnregisterServiceHook(int tcp_port,
			char *sockpath,
                        void (*h_greeting_function) (void),
                        void (*h_command_function) (void) ) ;

#endif /* DYNLOADER_H */
