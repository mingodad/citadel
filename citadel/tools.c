/*
 * tools.c -- Miscellaneous routines used by both the client and server.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tools.h"

char *safestrncpy(char *dest, const char *src, size_t n)
{
  if (dest == NULL || src == NULL)
    {
      fprintf(stderr, "safestrncpy: NULL argument\n");
      abort();
    }
  strncpy(dest, src, n);
  dest[n - 1] = 0;
  return dest;
}
