/*******************************************************
 *
 * Citadel Dynamic Loading Module
 * Written by Brian Costello
 * btx@calyx.net
 *
 ******************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <dirent.h>
#include <strings.h>
#include <syslog.h>
#include <pthread.h>
#include <limits.h>
#include "dynloader.h"
#include "citadel.h"
#include "server.h"
#include "sysdep_decls.h"

struct CleanupFunctionHook *CleanupHookTable = NULL;
struct SessionFunctionHook *SessionHookTable = NULL;
struct UserFunctionHook *UserHookTable = NULL;

struct ProtoFunctionHook
{
  void (*handler)(char *cmdbuf);
  char *cmd;
  char *desc;
  struct ProtoFunctionHook *next;
} *ProtoHookList = NULL;

void CtdlRegisterProtoHook(void (*handler)(char *), char *cmd, char *desc)
{
  struct ProtoFunctionHook *p = malloc(sizeof *p);
  
  if (p == NULL)
    {
      fprintf(stderr, "can't malloc new ProtoFunctionHook\n");
      exit(EXIT_FAILURE);
    }

  p->handler = handler;
  p->cmd = cmd;
  p->desc = desc;
  p->next = ProtoHookList;
  ProtoHookList = p;
}

int DLoader_Exec_Cmd(char *cmdbuf)
{
  struct ProtoFunctionHook *p;

  for (p = ProtoHookList; p; p = p->next)
    {
      if (!strncmp(cmdbuf, p->cmd, 4))
	{
	  p->handler(&cmdbuf[5]);
	  return 1;
	}
    }
  return 0;
}

void DLoader_Init(char *pathname)
{
   void *fcn_handle;
   char *dl_error;
   DIR *dir;
   struct dirent *dptr;
   struct DLModule_Info* (*h_init_fcn)(void);
   struct DLModule_Info *dl_info;

   char pathbuf[PATH_MAX];
   
   if ((dir = opendir(pathname))==NULL)
   {
      perror("opendir");
      exit(1);
   }
   
   while ((dptr=readdir(dir))!= NULL)
   {
      if (dptr->d_name[0] == '.')
         continue;
   
      snprintf(pathbuf, PATH_MAX, "%s/%s", pathname, dptr->d_name);
      if (!(fcn_handle = dlopen(pathbuf, RTLD_NOW)))
      {
         dl_error = dlerror();
         fprintf(stderr, "DLoader_Init dlopen failed (%s)\n", dl_error);
         continue;
      }
      
      h_init_fcn = dlsym(fcn_handle, "Dynamic_Module_Init");
      if ((dl_error = dlerror()) != NULL)
      {
         fprintf(stderr,"DLoader_Init dlsym failed (%s)\n", dl_error);
         continue;
      }
      
      dl_info = h_init_fcn();

      printf("Loaded module %s v%d.%d\nBy %s (%s)\n", dl_info->module_name, 
	     dl_info->major_version, dl_info->minor_version,
	     dl_info->module_author, dl_info->module_author_email);
   }	/* While */
}



void CtdlRegisterCleanupHook(void *fcn_ptr) {

	struct CleanupFunctionHook *newfcn;

	newfcn = (struct CleanupFunctionHook *)
		malloc(sizeof(struct CleanupFunctionHook));
	newfcn->next = CleanupHookTable;
	newfcn->h_function_pointer = fcn_ptr;
	CleanupHookTable = newfcn;

	lprintf(5, "Registered a new cleanup function\n");
	}


void CtdlRegisterSessionHook(void *fcn_ptr, int EventType) {

	struct SessionFunctionHook *newfcn;

	newfcn = (struct SessionFunctionHook *)
		malloc(sizeof(struct SessionFunctionHook));
	newfcn->next = SessionHookTable;
	newfcn->h_function_pointer = fcn_ptr;
	newfcn->eventtype = EventType;
	SessionHookTable = newfcn;

	lprintf(5, "Registered a new session function (type %d)\n", 
		EventType);
	}


void CtdlRegisterUserHook(void *fcn_ptr, int EventType) {

	struct UserFunctionHook *newfcn;

	newfcn = (struct UserFunctionHook *)
		malloc(sizeof(struct UserFunctionHook));
	newfcn->next = UserHookTable;
	newfcn->h_function_pointer = fcn_ptr;
	newfcn->eventtype = EventType;
	UserHookTable = newfcn;

	lprintf(5, "Registered a new user function (type %d)\n", 
		EventType);
	}


void PerformSessionHooks(int EventType) {
	struct SessionFunctionHook *fcn;

        for (fcn = SessionHookTable; fcn != NULL; fcn = fcn->next) {
		if (fcn->eventtype == EventType) {
                	(*fcn->h_function_pointer)();
			}
                }
	}

void PerformUserHooks(char *username, long usernum, int EventType) {
	struct UserFunctionHook *fcn;

        for (fcn = UserHookTable; fcn != NULL; fcn = fcn->next) {
		if (fcn->eventtype == EventType) {
                	(*fcn->h_function_pointer)(username, usernum);
			}
                }
	}

