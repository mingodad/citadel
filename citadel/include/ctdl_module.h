
#ifndef CTDL_MODULE_H
#define CTDL_MODULE_H

#include "sysdep.h"

#ifdef HAVE_GC
#define GC_THREADS
#define GC_REDIRECT_TO_LOCAL
#include <gc/gc_local_alloc.h>
#else
#define GC_MALLOC malloc
#define GC_MALLOC_ATOMIC malloc
#define GC_FREE free
#define GC_REALLOC realloc
#endif


#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <pwd.h>
#include <errno.h>
#include <syslog.h>
#include <sys/types.h>
#include <time.h>
#include <sys/wait.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <limits.h>

#include <libcitadel.h>

#include "server.h"
#include "sysdep_decls.h"
#include "msgbase.h"
#include "threads.h"
#include "citadel_dirs.h"
#include "context.h"

/*
 * define macros for module init stuff
 */
 
#define CTDL_MODULE_INIT(module_name) char *ctdl_module_##module_name##_init (int threading)

#define CTDL_INIT_CALL(module_name) ctdl_module_##module_name##_init (threading)

#define CTDL_MODULE_UPGRADE(module_name) char *ctdl_module_##module_name##_upgrade (void)

#define CTDL_UPGRADE_CALL(module_name) ctdl_module_##module_name##_upgrade ()

#define CtdlAideMessage(TEXT, SUBJECT)		\
	quickie_message(			\
		"Citadel",			\
		NULL,				\
		NULL,				\
		AIDEROOM,			\
		TEXT,				\
		FMT_CITADEL,			\
		SUBJECT) 


#define CtdlAideFPMessage(TEXT, SUBJECT, N, STR, STRLEN, ccid, ioid, TIME) \
	flood_protect_quickie_message(			 \
		"Citadel",				 \
		NULL,					 \
		NULL,					 \
		AIDEROOM,				 \
		TEXT,					 \
		FMT_CITADEL,				 \
		SUBJECT,				 \
		N,					 \
		STR,					 \
		STRLEN,					 \
		ccid,					 \
		ioid,					 \
		TIME)
/*
 * Hook functions available to modules.
 */
/* Priorities for  */
#define PRIO_QUEUE 500
#define PRIO_AGGR 1000
#define PRIO_SEND 1500
#define PRIO_CLEANUP 2000
/* Priorities for EVT_HOUSE */
#define PRIO_HOUSE 3000
/* Priorities for EVT_LOGIN */
#define PRIO_CREATE 10000
/* Priorities for EVT_LOGOUT */
#define PRIO_LOGOUT 15000
/* Priorities for EVT_LOGIN */
#define PRIO_LOGIN 20000
/* Priorities for EVT_START */
#define PRIO_START 25000
/* Priorities for EVT_STOP */
#define PRIO_STOP 30000
/* Priorities for EVT_ASYNC */
#define PRIO_ASYNC 35000
/* Priorities for EVT_SHUTDOWN */
#define PRIO_SHUTDOWN 40000
/* Priorities for EVT_UNSTEALTH */
#define PRIO_UNSTEALTH 45000
/* Priorities for EVT_STEALTH */
#define PRIO_STEALTH 50000
void CtdlRegisterTDAPVetoHook(int (*fcn_ptr)(StrBuf*), int EventType, int Priority);
void CtdlUnregisterTDAPVetoHook(int (*fcn_ptr) (StrBuf*), int EventType);


void CtdlRegisterSessionHook(void (*fcn_ptr)(void), int EventType, int Priority);
void CtdlUnregisterSessionHook(void (*fcn_ptr)(void), int EventType);
void CtdlShutdownServiceHooks(void);

void CtdlRegisterUserHook(void (*fcn_ptr)(struct ctdluser *), int EventType);
void CtdlUnregisterUserHook(void (*fcn_ptr)(struct ctdluser *), int EventType);

void CtdlRegisterXmsgHook(int (*fcn_ptr)(char *, char *, char *, char *), int order);
void CtdlUnregisterXmsgHook(int (*fcn_ptr)(char *, char *, char *, char *), int order);

void CtdlRegisterMessageHook(int (*handler)(struct CtdlMessage *, recptypes*),
							int EventType);
void CtdlUnregisterMessageHook(int (*handler)(struct CtdlMessage *, recptypes *),
			       int EventType);

void CtdlRegisterNetprocHook(int (*handler)(struct CtdlMessage *, char *) );
void CtdlUnregisterNetprocHook(int (*handler)(struct CtdlMessage *, char *) );

void CtdlRegisterRoomHook(int (*fcn_ptr)(struct ctdlroom *) );
void CtdlUnregisterRoomHook(int (*fnc_ptr)(struct ctdlroom *) );

