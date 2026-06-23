/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */


#pragma once

#include "wire.h"

/* one chat room */
struct Room
{
    char name[MAX_ROOM];
    char *ops;   /* path to ops file */
    char *bans;  /* path to bans file */
    char *topic; /* current topic, NULL if none */
    char *path;  /* path to room directory */
};

Room *room_create(Server *s, const char *name, const char *creator);
Room *room_find(Server *s, const char *name);
int room_is_banned(Room *r, const char *nick);
void room_add_op(Room *r, const char *nick);
void room_remove_op(Room *r, const char *nick);
void room_ban(Room *r, const char *nick);
void room_unban(Room *r, const char *nick);
void room_set_topic(Room *r, const char *text);
void room_log(Room *r, const char *line);
