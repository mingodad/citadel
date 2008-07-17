/* $Id$ */

#ifndef CTDL_MODULE_H
#define CTDL_MODULE_H

#include <libcitadel.h>
#include "server.h"
#include "sysdep_decls.h"
#include "msgbase.h"
#include "threads.h"
/*
 * define macros for module init stuff
 */
 
#define CTDL_MODULE_INIT(module_name) char *ctdl_module_##module_name##_init (int threading)

#define CTDL_INIT_CALL(module_name) ctdl_module_##module_name##_init (threading)

#define CTDL_MODULE_UPGRADE(module_name) char *ctdl_module_##module_name##_upgrade (void)

#define CTDL_UPGRADE_CALL(module_name) ctdl_module_##module_name##_upgrade ()


/*
 * Prototype for making log entries in Citadel.
 */

void CtdlLogPrintf(enum LogLevel loglevel, const char *format, ...);

/*
 * Fix the interface to aide_message so that it complies with the Coding style
 */
 
#define CtdlAideMessage(TEXT, SUBJECT) aide_message(TEXT, SUBJECT)

/*
 * Hook functions available to modules.
 */

void CtdlRegisterSessionHook(void (*fcn_ptr)(void), int EventType);
void CtdlUnregisterSessionHook(void (*fcn_ptr)(void), int EventType);

void CtdlRegisterUserHook(void (*fcn_ptr)(struct ctdluser *), int EventType);
void CtdlUnregisterUserHook(void (*fcn_ptr)(struct ctdluser *), int EventType);

void CtdlRegisterXmsgHook(int (*fcn_ptr)(char *, char *, char *, char *), int order);
void CtdlUnregisterXmsgHook(int (*fcn_ptr)(char *, char *, char *, char *), int order);

void CtdlRegisterMessageHook(int (*handler)(struct CtdlMessage *),
							int EventType);
void CtdlUnregisterMessageHook(int (*handler)(struct CtdlMessage *),
							int EventType);

void CtdlRegisterNetprocHook(int (*handler)(struct CtdlMessage *, char *) );
void CtdlUnregisterNetprocHook(int (*handler)(struct CtdlMessage *, char *) );

void CtdlRegisterRoomHook(int (*fcn_ptr)(struct ctdlroom *) );
void CtdlUnregisterRoomHook(int (*fnc_ptr)(struct ctdlroom *) );

void CtdlRegisterDeleteHook(void (*handler)(char *, long) );
void CtdlUnregisterDeleteHook(void (*handler)(char *, long) );

void CtdlRegisterCleanupHook(void (*fcn_ptr)(void));
void CtdlUnregisterCleanupHook(void (*fcn_ptr)(void));

void CtdlRegisterProtoHook(void (*handler)(char *), char *cmd, char *desc);
void CtdlUnregisterProtoHook(void (*handler)(char *), char *cmd);

void CtdlRegisterServiceHook(int tcp_port,
			     char *sockpath,
			     void (*h_greeting_function) (void),
			     void (*h_command_function) (void),
			     void (*h_async_function) (void),
			     const char *ServiceName
);
void CtdlUnregisterServiceHook(int tcp_port,
			char *sockpath,
                        void (*h_greeting_function) (void),
                        void (*h_command_function) (void),
                        void (*h_async_function) (void)
);

void CtdlRegisterFixedOutputHook(char *content_type,
			void (*output_function) (char *supplied_data, int len)
);
void CtdlUnRegisterFixedOutputHook(char *content_type);

void CtdlRegisterMaintenanceThread(char *name, void *(*thread_proc) (void *arg));

void CtdlRegisterSearchFuncHook(void (*fcn_ptr)(int *, long **, char *), char *name);


/*
 * Directory services hooks for LDAP etc
 */

#define DIRECTORY_USER_DEL 1	// Delete a user entry
#define DIRECTORY_CREATE_HOST 2	// Create a host entry if not already there.
#define DIRECTORY_CREATE_OBJECT 3	// Create a new object for directory entry
#define DIRECTORY_ATTRIB_ADD 4	// Add an attribute to the directory entry object
#define DIRECTORY_SAVE_OBJECT 5	// Save the object to the directory service
#define DIRECTORY_FREE_OBJECT 6	// Free the object and its attributes

int CtdlRegisterDirectoryServiceFunc(int (*func)(char *cn, char *ou, void **object), int cmd, char *module);
int CtdlDoDirectoryServiceFunc(char *cn, char *ou, void **object, char *module, int cmd);

/* TODODRW: This needs to be changed into a hook type interface
 * for now we have this horrible hack
 */
void CtdlModuleStartCryptoMsgs(char *ok_response, char *nosup_response, char *error_response);


/*
 * Citadel Threads API
 */
struct CtdlThreadNode *CtdlThreadCreate(char *name, long flags, void *(*thread_func) (void *arg), void *args);
struct CtdlThreadNode *CtdlThreadSchedule(char *name, long flags, void *(*thread_func) (void *arg), void *args, time_t when);
void CtdlThreadSleep(int secs);
void CtdlThreadStop(struct CtdlThreadNode *thread);
int CtdlThreadCheckStop(void);
/* void CtdlThreadCancel2(struct CtdlThreadNode *thread); Leave this out, it should never be needed */
const char *CtdlThreadName(const char *name);
struct CtdlThreadNode *CtdlThreadSelf(void);
int CtdlThreadGetCount(void);
int CtdlThreadGetWorkers(void);
double CtdlThreadGetWorkerAvg(void);
double CtdlThreadGetLoadAvg(void);
void CtdlThreadGC(void);
void CtdlThreadStopAll(void);
int CtdlThreadSelect(int n, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
void CtdlThreadAllocTSD(void);

#define CTDLTHREAD_BIGSTACK	0x0001
#define CTDLTHREAD_WORKER	0x0002

/* Macros to speed up getting outr thread */

#define MYCURSORS	(((ThreadTSD*)pthread_getspecific(ThreadKey))->cursors)
#define MYTID		(((ThreadTSD*)pthread_getspecific(ThreadKey))->tid)
#define CT		(((ThreadTSD*)pthread_getspecific(ThreadKey))->self)

/** return the current context list as an array and do it in a safe manner
 * The returned data is a copy so only reading is useful
 * The number of contexts is returned in count.
 * Beware, this does not copy any of the data pointed to by the context.
 * This means that you can not rely on things like the redirect buffer being valid.
 * You must free the returned pointer when done.
 */
struct CitContext *CtdlGetContextArray (int *count);
void CtdlFillSystemContext(struct CitContext *context, char *name);



/*
 * CtdlGetCurrentMessageNumber()  -  Obtain the current highest message number in the system
 * This provides a quick way to initialise a variable that might be used to indicate
 * messages that should not be processed. EG. a new Sieve script will use this
 * to record determine that messages older than this should not be processed.
 * This function is defined in control.c
 */
long CtdlGetCurrentMessageNumber(void);

#endif /* CTDL_MODULE_H */