void CtdlRegisterDeleteHook(void (*handler)(char *, long) );
void CtdlUnregisterDeleteHook(void (*handler)(char *, long) );

void CtdlRegisterCleanupHook(void (*fcn_ptr)(void));
void CtdlUnregisterCleanupHook(void (*fcn_ptr)(void));

void CtdlRegisterEVCleanupHook(void (*fcn_ptr)(void));
void CtdlUnregisterEVCleanupHook(void (*fcn_ptr)(void));

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

void CtdlRegisterSearchFuncHook(void (*fcn_ptr)(int *, long **, const char *), char *name);

/*
 * if you say a) (which may take a while)
 * don't forget to say b)
 */
void CtdlDisableHouseKeeping(void);
void CtdlEnableHouseKeeping(void);

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

/* return the current context list as an array and do it in a safe manner
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


int CtdlIsUserLoggedIn (char *user_name);
int CtdlIsUserLoggedInByNum (long usernum);
void CtdlBumpNewMailCounter(long which_user);


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
typedef struct CfgLineType CfgLineType;
typedef struct RoomNetCfgLine RoomNetCfgLine;
typedef struct OneRoomNetCfg OneRoomNetCfg;

unsigned CtdlCreateRoom(char *new_room_name,
			int new_room_type,
			char *new_room_pass,
			int new_room_floor,
			int really_create,
			int avoid_access,
			int new_room_view);
int CtdlGetRoom(struct ctdlroom *qrbuf, const char *room_name);
int CtdlGetRoomLock(struct ctdlroom *qrbuf, const char *room_name);
int CtdlDoIHavePermissionToDeleteThisRoom(struct ctdlroom *qr);
void CtdlRoomAccess(struct ctdlroom *roombuf, struct ctdluser *userbuf, int *result, int *view);
void CtdlPutRoomLock(struct ctdlroom *qrbuf);
typedef void (*ForEachRoomCallBack)(struct ctdlroom *EachRoom, void *out_data);
void CtdlForEachRoom(ForEachRoomCallBack CB, void *in_data);
typedef void (*ForEachRoomNetCfgCallBack)(struct ctdlroom *EachRoom, void *out_data, OneRoomNetCfg *OneRNCFG);
void CtdlForEachNetCfgRoom(ForEachRoomNetCfgCallBack CB, void *in_data, RoomNetCfg filter);
void SaveChangedConfigs(void);
void CtdlDeleteRoom(struct ctdlroom *qrbuf);
int CtdlRenameRoom(char *old_name, char *new_name, int new_floor);
void CtdlUserGoto (char *where, int display_result, int transiently, int *msgs, int *new, long *oldest, long *newest);
struct floor *CtdlGetCachedFloor(int floor_num);
void CtdlScheduleRoomForDeletion(struct ctdlroom *qrbuf);
void CtdlGetFloor (struct floor *flbuf, int floor_num);
void CtdlPutFloor (struct floor *flbuf, int floor_num);
void CtdlPutFloorLock(struct floor *flbuf, int floor_num);
int CtdlGetFloorByName(const char *floor_name);
int CtdlGetFloorByNameLock(const char *floor_name);
int CtdlGetAvailableFloor(void);
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
	ac_logged_in_or_guest,
	ac_logged_in,
	ac_room_aide,
	ac_aide,
	ac_internal,
};



/*
 * API declarations from serv_extensions.h
 */
void CtdlModuleDoSearch(int *num_msgs, long **search_msgs, const char *search_string, const char *func_name);

#define NODENAME		CtdlGetConfigStr("c_nodename")
#define FQDN			CtdlGetConfigStr("c_fqdn")
#define CTDLUID			ctdluid
#define CREATAIDE		CtdlGetConfigInt("c_creataide")
#define REGISCALL		CtdlGetConfigInt("c_regiscall")
#define TWITDETECT		CtdlGetConfigInt("c_twitdetect")
#define TWITROOM		CtdlGetConfigStr("c_twitroom")
#define RESTRICT_INTERNET	CtdlGetConfigInt("c_restrict")

typedef void (*CfgLineParser)(const CfgLineType *ThisOne, StrBuf *Line, const char *LinePos, OneRoomNetCfg *rncfg);
typedef void (*CfgLineSerializer)(const CfgLineType *ThisOne, StrBuf *OuptputBuffer, OneRoomNetCfg *rncfg, RoomNetCfgLine *data);
typedef void (*CfgLineDeAllocator)(const CfgLineType *ThisOne, RoomNetCfgLine **data);

