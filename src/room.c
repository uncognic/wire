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

#define _POSIX_C_SOURCE 200809L
#include "room.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* create a directory, silently ignore if it already exists */
static void ensure_dir(const char *path)
{
    char buf[512];
    snprintf(buf, sizeof(buf), "mkdir -p %s 2>/dev/null", path);
    system(buf);
}

Room *room_create(Server *s, const char *name, const char *creator)
{
    /* grow the rooms array */
    s->rooms = realloc(s->rooms, (s->nrooms + 1) * sizeof(Room));
    Room *r = &s->rooms[s->nrooms++];

    strncpy(r->name, name, MAX_ROOM - 1);

    /* create state files */
    char buf[512];
    snprintf(buf, sizeof(buf), ".wire/rooms/%s", name);
    r->path = strdup(buf);

    ensure_dir(buf);

    snprintf(buf, sizeof(buf), ".wire/rooms/%s/ops", name);
    r->ops = strdup(buf);
    FILE *f = fopen(r->ops, "a");
    if (f)
    {
        fprintf(f, "%s\n", creator);
        fclose(f);
    }

    snprintf(buf, sizeof(buf), ".wire/rooms/%s/bans", name);
    r->bans = strdup(buf);
    ensure_dir(".wire/rooms");

    return r;
}

Room *room_find(Server *s, const char *name)
{
    for (int i = 0; i < s->nrooms; i++)
    {
        if (strcmp(s->rooms[i].name, name) == 0)
        {
            return &s->rooms[i];
        }
    }
    return NULL;
}

int room_is_banned(Room *r, const char *nick)
{
    FILE *f = fopen(r->bans, "r");
    if (!f)
    {
        return 0;
    }

    char line[128];
    while (fgets(line, sizeof(line), f))
    {
        char *nl = strchr(line, '\n');
        if (nl)
        {
            *nl = 0;
        }
        if (strcmp(line, nick) == 0)
        {
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

void room_add_op(Room *r, const char *nick)
{
    FILE *f = fopen(r->ops, "a");
    if (f)
    {
        fprintf(f, "%s\n", nick);
        fclose(f);
    }
}

/* rewrite ops file omitting the given nick */
void room_remove_op(Room *r, const char *nick)
{
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s.tmp", r->ops);
    FILE *in = fopen(r->ops, "r");
    FILE *out = fopen(tmp, "w");
    if (!in || !out)
    {
        if (in)
        {
            fclose(in);
        }
        if (out)
        {
            fclose(out);
        }
        return;
    }
    char line[128];
    while (fgets(line, sizeof(line), in))
    {
        char *nl = strchr(line, '\n');
        if (nl)
        {
            *nl = 0;
        }
        if (strcmp(line, nick) != 0)
        {
            fprintf(out, "%s\n", line);
        }
    }
    fclose(in);
    fclose(out);
    rename(tmp, r->ops);
}

void room_ban(Room *r, const char *nick)
{
    FILE *f = fopen(r->bans, "a");
    if (f)
    {
        fprintf(f, "%s\n", nick);
        fclose(f);
    }
}

/* rewrite bans file omitting the given nick */
void room_unban(Room *r, const char *nick)
{
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s.tmp", r->bans);
    FILE *in = fopen(r->bans, "r");
    FILE *out = fopen(tmp, "w");
    if (!in || !out)
    {
        if (in)
        {
            fclose(in);
        }
        if (out)
        {
            fclose(out);
        }
        return;
    }
    char line[128];
    while (fgets(line, sizeof(line), in))
    {
        char *nl = strchr(line, '\n');
        if (nl)
        {
            *nl = 0;
        }
        if (strcmp(line, nick) != 0)
        {
            fprintf(out, "%s\n", line);
        }
    }
    fclose(in);
    fclose(out);
    rename(tmp, r->bans);
}

/* append a line to the room's log file */
void room_log(Room *r, const char *line)
{
    char buf[512];
    snprintf(buf, sizeof(buf), "%s/log", r->path);
    FILE *f = fopen(buf, "a");
    if (f)
    {
        fprintf(f, "%s", line);
        fclose(f);
    }
}
