
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

typedef void (*CtdlDbgFunction) (const int);

extern int DebugModules;
#define MDBGLOG(LEVEL) if ((LEVEL != LOG_DEBUG) || (DebugModules != 0))

#define MOD_syslog(LEVEL, FORMAT, ...)					\
	MDBGLOG(LEVEL) syslog(LEVEL,					\
			      "%s Modules: " FORMAT, IOSTR, __VA_ARGS__)

#define MODM_syslog(LEVEL, FORMAT)				\
	MDBGLOG(LEVEL) syslog(LEVEL,				\
			      "%s Modules: " FORMAT, IOSTR);




/*
 * ServiceFunctionHook extensions are used for hooks which implement various
 * protocols (either on TCP or on unix domain sockets) directly in the Citadel server.
 */
typedef struct ServiceFunctionHook ServiceFunctionHook;
struct ServiceFunctionHook {
	ServiceFunctionHook *next;
	int tcp_port;
	char *sockpath;
	void (*h_greeting_function) (void) ;
	void (*h_command_function) (void) ;
	void (*h_async_function) (void) ;
	int msock;
	const char* ServiceName; /* this is just for debugging and logging purposes. */
};
extern ServiceFunctionHook *ServiceHookTable;

typedef struct CleanupFunctionHook CleanupFunctionHook;
struct CleanupFunctionHook {
	CleanupFunctionHook *next;
	void (*h_function_pointer) (void);
};
extern CleanupFunctionHook *CleanupHookTable;


typedef struct __LogDebugEntry {
	CtdlDbgFunction F;
	const char *Name;
	long Len;
	const int *LogP;
} LogDebugEntry;
extern HashList *LogDebugEntryTable;
void initialize_server_extensions(void);
int DLoader_Exec_Cmd(char *cmdbuf);
char *Dynamic_Module_Init(void);

void CtdlDestroySessionHooks(void);
void PerformSessionHooks(int EventType);

int CheckTDAPVeto (int DBType, StrBuf *ErrMsg);
void CtdlDestroyTDAPVetoHooks(void);

void CtdlDestroyUserHooks(void);
void PerformUserHooks(struct ctdluser *usbuf, int EventType);

int PerformXmsgHooks(char *, char *, char *, char *);
void CtdlDestroyXmsgHooks(void);



void CtdlDestroyMessageHook(void);
int PerformMessageHooks(struct CtdlMessage *, recptypes *recps, int EventType);


void CtdlDestroyNetprocHooks(void);
int PerformNetprocHooks(struct CtdlMessage *, char *);

void CtdlDestroyRoomHooks(void);
int PerformRoomHooks(struct ctdlroom *);


void CtdlDestroyDeleteHooks(void);
void PerformDeleteHooks(char *, long);


void CtdlDestroyCleanupHooks(void);

void CtdlDestroyProtoHooks(void);

void CtdlDestroyServiceHook(void);

void CtdlDestroySearchHooks(void);

void CtdlDestroyFixedOutputHooks(void);
int PerformFixedOutputHooks(char *, char *, int);

void CtdlRegisterDebugFlagHook(const char *Name, long len, CtdlDbgFunction F, const int *);
void CtdlSetDebugLogFacilities(const char **Str, long n);
void CtdlDestroyDebugTable(void);

void netcfg_keyname(char *keybuf, long roomnum);

#endif /* SERV_EXTENSIONS_H */
