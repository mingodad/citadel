/* Citadel/UX call log stats program
 * version 2.4
 */
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <stdio.h>
#include "citadel.h"

#define disply(x,y) printf("%20s            %4.1f    %4.1f    %4d\n",x,((float)y)/calls,((float)y)/days,y)

#define GRANULARITY 100

int batch_mode = 0;

struct caller
  {
    struct caller *next;
    char Cname[30];
    int Ctimescalled;
  };

void get_config ();
struct config config;


void 
prompt ()
{
  char buf[16];
  if (batch_mode == 0)
    {
      printf ("Press return to continue...");
      fgets (buf, 16, stdin);
    }
  else
    {
      printf ("\n");
    }
}

int 
halfhour (time)			/* Returns half-hour time period of time */
     long time;
{
  int a;
  struct tm *tm;
  tm = (struct tm *) localtime (&time);
  a = (tm->tm_hour) * 3;
  if ((tm->tm_min) > 19)
    ++a;
  if ((tm->tm_min) > 39)
    ++a;
  return (a);
}



void 
progress (curr, max)
     long curr;
     long max;
{
  static int dots;
  int pos;

  if (curr == 0L)
    {
      printf ("--------------------------------------");
      printf ("--------------------------------------\r");
      fflush (stdout);
      dots = 0;
    }

  pos = (curr * 72) / max;
  while (dots < pos)
    {
      putc ('*', stdout);
      fflush (stdout);
      ++dots;
    }

  if (dots == 72)
    {
      printf ("                                      ");
      printf ("                                      \r");
      fflush (stdout);
    }

}




