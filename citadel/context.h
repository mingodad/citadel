/* $Id: sysdep_decls.h 7265 2009-03-25 23:18:46Z dothebart $ */

#ifndef CONTEXT_H
#define CONTEXT_H

#include <stdarg.h>
#include "sysdep.h"
#include "server.h"
#include "sysdep_decls.h"
#include "threads.h"


extern citthread_key_t MyConKey;			/* TSD key for MyContext() */
extern int num_sessions;
extern struct CitContext masterCC;

struct CitContext *MyContext (void);
void RemoveContext (struct CitContext *);
struct CitContext *CreateNewContext (void);
void context_cleanup(void);
void kill_session (int session_to_kill);
INLINE void become_session(struct CitContext *which_con);
void InitializeMasterCC(void);
void dead_session_purge(int force);



#endif /* CONTEXT_H */
