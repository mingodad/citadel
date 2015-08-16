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

void get_control (void);
void put_control (void);
void check_control(void);
long int get_new_message_number (void);
long int get_new_user_number (void);
long int get_new_room_number (void);
void migrate_legacy_control_record(void);