void 
main (argc, argv)
     int argc;
     char *argv[];
{
  struct calllog calllog;
  struct usersupp usersupp;
  int file, pos, a, b, lowest;
  float p, q;
  long timeon[72];
  long timeup[72];
  char dname[30];
  int sess = 0;
  long cftime, cttime, aa;
  int calls, logins, newusers;
  int badpws, terms, drops, sleeps;
  long from, to, tottime;
  int days, hours, minutes;
  char aaa[100];
  struct tm *tm;
  struct caller *callers = NULL;
  struct caller *callptr = NULL;
  FILE *fp, *sortpipe;
  char thegraph[GRANULARITY][73];
  int pc_only = 0;

  for (a = 0; a < argc; ++a)
    {
      if (!strcmp (argv[a], "-b"))
	batch_mode = 1;
      if (!strcmp (argv[a], "-p"))
	pc_only = 1;
    }


  for (a = 0; a < GRANULARITY; ++a)
    strcpy (thegraph[a],
	    "........................................................................");

  get_config ();

  if (pc_only)
    goto PC_ONLY_HERE;

  if (!batch_mode)
    printf ("Scanning call log, please wait...\n\n\n\n");

  file = open ("calllog.pos", O_RDONLY);
  read (file, &pos, 2);
  close (file);
  from = 0L;
  to = 0L;
  for (a = 0; a < 72; ++a)
    {
      timeon[a] = 0L;
      timeup[a] = 0L;
    }
  cftime = 0L;
  cttime = 0L;

  calls = 0;
  logins = 0;
  newusers = 0;
  badpws = 0;
  terms = 0;
  drops = 0;
  sleeps = 0;
  file = open ("calllog", O_RDONLY);
  lseek (file, (long) (pos * sizeof (struct calllog)), 0);
  if (!batch_mode)
    printf ("Scanning call log, please wait...\n");
  for (a = 0; a < CALLLOG; ++a)
    {
      if (!batch_mode)
	progress ((long) a, (long) (CALLLOG - 1));
      if ((a + pos) == CALLLOG)
	lseek (file, 0L, 0);
      read (file, &calllog, sizeof (struct calllog));
      if (calllog.CLflags != 0)
	{
	  if ((calllog.CLtime < from) || (from == 0L))
	    from = calllog.CLtime;
	  if ((calllog.CLtime > to) || (to == 0L))
	    to = calllog.CLtime;
	  strcpy (aaa, "");
	  if (calllog.CLflags & CL_CONNECT)
	    {
	      ++calls;
	      ++sess;
	      if (sess == 1)
		cftime = calllog.CLtime;
	      strcpy (dname, calllog.CLfullname);
	    }
	  if (calllog.CLflags & CL_LOGIN)
	    {
	      ++logins;
	      b = 0;
	      for (callptr = callers; callptr != NULL; callptr = callptr->next)
		{
		  if (!strcmp (callptr->Cname, calllog.CLfullname))
		    {
		      ++b;
		      ++callptr->Ctimescalled;
		    }
		}
	      if (b == 0)
		{
		  callptr = (struct caller *) malloc (sizeof (struct caller));
		  callptr->next = callers;
		  callers = callptr;
		  strcpy (callers->Cname, calllog.CLfullname);
		  callers->Ctimescalled = 1;
		}
	    }
	  if (calllog.CLflags & CL_NEWUSER)
	    ++newusers;
	  if (calllog.CLflags & CL_BADPW)
	    ++badpws;
	  if (calllog.CLflags & CL_TERMINATE)
	    {
	      ++terms;
	      --sess;
	      if (sess == 0)
		{
		  cttime = calllog.CLtime;
		  for (aa = cftime; aa <= cttime; aa = aa + 300L)
		    timeon[halfhour (aa)] = timeon[halfhour (aa)] + 5L;
		  cftime = 0L;
		  cttime = 0L;
		}
	    }
	  if (calllog.CLflags & CL_DROPCARR)
	    {
	      ++drops;
	      --sess;
	      if (sess == 0)
		{
		  cttime = calllog.CLtime;
		  for (aa = cftime; aa <= cttime; aa = aa + 300L)
		    timeon[halfhour (aa)] = timeon[halfhour (aa)] + 5L;
		  cftime = 0L;
		  cttime = 0L;
		}
	    }
	  if (calllog.CLflags & CL_SLEEPING)
	    {
	      ++sleeps;
	      --sess;
	      if (sess == 0)
		{
		  cttime = calllog.CLtime;
		  for (aa = cftime; aa <= cttime; aa = aa + 300L)
		    timeon[halfhour (aa)] = timeon[halfhour (aa)] + 5L;
		  cftime = 0L;
		  cttime = 0L;
		}
	    }

	  if (sess < 0)
	    sess = 0;

	}
    }
  close (file);
  tottime = to - from;
  days = (int) (tottime / 86400L);
  hours = (int) ((tottime % 86400L) / 3600L);
  minutes = (int) ((tottime % 3600L) / 60L);

  printf ("                              Avg/Call Avg/Day  Total\n");
  disply ("Calls:", calls);
  disply ("Logins:", logins);
  disply ("New users:", newusers);
  disply ("Bad pw attempts:", badpws);
  disply ("Proper logoffs:", terms);
  disply ("Carrier drops:", drops);
  disply ("Sleeping drops:", sleeps);

  printf ("\n");
  tm = (struct tm *) localtime (&from);
  printf ("From:              %s", (char *) asctime (localtime (&from)));
  printf ("To:                %s", (char *) asctime (localtime (&to)));
  printf ("Total report time: ");
  printf ("%d days, %d hours, %d minutes\n",
	  days, hours, minutes);

  for (aa = from; aa <= to; aa = aa + 1200L)
    timeup[halfhour (aa)] = timeup[halfhour (aa)] + 20L;
  prompt ();

  lowest = GRANULARITY - 1;
  for (b = 0; b < 72; ++b)
    {
      for (a = 0; a <= GRANULARITY; ++a)
	{
	  p = ((float) timeon[b]) / ((float) timeup[b]) * GRANULARITY;
	  q = (float) a;
	  if (p >= q)
	    {
	      thegraph[(GRANULARITY - 1) - a][b] = '#';
	      if (lowest > (GRANULARITY - 1) - a)
		lowest = (GRANULARITY - 1) - a;
	    }
	}
    }

  printf ("\n\n\n\n\n\n");

  b = ((GRANULARITY - lowest) / 18);
  if (b < 1)
    b = 1;
  for (a = lowest; a < GRANULARITY; a = a + b)
    printf ("%2d%% |%s\n",
	    100 - (a + 1),
	    thegraph[a]);
  printf ("    +------------------------------------------------------------------------\n");
  printf ("     0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16 17 18 19 20 21 22 23\n");
  fflush (stdout);
  prompt ();

  printf ("\n\n\n\n");


  printf ("Top 20 Callers (sorted by total number of logins)\n");
  printf ("Calls Avg/Day Username\n");
  printf ("----- ------- ------------------------------\n");
  fflush (stdout);
  sortpipe = (FILE *) popen ("sort |tail -20 |sort -r", "w");
  for (callptr = callers; callptr != NULL; callptr = callptr->next)
    {
      fprintf (sortpipe, "%5d %7.2f %-30s\n",
	       callptr->Ctimescalled,
	       (((float) callptr->Ctimescalled) / ((float) days)),
	       callptr->Cname);
    }
  pclose (sortpipe);
  while (callers != NULL)
    {
      callptr = callers->next;
      free (callers);
      callers = callptr;
    }
  prompt ();

PC_ONLY_HERE:
  printf ("Top 20 Contributing Users (post to call ratio)\n");
  printf ("P/C Ratio Username\n");
  printf ("--------- ------------------------------\n");
  fflush (stdout);
  sortpipe = (FILE *) popen ("sort |tail -20 |sort -r", "w");
  fp = fopen ("usersupp", "r");
  while ((fp != NULL)
       && (fread ((char *) &usersupp, sizeof (struct usersupp), 1, fp) > 0))
    {
      fprintf (sortpipe, "%9.2f %-30s\n",
	       ((float) usersupp.posted / (float) usersupp.timescalled),
	       usersupp.fullname);
    }
  fclose (fp);
  pclose (sortpipe);
  exit (0);
}