struct CfgLineType {
	RoomNetCfg C;
	CfgLineParser Parser;
	CfgLineSerializer Serializer;
	CfgLineDeAllocator DeAllocator;
	ConstStr Str;
	int IsSingleLine;
	int nSegments;
};

struct RoomNetCfgLine {
	RoomNetCfgLine *next;
	int nValues;
	StrBuf **Value;
};

struct OneRoomNetCfg {
	long lastsent;
	long changed;
	StrBuf *Sender;
	StrBuf *RoomInfo;
	RoomNetCfgLine *NetConfigs[maxRoomNetCfg];
	StrBuf *misc;
};


#define CtdlREGISTERRoomCfgType(a, p, uniq, nSegs, s, d) RegisterRoomCfgType(#a, sizeof(#a) - 1, a, p, uniq, nSegs, s, d);
void RegisterRoomCfgType(const char* Name, long len, RoomNetCfg eCfg, CfgLineParser p, int uniq, int nSegments, CfgLineSerializer s, CfgLineDeAllocator d);
void ParseGeneric(const CfgLineType *ThisOne, StrBuf *Line, const char *LinePos, OneRoomNetCfg *sc);
void SerializeGeneric(const CfgLineType *ThisOne, StrBuf *OutputBuffer, OneRoomNetCfg *sc, RoomNetCfgLine *data);
void DeleteGenericCfgLine(const CfgLineType *ThisOne, RoomNetCfgLine **data);
RoomNetCfgLine *DuplicateOneGenericCfgLine(const RoomNetCfgLine *data);
void AddRoomCfgLine(OneRoomNetCfg *OneRNCfg, struct ctdlroom *qrbuf, RoomNetCfg LineType, RoomNetCfgLine *Line);

OneRoomNetCfg* CtdlGetNetCfgForRoom(long QRNumber);

typedef struct _nodeconf {
	int DeleteMe;
	StrBuf *NodeName;
	StrBuf *Secret;
	StrBuf *Host;
	StrBuf *Port;
}CtdlNodeConf;

HashList* CtdlLoadIgNetCfg(void);


int CtdlNetconfigCheckRoomaccess(char *errmsgbuf, 
				 size_t n,
				 const char* RemoteIdentifier);


typedef struct __NetMap {
	StrBuf *NodeName;
	time_t lastcontact;
	StrBuf *NextHop;
}CtdlNetMap;

HashList* CtdlReadNetworkMap(void);
StrBuf *CtdlSerializeNetworkMap(HashList *Map);
void NetworkLearnTopology(char *node, char *path, HashList *the_netmap, int *netmap_changed);
int CtdlIsValidNode(const StrBuf **nexthop,
		    const StrBuf **secret,
		    StrBuf *node,
		    HashList *IgnetCfg,
		    HashList *the_netmap);




int CtdlNetworkTalkingTo(const char *nodename, long len, int operation);

/*
 * Operations that can be performed by network_talking_to()
 */
enum {
        NTT_ADD,
        NTT_REMOVE,
        NTT_CHECK
};

/*
 * Expose API calls from user_ops.c
 */
int CtdlGetUser(struct ctdluser *usbuf, char *name);
int CtdlGetUserLen(struct ctdluser *usbuf, const char *name, long len);
int CtdlGetUserLock(struct ctdluser *usbuf, char *name);
void CtdlPutUser(struct ctdluser *usbuf);
void CtdlPutUserLock(struct ctdluser *usbuf);

int CtdlLockGetCurrentUser(void);
void CtdlPutCurrentUserLock(void);

int CtdlGetUserByNumber(struct ctdluser *usbuf, long number);
void CtdlGetRelationship(visit *vbuf,
                        struct ctdluser *rel_user,
                        struct ctdlroom *rel_room);
void CtdlSetRelationship(visit *newvisit,
                        struct ctdluser *rel_user,
                        struct ctdlroom *rel_room);
void CtdlMailboxName(char *buf, size_t n, const struct ctdluser *who, const char *prefix);

int CtdlLoginExistingUser(char *authname, const char *username);

/*
 * Values which may be returned by CtdlLoginExistingUser()
 */
enum {
	pass_ok,
	pass_already_logged_in,
	pass_no_user,
	pass_internal_error,
	pass_wrong_password
};

int CtdlTryPassword(const char *password, long len);
/*
 * Values which may be returned by CtdlTryPassword()
 */
enum {
	login_ok,
	login_already_logged_in,
	login_too_many_users,
	login_not_found
};

void CtdlUserLogout(void);




/*
 * Expose API calls from msgbase.c
 */



/*
 * Expose API calls from euidindex.c
 */
long CtdlLocateMessageByEuid(char *euid, struct ctdlroom *qrbuf);



#endif /* CTDL_MODULE_H */
