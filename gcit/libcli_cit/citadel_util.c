/*

citadel_util.c
btx@calyx.net

These routines deal with the two citadel structures, parms and lists.

*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include "client_api.h"
#include "transport.h"
#include "citadel_util.h"

int get_num_args(char *line)
{
   int nargs;
   char *cptr;
   
   if (strlen(line) < 4)
      return 0;
      
   nargs = 1;

   for (cptr = &line[4]; ((*cptr) && (*cptr != '\n')); cptr++)
   {
      if ((*cptr) == '|')
         nargs++;
   }
   
   return nargs;
}

int get_arg_no(int argno, char *line, char *buf)
{
   int i,j;
   int an=1;
   for (i=0; ((i<strlen(line)) && (an <= argno)); i++)
   {
      if (line[i] == '|')
         an++;
   }
   
   if (i==strlen(line))
   {
      fprintf(stderr, "ERror- trying to read arg # %d from %s\n", argno, line);
      return -1;
   }
   for (j=0; ((line[i] != '|') && (line[i] != '\0')); j++)
   {
      buf[j] = line[i++];
   }

   buf[j] = '\0';
   return OK;
}

int citadel_parseparms(char *line, citadel_parms *parms)
{
   int i;
   char arg[256];
   char nbuf[4];
   char *eptr;

   if (!parms)
   {
      fprintf(stderr, "Error: parms in citadel_parseparms is NULL!\n");
      return -1;
   }

   if (!line)
   {
      fprintf(stderr, "Error: line in citadel_parseparms is NULL!\n");
      return -1;
   }

   parms->line = (char *)strdup(line);
   parms->argc = get_num_args(line);
#ifdef DEBUG
   printf("There are %d args in the line %s.\n", parms->argc, line);   
#endif
   
   nbuf[3] = '\0';
   nbuf[0] = line[0];
   nbuf[1] = line[1];
   nbuf[2] = line[2];
   
   parms->return_code = strtol(nbuf, &eptr, 10);
   
   for (i=0; i<parms->argc; i++)
   {
      if (eptr[0])
         get_arg_no(i, line, arg);
      else
         get_arg_no(i, &line[4], arg);
#ifdef DEBUG
      printf("Got arg #%d - %s\n", i, arg);
#endif            
      parms->argv[i] = (char *) strdup(arg);
   }

   return 0;
}
                  
int free_citadel_parms(citadel_parms **parms)
{
   int i;

   if ((!parms) || (!*parms))
   {
      fprintf(stderr, "Error: parms in free_citadel_parms is NULL!\n");
      return -1;
   }
   
#ifdef DEBUG
   printf("free_citadel_parms: argc=%d\n", (*parms)->argc);
#endif   

   if ((*parms)->line)
      free((*parms)->line);

   for (i=0; i<(*parms)->argc; i++)
   {
      if ((*parms)->argv[i])
         free ((*parms)->argv[i]);
   }
   
#ifdef DEBUG
   printf("Freeing structure\n");
#endif   
   free(*parms);
   *parms = NULL;
   return(0);
}

citadel_parms *newparms()
{
   citadel_parms *parms;

   if ((parms = (citadel_parms *)malloc(sizeof(citadel_parms))) == NULL)
   {
      fprintf(stderr, "Malloc in newparms() failed\n");
      exit(1);
   }
   
   bzero(parms, sizeof(citadel_parms));
   parms->argc = 0;
   
   return(parms);
                           
}

int reset_parms(citadel_parms **parms)
{
   if (!parms)
      return 0;
      
   free_citadel_parms(parms);
   if (!(*parms = newparms()))
   {
      fprintf(stderr, "Error: newparms() failed in reset_parms()!\n");
      exit(1);	/* fatal */
   }
#ifdef DEBUG
   fprintf(stderr, "Parms reset.\n");
#endif 
   return (*parms != NULL);
}

int get_line(int fd, char *line, int maxline)
{
   int cl = 0;
   int n;
   
   if (!line)
   {
      fprintf(stderr, "Error: line in citadel_recv is NULL!\n");
      return -1;
   }
   
#ifdef DEBUG
   fprintf(stderr, "Reading in get_line...\n");
#endif   
   line[0] = '\0';
   
   do
   {
      n = read(fd, &line[cl], 1);
      
      if (n)
      {
         if (line[cl] == '\n')
            line[cl] = '\0';
       
         cl ++;
      }
   } while ((n) && (cl < maxline) && (line[cl-1]));
   
   if (cl == maxline)
      line[maxline-1] = '\0';
      
   if (!n)
      line[cl] = '\0';
   
#ifdef DEBUG
   printf("Received: %s\n", line);
#endif
   return(cl);
}

int citadel_recvparms(int sd, citadel_parms *parms)
{
   char strbuf[CITADEL_STR_SIZE];
   int ret;
   
   ret = citadel_recv_line(sd, strbuf, CITADEL_STR_SIZE);
   if (ret < 0)
      return(ret);
      
   if ((ret=citadel_parseparms(strbuf, parms)) < 0)
   {
      fprintf(stderr, "Error: citadel_parseparms failed in citadel_recvparms\n");
   }
   
   return(ret);
}

int citadel_send_line(int sd, char *line)
{
   char strbuf[CITADEL_STR_SIZE+1];
   
   strbuf[CITADEL_STR_SIZE] = '\0';
   snprintf(strbuf, CITADEL_STR_SIZE, "%s\n", line);
   return(citadel_send(sd, strbuf, strlen(strbuf)));
}
                                                                                 
