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
#include "dynloader.h"
#include "citadel.h"
#include "server.h"
#include "sysdep_decls.h"

symtab *global_symtab;

struct FunctionHook *HookTable = NULL;

int DLoader_Exec_Cmd(char *cmdbuf)
{
   symtab *t_sym;
   void *fcn_handle;
   char *dl_error;
   void (*cmd_ptr)(void *);
   
   for (t_sym = global_symtab; ((t_sym) && (strncmp(t_sym->server_cmd, cmdbuf, strlen(t_sym->server_cmd))) ); t_sym=t_sym->next)
      ;
      
   if (t_sym)
   {
      if (!(fcn_handle = dlopen(t_sym->module_path, RTLD_NOW)))
      {
         dl_error = dlerror();
         syslog(LOG_NOTICE, "WARNING: module %s failed to load", t_sym->module_path);
         return(0);
      }
      
      cmd_ptr = dlsym(fcn_handle, t_sym->fcn_name);
      if ((dl_error = dlerror()) != NULL)
      {
         syslog(LOG_NOTICE, "dlsym error: %s - %s", dl_error, t_sym->module_path);
         return(0);
      }
      (*cmd_ptr)(&cmdbuf[5]);
      dlclose(fcn_handle);
      if ((dl_error = dlerror()) != NULL)
      {
         syslog(LOG_NOTICE, "dlclose error: %s", dl_error);
         return(0);
      }
      return(1);
   }  /* If symbol found */

   return(0);
}

void add_symbol(char *fcn_name, char *server_cmd, char *info_msg, symtab **first_symtab)
{
   symtab *new_symtab, *t_sym, *last_sym;
   
   if (!(new_symtab = malloc(sizeof(symtab))))
   {
      perror("Malloc new symtab");
      exit(1);
   }
   
   new_symtab->fcn_name = strdup(fcn_name);
   new_symtab->server_cmd = strdup(server_cmd);
   new_symtab->info_msg = strdup(info_msg);
   new_symtab->next = NULL;
   
   if (!(*first_symtab))
      (*first_symtab) = new_symtab;
   else
   {
      last_sym = NULL;
      for (t_sym = (*first_symtab); (t_sym); t_sym = t_sym->next)
         last_sym = t_sym;
      last_sym->next = new_symtab;
   }
}

void DLoader_Init(char *pathname, symtab **my_symtab)
{
   void *fcn_handle;
   void (*h_init_fcn)(struct DLModule_Info *);
   void (*h_get_symtab)(symtab **);
   char *dl_error;
   char *filename;
   DIR *dir;
   struct dirent *dptr;
   
   char pathbuf[512];
   struct DLModule_Info dl_info;
   symtab *stab = NULL;
   symtab *t_sym;
   
   global_symtab = NULL;			/* Global symbol table */
   
   if ((dir = opendir(pathname))==NULL)
   {
      perror("opendir");
      exit(1);
   }
   
   while ((dptr=readdir(dir))!= NULL)
   {
      if (dptr->d_name[0] == '.')
         continue;
   
      filename = strdup(dptr->d_name);
      snprintf(pathbuf, 512, "%s/%s", pathname, filename);
      if (!(fcn_handle = dlopen(pathbuf, RTLD_NOW)))
      {
         dl_error = dlerror();
         fprintf(stderr, "DLoader_Init dlopen failed (%s)", dl_error);
         continue;
      }
      
      h_init_fcn = dlsym(fcn_handle, "Dynamic_Module_Init");
      if ((dl_error = dlerror()) != NULL)
      {
         fprintf(stderr,"DLoader_Init dlsym failed (%s)", dl_error);
         continue;
      }
      
      (*h_init_fcn)(&dl_info);

      printf("Loaded module %s v%d.%d\nBy %s (%s)\n", dl_info.module_name, 
                                                     dl_info.major_version,
                                                     dl_info.minor_version,
                                                     dl_info.module_author,
                                                     dl_info.module_author_email);

      h_get_symtab = dlsym(fcn_handle, "Get_Symtab");
      if ((dl_error = dlerror()) != NULL)
      {
         fprintf(stderr,"DLoader_Init dlsym failed for Get_Symtab (%s) on module %s", dl_error, dl_info.module_name);
         continue;
      }
      
/* Get the symbol table for the current module and link it on */      
      
      (*h_get_symtab)(&stab);
      if (!(*my_symtab))
      {
         (*my_symtab) = global_symtab = stab;
      }
      else
      {
         for (t_sym = (*my_symtab) ; t_sym->next; t_sym=t_sym->next);
            ;
         t_sym->next = stab;
      }
      for (t_sym = stab; t_sym; t_sym=t_sym->next)
         t_sym->module_path = (char *)strdup(pathbuf);
      dlclose(fcn_handle);
      if ((dl_error = dlerror()) != NULL)
      {
         fprintf(stderr,"DLoader_Init dlclose failed (%s)", dl_error);
         continue;
      }
      
   }	/* While */
   
}



void CtdlRegisterHook(void *fcn_ptr, int fcn_type) {

	struct FunctionHook *newfcn;

	newfcn = (struct FunctionHook *) malloc(sizeof(struct FunctionHook));
	newfcn->next = HookTable;
	newfcn->h_function_pointer = fcn_ptr;
	newfcn->h_type = fcn_type;

	HookTable = newfcn;

	lprintf(5, "Registered a new function (type %d)\n", fcn_type);
	}
