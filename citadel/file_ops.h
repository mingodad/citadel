/* $Id$ */
#ifndef FILE_OPS_H
#define FILE_OPS_H

#include "context.h"

void OpenCmdResult (char *, const char *);
void abort_upl (CitContext *who);

int network_talking_to(char *nodename, int operation);

/*
 * Operations that can be performed by network_talking_to()
 */
enum {
        NTT_ADD,
        NTT_REMOVE,
        NTT_CHECK
};

#endif /* FILE_OPS_H */