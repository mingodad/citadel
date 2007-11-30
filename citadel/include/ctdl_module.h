/* $Id$ */

#ifndef CTDL_MODULE_H
#define CTDL_MODULE_H

#include "server.h"
#include "sysdep_decls.h"
#include "msgbase.h"

/*
 * define macros for module init stuff
 */
 
#define CTDL_MODULE_INIT(module_name) char *ctdl_module_##module_name##_init (int threading)

#define CTDL_INIT_CALL(module_name) ctdl_module_##module_name##_init (threading)


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

void CtdlRegisterXmsgHook(int (*fcn_ptr)(char *, char *, char *), int order);
void CtdlUnregisterXmsgHook(int (*fcn_ptr)(char *, char *, char *), int order);

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
void CtdlThreadSleep(int secs);
void CtdlThreadStop(struct CtdlThreadNode *thread);
int CtdlThreadCheckStop(struct CtdlThreadNode *thread);
void CtdlThreadCancel(struct CtdlThreadNode *thread);
char *CtdlThreadName(struct CtdlThreadNode *thread, char *name);
struct CtdlThreadNode *CtdlThreadSelf(void);
int CtdlThreadGetCount(void);
void CtdlThreadGC(void);
void CtdlThreadStopAll(void);
int CtdlThreadSelect(int n, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, const struct timeval *timeout, struct CtdlThreadNode *self);

/* Macros to speed up getting outr thread */
#define CT _this_cit_thread
#define CT_PUSH() \
	struct CtdlThreadNode *_this_cit_thread;\
	_this_cit_thread = CtdlThreadSelf()


#ifdef WITH_THREADLOG
#define CtdlThreadPushName(NAME) \
	char *_push_name; \
	_push_name = CtdlThreadName(CtdlThreadSelf(), NAME)

#define CtdlThreadPopName() \
	free (CtdlThreadName(CtdlThreadSelf(), _push_name))
#else
#define CtdlThreadPushName(NAME)
#define CtdlThreadPopName()
#endif

#endif /* CTDL_MODULE_H */
