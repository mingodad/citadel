/* 
 * readlog.c  (a simple program to parse citadel.log)
 * $Id$
 */

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include "citadel.h"

void get_config (void);
struct config config;

int 
main (int argc, char **argv)
{
  time_t LogTime;
  unsigned int LogType;
  char LogName[256];

  char buf[256];
  char aaa[100];
  struct tm *tm;
  char *tstring;
  FILE *logfp;

  get_config ();

  logfp = fopen ("citadel.log", "r");
  if (logfp == NULL)
    {
      perror ("Could not open citadel.log");
      exit (errno);
    }
  else
    {
      while (fgets (buf, 256, logfp) != NULL)
	{
	  buf[strlen (buf) - 1] = 0;
	  strcat(buf, " ");

	  LogTime = atol (strtok(buf, "|"));
          LogType = atol (strtok(NULL, "|"));
          strcpy(LogName, strtok(NULL, "|"));

	  if (LogType != 0)
	    {
	      strcpy (aaa, "");
	      if (LogType & CL_CONNECT)
		strcpy (aaa, "Connect");
	      if (LogType & CL_LOGIN)
		strcpy (aaa, "Login");
	      if (LogType & CL_NEWUSER)
		strcpy (aaa, "New User");
	      if (LogType & CL_BADPW)
		strcpy (aaa, "Bad PW Attempt");
	      if (LogType & CL_TERMINATE)
		strcpy (aaa, "Terminate");
	      if (LogType & CL_DROPCARR)
		strcpy (aaa, "Dropped Carrier");
	      if (LogType & CL_SLEEPING)
		strcpy (aaa, "Sleeping");
	      if (LogType & CL_PWCHANGE)
		strcpy (aaa, "Changed Passwd");
	      tm = (struct tm *) localtime (&LogTime);
	      tstring = (char *) asctime (tm);
	      printf ("%30s %20s %s", LogName, aaa, tstring);
	    }
	}
    }
  fclose(logfp);
  exit (0);
}
