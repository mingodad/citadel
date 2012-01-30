/*
 * Copyright (c) 2001-2012 by the citadel.org team
 *
 *  This program is open source software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 *  
 *  
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  
 *  
 *  
 */

void imap_pick_range(const char *range, int is_uid);
void imap_fetch(int num_parms, ConstStr *Params);
void imap_uidfetch(int num_parms, ConstStr *Params);
void imap_fetch_flags(int seq);
int imap_extract_data_items(citimap_command *Cmd);
