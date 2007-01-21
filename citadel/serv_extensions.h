/* $Id$ */

#ifndef SERV_EXTENSIONS_H
#define SERV_EXTENSIONS_H

#include "server.h"

/*
 * This is where we declare all of the server extensions we have.
 * We'll probably start moving these to a more sane location in the near
 * future.  For now, this just shuts up the compiler.
 */
char *serv_bio_init(void);
char *serv_calendar_init(void);
void serv_calendar_destroy(void);
char *serv_notes_init(void);
char *serv_ldap_init(void);
char *serv_chat_init(void);
char *serv_expire_init(void);
char *serv_imap_init(void);
char *serv_inetcfg_init(void);
char *serv_listsub_init(void);
char *serv_mrtg_init(void);
char *serv_netfilter_init(void);
char *serv_network_init(void);
char *serv_newuser_init(void);
char *serv_pas2_init(void);
char *serv_pop3_init(void);
char *serv_rwho_init(void);
char *serv_smtp_init(void);
char *serv_spam_init(void);
char *serv_test_init(void);
char *serv_upgrade_init(void);
char *serv_vandelay_init(void);
char *serv_vcard_init(void);
char *serv_fulltext_init(void);
char *serv_autocompletion_init(void);
char *serv_postfix_tcpdict(void);
char *serv_managesieve_init(void);
char *serv_sieve_init(void);
char *serv_funambol_init(void);
/*
 */

void initialize_server_extensions(void);
int DLoader_Exec_Cmd(char *cmdbuf);
char *Dynamic_Module_Init(void);

void CtdlRegisterSessionHook(void (*fcn_ptr)(void), int EventType);
void CtdlUnregisterSessionHook(void (*fcn_ptr)(void), int EventType);
void CtdlDestroySessionHooks(void);
void PerformSessionHooks(int EventType);

void CtdlRegisterUserHook(void (*fcn_ptr)(struct ctdluser *), int EventType);
void CtdlUnregisterUserHook(void (*fcn_ptr)(struct ctdluser *), int EventType);
void CtdlDestroyUserHooks(void);
void PerformUserHooks(struct ctdluser *usbuf, int EventType);

void CtdlRegisterXmsgHook(int (*fcn_ptr)(char *, char *, char *), int order);
void CtdlUnregisterXmsgHook(int (*fcn_ptr)(char *, char *, char *), int order);
int PerformXmsgHooks(char *, char *, char *);
void CtdlDestroyXmsgHooks(void);



void CtdlRegisterMessageHook(int (*handler)(struct CtdlMessage *),
							int EventType);
void CtdlUnregisterMessageHook(int (*handler)(struct CtdlMessage *),
							int EventType);
void CtdlDestroyMessageHook(void);
int PerformMessageHooks(struct CtdlMessage *, int EventType);


void CtdlRegisterNetprocHook(int (*handler)(struct CtdlMessage *, char *) );
void CtdlUnregisterNetprocHook(int (*handler)(struct CtdlMessage *, char *) );
void CtdlDestroyNetprocHooks(void);
int PerformNetprocHooks(struct CtdlMessage *, char *);

void CtdlRegisterDeleteHook(void (*handler)(char *, long) );
void CtdlUnregisterDeleteHook(void (*handler)(char *, long) );
void CtdlDestroyDeleteHooks(void);
void PerformDeleteHooks(char *, long);


void CtdlRegisterCleanupHook(void (*fcn_ptr)(void));
void CtdlUnregisterCleanupHook(void (*fcn_ptr)(void));
void CtdlDestroyCleanupHooks(void);
void CtdlRegisterProtoHook(void (*handler)(char *), char *cmd, char *desc);
void CtdlUnregisterProtoHook(void (*handler)(char *), char *cmd);
void CtdlDestroyProtoHooks(void);
void CtdlRegisterServiceHook(int tcp_port,
			char *sockpath,
                        void (*h_greeting_function) (void),
                        void (*h_command_function) (void),
                        void (*h_async_function) (void)
);
void CtdlUnregisterServiceHook(int tcp_port,
			char *sockpath,
                        void (*h_greeting_function) (void),
                        void (*h_command_function) (void),
                        void (*h_async_function) (void)
);
void CtdlDestroyServiceHook(void);

void CtdlRegisterFixedOutputHook(char *content_type,
			void (*output_function) (char *supplied_data, int len)
);
void CtdlUnRegisterFixedOutputHook(char *content_type);
void CtdlDestroyFixedOutputHooks(void);
int PerformFixedOutputHooks(char *, char *, int);

#endif /* SERV_EXTENSIONS_H */
