/* $Id$ */

#ifndef CTDL_MODULE_H
#define CTDL_MODULE_H

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <pwd.h>
#include <errno.h>
#include <sys/types.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <sys/wait.h>
#include <string.h>
#include <limits.h>


#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif


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

#define CtdlAideMessage(TEXT, SUBJECT) quickie_message("Citadel",NULL,NULL,AIDEROOM,TEXT,FMT_CITADEL,SUBJECT) 

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

int CtdlTrySingleUser(void);
void CtdlEndSingleUser(void);
int CtdlWantSingleUser(void);
int CtdlIsSingleUser(void);


/*
 * CtdlGetCurrentMessageNumber()  -  Obtain the current highest message number in the system
 * This provides a quick way to initialise a variable that might be used to indicate
 * messages that should not be processed. EG. a new Sieve script will use this
 * to record determine that messages older than this should not be processed.
 * This function is defined in control.c
 */
long CtdlGetCurrentMessageNumber(void);



/*
 * Expose various room operation functions from room_ops.c to the modules API
 */

unsigned CtdlCreateRoom(char *new_room_name,
			int new_room_type,
			char *new_room_pass,
			int new_room_floor,
			int really_create,
			int avoid_access,
			int new_room_view);
int CtdlGetRoom(struct ctdlroom *qrbuf, char *room_name);
int CtdlGetRoomLock(struct ctdlroom *qrbuf, char *room_name);
int CtdlDoIHavePermissionToDeleteThisRoom(struct ctdlroom *qr);
void CtdlRoomAccess(struct ctdlroom *roombuf, struct ctdluser *userbuf,
		int *result, int *view);
void CtdlPutRoomLock(struct ctdlroom *qrbuf);
void CtdlForEachRoom(void (*CallBack)(struct ctdlroom *EachRoom, void *out_data),
	void *in_data);
void CtdlDeleteRoom(struct ctdlroom *qrbuf);
int CtdlRenameRoom(char *old_name, char *new_name, int new_floor);
void CtdlUserGoto (char *where, int display_result, int transiently,
			int *msgs, int *new);
struct floor *CtdlGetCachedFloor(int floor_num);
void CtdlScheduleRoomForDeletion(struct ctdlroom *qrbuf);
void CtdlGetFloor (struct floor *flbuf, int floor_num);
void CtdlPutFloor (struct floor *flbuf, int floor_num);
int CtdlIsNonEditable(struct ctdlroom *qrbuf);
void CtdlPutRoom(struct ctdlroom *);

/*
 * Possible return values for CtdlRenameRoom()
 */
enum {
	crr_ok,				/* success */
	crr_room_not_found,		/* room not found */
	crr_already_exists,		/* new name already exists */
	crr_noneditable,		/* cannot edit this room */
	crr_invalid_floor,		/* target floor does not exist */
	crr_access_denied		/* not allowed to edit this room */
};



/*
 * API declarations from citserver.h
 */
int CtdlAccessCheck(int);
/* 'required access level' values which may be passed to CtdlAccessCheck()
 */
enum {
	ac_none,
	ac_logged_in,
	ac_room_aide,
	ac_aide,
	ac_internal
};



/*
 * API declarations from serv_extensions.h
 */
void CtdlModuleDoSearch(int *num_msgs, long **search_msgs, char *search_string, char *func_name);
/* 
 * Global system configuration.  Don't change anything here.  It's all in dtds/config-defs.h now.
 */
struct config {
#include "datadefinitions.h"
#include "dtds/config-defs.h"
#include "undef_data.h"
};

extern struct config config;

#endif /* CTDL_MODULE_H */
