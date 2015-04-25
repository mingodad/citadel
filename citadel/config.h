/*
 * Copyright (c) 1987-2015 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "serv_extensions.h"
#include "citadel_dirs.h"

#define CtdlGetConfigInt(x)	atoi(CtdlGetConfigStr(x))
#define CtdlGetConfigLong(x)	atol(CtdlGetConfigStr(x))

void initialize_config_system(void);
void shutdown_config_system(void);
void put_config(void);
void CtdlSetConfigStr(char *, char *);
char *CtdlGetConfigStr(char *);
void CtdlSetConfigInt(char *key, int value);
void CtdlSetConfigLong(char *key, long value);

char *CtdlGetSysConfig(char *sysconfname);
void CtdlPutSysConfig(char *sysconfname, char *sysconfdata);
void validate_config(void);
