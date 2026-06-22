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

#include <stddef.h>
#include <stdint.h>

/* protocol limits */
#define MAX_LINE 4096   /* max line length a client can send */
#define MAX_NICK 64     /* max nickname length */
#define MAX_ROOM 256    /* max room name length */
#define MAX_CLIENTS 128 /* max simultaneous connections */
#define MAX_ARGS 16     /* max /command arguments */

#define PORT_DEFAULT 7070 /* default listen port */

typedef struct Room Room;

/* per-client state tracked by the server */
typedef struct
{
    int fd;              /* client socket */
    char nick[MAX_NICK]; /* nickname, empty = not set yet */
    char room[MAX_ROOM]; /* current room, empty = nowhere */
    int is_op;           /* operator in current room */
    char buf[MAX_LINE];  /* partial line buffer */
    int buflen;          /* bytes used in buf */
} Client;

typedef struct
{
    Client clients[MAX_CLIENTS]; /* connected clients */
    int nclients;                /* number of connected clients */
    Room *rooms;                 /* dynamic array of rooms */
    int nrooms;                  /* number of rooms */
    int listen_fd;               /* server socket */
} Server;
