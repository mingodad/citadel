/*
 * Copyright (c) 1996-2013 by the citadel.org team
 *
 * This program is open source software.  You can redistribute it and/or
 * modify it under the terms of the GNU General Public License, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

void StrEscPuts(const StrBuf *strbuf);
void StrEscputs1(const StrBuf *strbuf, int nbsp, int nolinebreaks);

void urlescputs(const char *);
void hurlescputs(const char *);
long stresc(char *target, long tSize, char *strbuf, int nbsp, int nolinebreaks);
void escputs(const char *strbuf);
