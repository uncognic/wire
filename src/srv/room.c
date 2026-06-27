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
#include <sys/stat.h>
#include <time.h>

static void mkpath(const char *path, mode_t mode)
{
    char tmp[512];
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = 0;

    for (char *p = tmp + 1; *p; p++)
    {
        if (*p == '/')
        {
            *p = 0;
            mkdir(tmp, mode);
            *p = '/';
        }
    }
    mkdir(tmp, mode);
}

/* load topic from disk, returns allocated string or NULL */
static char *load_topic(const char *path)
{
    char tpath[512];
    snprintf(tpath, sizeof(tpath), "%s/topic", path);
    FILE *f = fopen(tpath, "r");
    if (!f)
        return NULL;

    char line[MAX_LINE];
    if (fgets(line, sizeof(line), f))
    {
        char *nl = strchr(line, '\n');
        if (nl) *nl = 0;
        fclose(f);
        return strdup(line);
    }
    fclose(f);
    return NULL;
}

Room *room_create(Server *s, const char *name, const char *creator)
{
    Room *tmp = realloc(s->rooms, (s->nrooms + 1) * sizeof(Room));
    if (!tmp)
        return NULL;
    s->rooms = tmp;
    Room *r = &s->rooms[s->nrooms++];
    memset(r, 0, sizeof(*r));

    strncpy(r->name, name, MAX_ROOM - 1);

    char buf[512];
    snprintf(buf, sizeof(buf), "%s/rooms/%s", s->data_dir, name);
    r->path = strdup(buf);
    mkpath(buf, 0755);

    snprintf(buf, sizeof(buf), "%s/rooms/%s/ops", s->data_dir, name);
    r->ops = strdup(buf);
    FILE *f = fopen(r->ops, "a");
    if (f)
    {
        fprintf(f, "%s\n", creator);
        fclose(f);
    }

    snprintf(buf, sizeof(buf), "%s/rooms/%s/bans", s->data_dir, name);
    r->bans = strdup(buf);

    r->topic = load_topic(r->path);

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

int valid_name(const char *name)
{
    if (!name || !name[0])
        return 0;
    for (const char *p = name; *p; p++)
    {
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
              (*p >= '0' && *p <= '9') || *p == '-' || *p == '_'))
            return 0;
    }
    return 1;
}

int room_is_op(Room *r, const char *nick)
{
    FILE *f = fopen(r->ops, "r");
    if (!f)
        return 0;
    char line[128];
    while (fgets(line, sizeof(line), f))
    {
        char *nl = strchr(line, '\n');
        if (nl)
            *nl = 0;
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

/* append a timestamped line to the room's log file */
void room_log(Room *r, const char *line)
{
    char buf[512];
    snprintf(buf, sizeof(buf), "%s/log", r->path);
    FILE *f = fopen(buf, "a");
    if (!f)
        return;

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char ts[64];
    strftime(ts, sizeof(ts), "[%Y-%m-%d %H:%M:%S] ", tm);

    fprintf(f, "%s%s", ts, line);
    fclose(f);
}

void room_set_topic(Room *r, const char *text)
{
    char tpath[512];
    snprintf(tpath, sizeof(tpath), "%s/topic", r->path);
    FILE *f = fopen(tpath, "w");
    if (f)
    {
        fprintf(f, "%s\n", text);
        fclose(f);
    }

    free(r->topic);
    r->topic = strdup(text);
}
