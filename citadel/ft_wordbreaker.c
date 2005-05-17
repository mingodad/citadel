/*
 * $Id$
 *
 * Default wordbreaker module for full text indexing.
 *
 */


#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <pwd.h>
#include <errno.h>
#include <sys/types.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <sys/wait.h>
#include <string.h>
#include <limits.h>
#include "citadel.h"
#include "server.h"
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "serv_extensions.h"
#include "database.h"
#include "msgbase.h"
#include "control.h"
#include "tools.h"
#include "ft_wordbreaker.h"


void wordbreaker(char *text, int *num_tokens, int **tokens) {

	int wb_num_tokens = 0;
	int wb_num_alloc = 0;
	int *wb_tokens = NULL;

	wb_num_tokens = 3;
	wb_tokens = malloc(wb_num_tokens * sizeof(int));

	wb_tokens[0] = 6;
	wb_tokens[1] = 7;	/* FIXME this obviously isn't a wordbreaker */
	wb_tokens[2] = 8;

	*num_tokens = wb_num_tokens;
	*tokens = wb_tokens;
}

