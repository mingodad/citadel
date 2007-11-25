/* $Id$ */

#ifndef SERV_EXTENSIONS_H
#define SERV_EXTENSIONS_H

#include "server.h"

/*
 * This is where we declare all of the server extensions we have.
 * We'll probably start moving these to a more sane location in the near
 * future.  For now, this just shuts up the compiler.
 */
//void serv_calendar_destroy(void);
//char *serv_test_init(void);
//char *serv_postfix_tcpdict(void);
/*
 */

 
 
/*
 * Structure defentitions for hook tables
 */
 

struct LogFunctionHook {
	struct LogFunctionHook *next;
	int loglevel;
	void (*h_function_pointer) (char *);
};
extern struct LogFunctionHook *LogHookTable;

struct CleanupFunctionHook {
	struct CleanupFunctionHook *next;
	void (*h_function_pointer) (void);
};
extern struct CleanupFunctionHook *CleanupHookTable;

struct FixedOutputHook {
	struct FixedOutputHook *next;
	char content_type[64];
	void (*h_function_pointer) (char *, int);
};
extern struct FixedOutputHook *FixedOutputTable;



/*
 * SessionFunctionHook extensions are used for any type of hook for which
 * the context in which it's being called (which is determined by the event
 * type) will make it obvious for the hook function to know where to look for
 * pertinent data.
 */
struct SessionFunctionHook {
	struct SessionFunctionHook *next;
	void (*h_function_pointer) (void);
	int eventtype;
};
extern struct SessionFunctionHook *SessionHookTable;


/*
 * UserFunctionHook extensions are used for any type of hook which implements
 * an operation on a user or username (potentially) other than the one
 * operating the current session.
 */
struct UserFunctionHook {
	struct UserFunctionHook *next;
	void (*h_function_pointer) (struct ctdluser *usbuf);
	int eventtype;
};
extern struct UserFunctionHook *UserHookTable;

/*
 * MessageFunctionHook extensions are used for hooks which implement handlers
 * for various types of message operations (save, read, etc.)
 */
struct MessageFunctionHook {
	struct MessageFunctionHook *next;
	int (*h_function_pointer) (struct CtdlMessage *msg);
	int eventtype;
};
extern struct MessageFunctionHook *MessageHookTable;


/*
 * NetprocFunctionHook extensions are used for hooks which implement handlers
 * for incoming network messages.
 */
struct NetprocFunctionHook {
	struct NetprocFunctionHook *next;
	int (*h_function_pointer) (struct CtdlMessage *msg, char *target_room);
};
extern struct NetprocFunctionHook *NetprocHookTable;


/*
 * DeleteFunctionHook extensions are used for hooks which get called when a
 * message is about to be deleted.
 */
struct DeleteFunctionHook {
	struct DeleteFunctionHook *next;
	void (*h_function_pointer) (char *target_room, long msgnum);
};
extern struct DeleteFunctionHook *DeleteHookTable;


/*
 * ExpressMessageFunctionHook extensions are used for hooks which implement
 * the sending of an instant message through various channels.  Any function
 * registered should return the number of recipients to whom the message was
 * successfully transmitted.
 */
struct XmsgFunctionHook {
	struct XmsgFunctionHook *next;
	int (*h_function_pointer) (char *, char *, char *);
	int order;
};
extern struct XmsgFunctionHook *XmsgHookTable;




/*
 * ServiceFunctionHook extensions are used for hooks which implement various
 * protocols (either on TCP or on unix domain sockets) directly in the Citadel server.
 */
struct ServiceFunctionHook {
	struct ServiceFunctionHook *next;
	int tcp_port;
	char *sockpath;
	void (*h_greeting_function) (void) ;
	void (*h_command_function) (void) ;
	void (*h_async_function) (void) ;
	int msock;
	const char* ServiceName; /* this is just for debugging and logging purposes. */
};
extern struct ServiceFunctionHook *ServiceHookTable;


/*
 * RoomFunctionHook extensions are used for hooks which impliment room
 * processing functions when new messages are added EG. SIEVE.
 */
struct RoomFunctionHook {
	struct RoomFunctionHook *next;
	int (*fcn_ptr) (struct ctdlroom *);
};
extern struct RoomFunctionHook *RoomHookTable;



struct SearchFunctionHook {
	struct SearchFunctionHook *next;
	void (*fcn_ptr) (int *, long **, char *);
	char *name;
};
extern struct SearchFunctionHook *SearchFunctionHookTable;

 
 
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
