/*
 * Copyright (c) 1987-2012 by the citadel.org team
 *
 *  This program is open source software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include "serv_extensions.h"
#include "citadel_dirs.h"


void get_config(void);
void put_config(void);

char *CtdlGetSysConfig(char *sysconfname);
void CtdlPutSysConfig(char *sysconfname, char *sysconfdata);
