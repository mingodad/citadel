/*
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

/*
 * Colors for color() command
 */
#define DIM_BLACK	0
#define DIM_RED		1
#define DIM_GREEN	2
#define DIM_YELLOW	3
#define DIM_BLUE	4
#define DIM_MAGENTA	5
#define DIM_CYAN	6
#define DIM_WHITE	7
#define BRIGHT_BLACK	8
#define BRIGHT_RED	9
#define BRIGHT_GREEN	10
#define BRIGHT_YELLOW	11
#define BRIGHT_BLUE	12
#define BRIGHT_MAGENTA	13
#define BRIGHT_CYAN	14
#define BRIGHT_WHITE	15
#define COLOR_PUSH	16	/* Save current color */
#define COLOR_POP	17	/* Restore saved color */
#define ORIGINAL_PAIR	-1	/* Default terminal colors */

/*
 * declarations
 */
void load_command_set(void);
void stty_ctdl(int cmd);
void newprompt(char *prompt, char *str, int len);
void strprompt(char *prompt, char *str, int len);
int boolprompt(char *prompt, int prev_val);
int intprompt(char *prompt, int ival, int imin, int imax);
int fmout(int width, FILE *fpin, char *text, FILE *fpout, int subst);
int getcmd(CtdlIPC *ipc, char *argbuf);
void display_help(CtdlIPC *ipc, char *name);
void color(int colornum);
void cls(int colornum);
void send_ansi_detect(void);
void look_for_ansi(void);
int inkey(void);
void set_keepalives(int s);
extern int enable_color;
int yesno(void);
int yesno_d(int d);
void keyopt(char *);
char keymenu(char *menuprompt, char *menustring);
void async_ka_start(void);
void async_ka_end(void);
int checkpagin(int lp, unsigned int pagin, unsigned int height);
char was_a_key_pressed(void);

#ifdef __GNUC__
void pprintf(const char *format, ...) __attribute__((__format__(__printf__,1,2)));
#else
void pprintf(const char *format, ...);
#endif



extern char rc_url_cmd[SIZ];
extern char rc_open_cmd[SIZ];
extern char rc_gotmail_cmd[SIZ];
extern int lines_printed;
extern int rc_remember_passwords;
