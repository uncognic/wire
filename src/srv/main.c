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

#include "server.h"
#include "wire.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char** argv)
{
    unsigned short port = PORT_DEFAULT;
    int opt;

    while ((opt = getopt(argc, argv, "p:")) != -1)
    {
        switch (opt)
        {
            case 'p':
            {
                long n = strtol(optarg, NULL, 10);
                if (n <= 0 || n > 65535)
                {
                    fprintf(stderr, "wire: invalid port '%s'\n", optarg);
                    return 1;
                }
                port = (unsigned short)n;
                break;
            }
            default:
                fprintf(stderr, "usage: %s [-p port]\n", argv[0]);
                return 1;
        }
    }

    Server s = {0};

    if (server_init(&s, port) < 0)
    {
        fprintf(stderr, "wire: failed to start server\n");
        return 1;
    }

    printf("wire: listening on port %d\n", port);
    server_run(&s);
    return 0;
}
