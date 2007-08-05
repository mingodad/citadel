/* $Id$ */

#ifndef SERV_EXTENSIONS_H
#define SERV_EXTENSIONS_H

#include "server.h"

/*
 * This is where we declare all of the server extensions we have.
 * We'll probably start moving these to a more sane location in the near
 * future.  For now, this just shuts up the compiler.
 */
void serv_calendar_destroy(void);
char *serv_test_init(void);
char *serv_postfix_tcpdict(void);
/*
 */

void initialize_server_extensions(void);
int DLoader_Exec_Cmd(char *cmdbuf);
char *Dynamic_Module_Init(void);

void CtdlDestroySessionHooks(void);
void PerformSessionHooks(int EventType);

void CtdlDestroyUserHooks(void);
void PerformUserHooks(struct ctdluser *usbuf, int EventType);

int PerformXmsgHooks(char *, char *, char *);
void CtdlDestroyXmsgHooks(void);



void CtdlDestroyMessageHook(void);
int PerformMessageHooks(struct CtdlMessage *, int EventType);


void CtdlDestroyNetprocHooks(void);
int PerformNetprocHooks(struct CtdlMessage *, char *);

void CtdlDestroyRoomHooks(void);
int PerformRoomHooks(struct ctdlroom *);


void CtdlDestroyDeleteHooks(void);
void PerformDeleteHooks(char *, long);


void CtdlDestroyCleanupHooks(void);

void CtdlDestroyProtoHooks(void);

void CtdlDestroyServiceHook(void);

void CtdlDestroyFixedOutputHooks(void);
int PerformFixedOutputHooks(char *, char *, int);

void CtdlModuleDoSearch(int *num_msgs, long **search_msgs, char *search_string, char *func_name);

#endif /* SERV_EXTENSIONS_H */
