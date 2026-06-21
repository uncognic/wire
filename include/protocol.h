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

/* dispatch one complete line from a client */
void handle_line(Server *s, Client *c, const char *line);

/* send the MOTD file to a client */
void send_motd(Client *c);

/* send a formatted message to a single client */
void send_msg(Client *c, const char *fmt, ...);

/* broadcast a message to all clients in a room except sender */
void broadcast_room(Server *s, Room *r, Client *sender, const char *msg);
