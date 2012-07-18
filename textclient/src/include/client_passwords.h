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


void determine_pwfilename(char *pwfile, size_t n);
void get_stored_password(
		char *host,
		char *port,
		char *username,
		char *password);
void set_stored_password(
		char *host,
		char *port,
		char *username,
		char *password);
void offer_to_remember_password(CtdlIPC *ipc,
		char *host,
		char *port,
		char *username,
		char *password);
