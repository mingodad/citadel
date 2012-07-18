/*
 * client code may need the ERR define
 *
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

void screen_new(void);
int scr_printf(char *fmt, ...);
#define SCR_NOBLOCK 0
#define SCR_BLOCK -1
int scr_getc(int delay);
int scr_putc(int c);
void scr_flush(void);
int scr_blockread(void);
RETSIGTYPE scr_winch(int signum);
void wait_indicator(int state);
void ctdl_beep(void);
void scr_wait_indicator(int);
extern char status_line[];
extern void check_screen_dims(void);

extern int screenwidth;
extern int screenheight;
