/*
 * $Id$
 *
 */

#include "serv_extensions.h"
#include "citadel_dirs.h"

/* 
 * Global system configuration.  Don't change anything here.  It's all in dtds/config-defs.h now.
 */
struct config {
#include "datadefinitions.h"
#include "dtds/config-defs.h"
#include "undef_data.h"
};


void get_config(void);
void put_config(void);
extern struct config config;