int citadel_sendparms(int sd, citadel_parms *parms, char *cmd, int expect)
{
   char strbuf[CITADEL_STR_SIZE];
   int nb=0;
   int i;
   int ret;
   
   strbuf[0] = '\0';
   
   strcpy(strbuf, cmd);
   
   if (parms)
   {
      strcat(strbuf, " ");
      nb += strlen(strbuf)+1;
      
      for (i=0; i<parms->argc; i++)
      {
         if ((nb + strlen(parms->argv[i]) + 2) > CITADEL_STR_SIZE)
            break;
         
         if (i > 0)
         {
            strcat(strbuf, "|");
            nb += 1;
         }
         
         strcat(strbuf, parms->argv[i]);
         nb += strlen(parms->argv[i]);
      }
   }
   strcat(strbuf, "\n");
   nb += 1;
   
   ret = citadel_send(sd, strbuf, nb);
   if ((ret < 0) || (!expect))
      return(ret);
      
   ret = citadel_recvparms(sd, parms);
   return(ret);
}

int add_citadel_list(citadel_list **first_list, char *item)
{
   citadel_list *newlist, *t_list;

#ifdef DEBUG
   fprintf(stderr, "Adding %s to the citadel list\n", item);
#endif   
   
   if ((newlist = (citadel_list *)malloc(sizeof(citadel_list))) == NULL)
   {
      fprintf(stderr, "Malloc failed in add_citadel_list!\n");
      exit(1);
   }
   
   newlist->next = NULL;
   newlist->listitem = (char *)strdup(item);
   
   if (!first_list)
   {
      fprintf(stderr, "Firstlist is NULL in add_citadel_list!\n");
      return -1;
   }

   if (!(*first_list))
   {
      *first_list = newlist;
      return 1;
   }
   
   for (t_list = *first_list; t_list->next; t_list=t_list->next)
      ;
      
   t_list->next = newlist;
   
   return(1);
}

int free_citadel_list(citadel_list **first_list)
{
   citadel_list *t_list, *n_list;
   
   if (!first_list)
   {
      fprintf(stderr, "First_list is NULL in add_citadel_list!\n");
      return -1;
   }
   
   for (t_list = *first_list; t_list; t_list = n_list)
   {
      n_list = t_list->next;
      
      if (t_list->listitem)
         free(t_list->listitem);
      
      free(t_list);
   }
   
   *first_list = NULL;
   
   return(1);
}

/*
 * citadel_receive_listing() - receives a citadel listing.  Assumes
 * LISTING_FOLLOWS has already been received by the client.  Pass this
 * the socket descriptor and a list not yet allocated.
 */

int citadel_receive_listing(int sd, citadel_list **list)
{
   char strbuf[CITADEL_STR_SIZE];
   int ret;
   int notdone;
   
   do
   {
      ret = citadel_recv_line(sd, strbuf, CITADEL_STR_SIZE);
      if (ret < 1)
         return(ret);
      
      notdone = (strcmp(strbuf, "000"));
      if (notdone)
         add_citadel_list(list, strbuf);
      
   } while (notdone);
   
   return OK;
}

/*
 * citadel_send_listing() - sends a citadel listing.  Assumes
 * SEND_FOLLOWS has already been received by the client.  Pass this
 * the socket descriptor and a filled list.
 */

int citadel_send_listing(int sd, citadel_list *list)
{
   int ret;
   citadel_list *t_list;
   char strbuf[256];
   
   if (!list)
   {
      fprintf(stderr, "Error: list is NULL in citadel_send_listing!\n");
      return -1;
   }
   
   for (t_list = list; t_list; t_list=t_list->next)
   {
      snprintf(strbuf, sizeof(strbuf)-1, "%s\n", t_list->listitem);
      ret = citadel_send(sd, strbuf, strlen(strbuf));
      if (ret < 0)
         return(ret);
   }
   
   ret = citadel_send(sd, "000\n", 4);
   
   return(ret);
}

/*
 * citadel_send_listing_file(char *) - sends a citadel listing from the
 * file filename.  This assumes SEND_FOLLOWS has already been received 
 * by the client.  Pass this the socket descriptor and the filename
 */

int citadel_send_listing_file(int sd, char *filename)
{
   int ret;
   int fd;
   int l;
   char strbuf[CITADEL_STR_SIZE];
   
   if (!filename)
   {
      fprintf(stderr, "Error: filename is NULL in citadel_send_listing_file!\n");
      return -1;
   }
   
   if ((fd = open(filename, O_RDONLY)) < 0)
   {
      fprintf(stderr, "Error- filename %s does not exist!\n", filename);
      return(-1);
   }
   
   do
   {
      l = get_line(fd, strbuf, sizeof(strbuf));
      
      if (l > 0)
      {
/* If there's a blank in the first, despite l == 1, 
 * it means the line was just a \n.
 */
         if (!strbuf[0])
         {
            strcpy(strbuf, "\n");
            l = 3;
         }

         ret = citadel_send_line(sd, strbuf);
         if (ret < 0)
         {
            close(fd);
            return(ret);
         }
      }
   } while (l>0);
   
   ret = citadel_send(sd, "000\n", 4);
   
   close(fd);
   
   return(1);
}


