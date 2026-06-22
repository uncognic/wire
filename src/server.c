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

#include "server.h"
#include "protocol.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static volatile int running = 1;

static void handle_sigint(int sig)
{
    (void)sig;
    running = 0;
}

static int make_socket(void)
{
    int fd = socket(AF_INET6, SOCK_STREAM, 0);
    if (fd < 0)
    {
        return -1;
    }

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* accept both ipv4 and ipv6 connections on the same socket */
    opt = 0;
    setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt));

    struct sockaddr_in6 addr = {0};
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(PORT_DEFAULT);
    addr.sin6_addr = in6addr_any;

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        close(fd);
        return -1;
    }

    if (listen(fd, 16) < 0)
    {
        close(fd);
        return -1;
    }

    return fd;
}

/* remove client i by swapping in the last client */
static void drop_client(Server *s, int i)
{
    close(s->clients[i].fd);
    s->clients[i] = s->clients[--s->nclients];
}

void server_drop(Server *s, Client *c)
{
    int idx = (int)(c - s->clients);
    if (idx >= 0 && idx < s->nclients)
    {
        drop_client(s, idx);
    }
}

int server_init(Server *s)
{
    signal(SIGINT, handle_sigint);
    signal(SIGPIPE, SIG_IGN);

    s->listen_fd = make_socket();
    if (s->listen_fd < 0)
    {
        return -1;
    }

    return 0;
}

void server_run(Server *s)
{
    /* slot 0 = listen fd, slots 1..n = client fds */
    struct pollfd fds[MAX_CLIENTS + 1];

    while (running)
    {
        fds[0].fd = s->listen_fd;
        fds[0].events = POLLIN;

        for (int i = 0; i < s->nclients; i++)
        {
            fds[i + 1].fd = s->clients[i].fd;
            fds[i + 1].events = POLLIN;
        }

        int nfds = s->nclients + 1;
        int ret = poll(fds, nfds, 500);
        if (ret < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            break;
        }

        /* new connection */
        if (fds[0].revents & POLLIN)
        {
            int cfd = accept(s->listen_fd, NULL, NULL);
            if (cfd >= 0 && s->nclients < MAX_CLIENTS)
            {
                Client *c = &s->clients[s->nclients++];
                memset(c, 0, sizeof(*c));
                c->fd = cfd;
                send_msg(c, "connected to wire server\r\n");
                send_msg(c, "send a nick to start\r\n");
            }
            else if (cfd >= 0)
            {
                const char *msg = "server full\r\n";
                write(cfd, msg, strlen(msg));
                close(cfd);
            }
        }

        /* client I/O */
        for (int i = 0; i < s->nclients; i++)
        {
            if (fds[i + 1].revents & (POLLIN | POLLHUP | POLLERR))
            {
                char buf[MAX_LINE];
                ssize_t n = read(s->clients[i].fd, buf, sizeof(buf) - 1);
                if (n <= 0)
                {
                    drop_client(s, i--);
                }
                else
                {
                    buf[n] = 0;
                    char *nl;
                    if ((nl = strchr(buf, '\n')))
                    {
                        *nl = 0;
                    }
                    if ((nl = strchr(buf, '\r')))
                    {
                        *nl = 0;
                    }
                    if (buf[0])
                    {
                        handle_line(s, &s->clients[i], buf);
                    }
                }
            }
        }
    }

    server_shutdown(s);
}

void server_shutdown(Server *s)
{
    for (int i = 0; i < s->nclients; i++)
    {
        close(s->clients[i].fd);
    }
    close(s->listen_fd);
    printf("wire: shut down\n");
}
