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

#include "protocol.h"
#include "room.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* send a message to a single client */
void send_msg(Client *c, const char *fmt, ...)
{
    char buf[MAX_LINE];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (n > 0)
    {
        size_t len = n < (int)sizeof(buf) ? (size_t)n : sizeof(buf) - 1;
        write(c->fd, buf, len);
    }
}

void send_motd(Client *c)
{
    FILE *f = fopen(".wire/motd", "r");
    if (!f)
    {
        return;
    }
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f))
    {
        send_msg(c, "%s", line);
    }
    fclose(f);
}

void broadcast_room(Server *s, Room *r, Client *sender, const char *msg)
{
    char buf[MAX_LINE];
    int n = snprintf(buf, sizeof(buf), "<%s> %s\r\n", sender->nick, msg);

    /* send message to each client */
    for (int i = 0; i < s->nclients; i++)
    {
        Client *c = &s->clients[i];
        if (c != sender && strcmp(c->room, r->name) == 0)
        {
            write(c->fd, buf, n);
        }
    }

    room_log(r, buf);
}

/* tokenise a /command line into argv-style array */
static int tokenise(const char *line, char args[MAX_ARGS][MAX_LINE])
{
    int argc = 0;
    const char *p = line + 1; /* skip / */

    while (*p && argc < MAX_ARGS)
    {
        while (*p == ' ')
        {
            p++;
        }
        if (!*p)
        {
            break;
        }
        int j = 0;
        while (*p && *p != ' ' && j < MAX_LINE - 1)
        {
            args[argc][j++] = *p++;
        }
        args[argc][j] = 0;
        argc++;
    }
    return argc;
}

void handle_line(Server *s, Client *c, const char *line)
{
    /* first message must be the nick */
    if (c->nick[0] == 0)
    {
        for (int i = 0; i < s->nclients; i++)
        {
            if (&s->clients[i] != c && strcmp(s->clients[i].nick, line) == 0)
            {
                send_msg(c, "nick taken\r\n");
                return;
            }
        }
        strncpy(c->nick, line, MAX_NICK - 1);
        send_msg(c, "ok, nick set to %s\r\n", c->nick);
        send_motd(c);
        return;
    }

    /* commands start with / */
    if (line[0] == '/')
    {
        char args[MAX_ARGS][MAX_LINE];
        int argc = tokenise(line, args);
        if (argc == 0)
        {
            return;
        }

        if (strcmp(args[0], "join") == 0)
        {
            if (argc < 2)
            {
                send_msg(c, "usage: /join <room>\r\n");
                return;
            }
            Room *r = room_find(s, args[1]);
            if (!r)
            {
                r = room_create(s, args[1], c->nick);
                c->is_op = 1; /* first joiner gets op */
            }
            strncpy(c->room, r->name, MAX_ROOM - 1);
            send_msg(c, "joined %s\r\n", r->name);
        }
        else if (strcmp(args[0], "ban") == 0)
        {
            if (argc < 2)
            {
                send_msg(c, "usage: /ban <nick>\r\n");
                return;
            }
            Room *r = room_find(s, c->room);
            if (!r || !c->is_op)
            {
                send_msg(c, "not op in this room\r\n");
                return;
            }
            room_ban(r, args[1]);
            send_msg(c, "banned %s\r\n", args[1]);
        }
        else if (strcmp(args[0], "motd") == 0)
        {
            send_motd(c);
        }
        else if (strcmp(args[0], "help") == 0)
        {
            send_msg(c, "commands: /join, /ban, /motd, /help\r\n");
        }
        else
        {
            send_msg(c, "unknown command: /%s\r\n", args[0]);
        }
        return;
    }

    /* plain text, send to current room */
    if (c->room[0] == 0)
    {
        send_msg(c, "join a room first: /join <room>\r\n");
        return;
    }

    Room *r = room_find(s, c->room);
    if (!r)
    {
        send_msg(c, "not in a room\r\n");
        return;
    }
    if (room_is_banned(r, c->nick))
    {
        send_msg(c, "you are banned from this room\r\n");
        return;
    }

    broadcast_room(s, r, c, line);
}
