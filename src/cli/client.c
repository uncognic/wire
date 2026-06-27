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
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <termios.h>
#include <unistd.h>

/* lines to keep in scrollback */
#define MAX_LINES 1024
#define MAX_LINE 4096
#define RBUF_SIZE 4096
#define PORT_DEFAULT 7070
#define SCROLL_ROWS_OFFSET 3

/* ANSI escape sequences */
#define ANSI_HOME_CLR "\033[H\033[J"
#define ANSI_CLR_EOL "\033[K"

static const char *host;
static int port;
static int sockfd = -1;
static struct termios saved_term;

/* scrollback ring buffer */
static char *lines[MAX_LINES];
static int line_count;
static int scroll_pos;

/* input */
static char input[MAX_LINE];
static int input_len;

/* terminal dimensions */
static int rows = 24;
static int cols = 80;

static char current_room[256] = "";
static char current_nick[64] = "";
static int is_op;

/* colors */
#define C_RESET "\033[0m"
#define C_GREEN "\033[32m"
#define C_YELLOW "\033[33m"
#define C_RED "\033[31m"
#define C_CYAN "\033[36m"
#define C_GRAY "\033[90m"
#define C_REV "\033[7m"

static void die(const char *msg)
{
    fprintf(stderr, "wirec: %s\n", msg);
    exit(1);
}

static void push_line(const char *line)
{
    if (line_count < MAX_LINES)
    {
        lines[line_count] = strdup(line);
        line_count++;
    }
    else
    {
        free(lines[0]);
        memmove(lines, lines + 1, (MAX_LINES - 1) * sizeof(char *));
        lines[MAX_LINES - 1] = strdup(line);
    }
    scroll_pos = line_count;
}

static void redraw(void)
{
    /* move cursor home and clear */
    write(STDOUT_FILENO, ANSI_HOME_CLR, sizeof(ANSI_HOME_CLR) - 1);

    /* calculate how many lines fit in the scroll area */
    int scroll_rows = rows - SCROLL_ROWS_OFFSET;
    int start = scroll_pos - scroll_rows;
    if (start < 0)
    {
        start = 0;
    }

    /* draw chat */
    for (int i = start; i < scroll_pos && i < line_count; i++)
    {
        char buf[MAX_LINE];
        strncpy(buf, lines[i], sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = 0;
        char *nl = strchr(buf, '\r');
        if (nl)
        {
            *nl = 0;
        }
        nl = strchr(buf, '\n');
        if (nl)
        {
            *nl = 0;
        }

        /* truncate to terminal width */
        if ((int)strlen(buf) > cols)
        {
            buf[cols] = 0;
        }

        write(STDOUT_FILENO, "\r", 1);
        write(STDOUT_FILENO, ANSI_CLR_EOL, sizeof(ANSI_CLR_EOL) - 1);
        if (buf[0])
        {
            write(STDOUT_FILENO, buf, strlen(buf));
        }
        write(STDOUT_FILENO, "\n", 1);
    }

    /* clear remaining scroll rows */
    for (int i = scroll_pos - start; i < scroll_rows; i++)
    {
        write(STDOUT_FILENO, "\r", 1);
        write(STDOUT_FILENO, ANSI_CLR_EOL, sizeof(ANSI_CLR_EOL) - 1);
        write(STDOUT_FILENO, "\n", 1);
    }

    /* status bar */
    write(STDOUT_FILENO, "\r", 1);
    write(STDOUT_FILENO, ANSI_CLR_EOL, sizeof(ANSI_CLR_EOL) - 1);
    char status[512];
    if (current_nick[0])
    {
        snprintf(status, sizeof(status), "%s @ %s%s", current_nick,
                 current_room[0] ? current_room : "(no room)", is_op ? " [op]" : "");
    }
    else
    {
        snprintf(status, sizeof(status), "not connected");
    }
    if ((int)strlen(status) > cols)
    {
        status[cols] = 0;
    }
    write(STDOUT_FILENO, C_REV, strlen(C_REV));
    write(STDOUT_FILENO, status, strlen(status));
    write(STDOUT_FILENO, C_RESET, strlen(C_RESET));
    write(STDOUT_FILENO, "\n", 1);

    /* prompt line */
    write(STDOUT_FILENO, "\r", 1);
    write(STDOUT_FILENO, ANSI_CLR_EOL, sizeof(ANSI_CLR_EOL) - 1);
    write(STDOUT_FILENO, "> ", 2);
    write(STDOUT_FILENO, input, input_len);
    write(STDOUT_FILENO, ANSI_CLR_EOL, sizeof(ANSI_CLR_EOL) - 1);
}

static void term_raw(void)
{
    struct termios raw;
    tcgetattr(STDIN_FILENO, &saved_term);
    raw = saved_term;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static void term_reset(void)
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_term);
}

static int tcp_connect(void)
{
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    struct addrinfo hints = {0}, *ai;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int err = getaddrinfo(host, port_str, &hints, &ai);
    if (err)
    {
        return -1;
    }

    int fd = -1;
    for (struct addrinfo *p = ai; p; p = p->ai_next)
    {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0)
        {
            continue;
        }
        if (connect(fd, p->ai_addr, p->ai_addrlen) == 0)
        {
            break;
        }
        close(fd);
        fd = -1;
    }

    freeaddrinfo(ai);
    return fd;
}

static void do_connect(void)
{
    sockfd = tcp_connect();
    if (sockfd < 0)
    {
        die("connection failed");
    }
}

static void handle_server(const char *line)
{
    if (strncmp(line, "ok, nick set to ", 16) == 0)
    {
        strncpy(current_nick, line + 16, sizeof(current_nick) - 1);
    }
    else if (strncmp(line, "joined ", 7) == 0)
    {
        strncpy(current_room, line + 7, sizeof(current_room) - 1);
        is_op = 0;
    }
    else if (strncmp(line, "you were kicked", 15) == 0)
    {
        current_room[0] = 0;
        is_op = 0;
    }
    else if (strncmp(line, "opped ", 6) == 0)
    {
        if (strcmp(line + 6, current_nick) == 0)
        {
            is_op = 1;
        }
    }
    else if (strncmp(line, "deopped ", 8) == 0)
    {
        if (strcmp(line + 8, current_nick) == 0)
        {
            is_op = 0;
        }
    }

    /* color messages depending on if they are system messages or not */
    if (line[0] == '<')
    {
        char buf[MAX_LINE];
        snprintf(buf, sizeof(buf), "%s%s%s", C_GREEN, line, C_RESET);
        push_line(buf);
    }
    else
    {
        char buf[MAX_LINE];
        snprintf(buf, sizeof(buf), "%s%s%s", C_YELLOW, line, C_RESET);
        push_line(buf);
    }
}

static int poll_loop(void)
{
    char nick_set = 0;

    /* get terminal size */
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0)
    {
        rows = ws.ws_row;
        cols = ws.ws_col;
    }

    term_raw();
    atexit(term_reset);

    /* read buf for server */
    char rbuf[RBUF_SIZE];
    int rlen = 0;

    for (;;)
    {
        struct pollfd fds[2];
        fds[0].fd = STDIN_FILENO;
        fds[0].events = POLLIN;
        fds[1].fd = sockfd;
        fds[1].events = POLLIN;

        int ret = poll(fds, 2, -1);
        if (ret < 0)
        {
            break;
        }

        /* stdin */
        if (fds[0].revents & POLLIN)
        {
            char ch;
            if (read(STDIN_FILENO, &ch, 1) != 1)
            {
                break;
            }

            if (ch == '\n' || ch == '\r')
            {
                if (input_len > 0)
                {
                    char buf[MAX_LINE];
                    if (!nick_set)
                    {
                        snprintf(buf, sizeof(buf), "%s>>> %s%s", C_CYAN, input, C_RESET);
                        nick_set = 1;
                    }
                    else
                    {
                        snprintf(buf, sizeof(buf), "%s<%s> %s%s", C_GREEN, "you", input, C_RESET);
                    }
                    push_line(buf);

                    write(sockfd, input, input_len);
                    write(sockfd, "\n", 1);
                    input_len = 0;
                }
                redraw();
            }
            else if (ch == 3)
            {
                /* ctrl c */
                break;
            }
            else if (ch == 127 || ch == '\b')
            {
                /* backspace */
                if (input_len > 0)
                {
                    input[--input_len] = 0;
                }
                redraw();
            }
            else if (ch >= 32 && ch < 127 && input_len < (int)sizeof(input) - 1)
            {
                input[input_len++] = ch;
                input[input_len] = 0;
                redraw();
            }
        }

        /* server data */
        if (fds[1].revents & (POLLIN | POLLHUP | POLLERR))
        {
            ssize_t n = read(sockfd, rbuf + rlen, sizeof(rbuf) - rlen - 1);
            if (n <= 0)
            {
                break;
            }

            rlen += (int)n;
            rbuf[rlen] = 0;

            int consumed = 0;
            for (int j = 0; j < rlen; j++)
            {
                if (rbuf[j] == '\n')
                {
                    rbuf[j] = 0;
                    if (j > 0 && rbuf[j - 1] == '\r')
                    {
                        rbuf[j - 1] = 0;
                    }

                    handle_server(rbuf + consumed);
                    consumed = j + 1;
                    redraw();
                }
            }

            if (consumed < rlen)
            {
                rlen -= consumed;
                memmove(rbuf, rbuf + consumed, rlen);
            }
            else
            {
                rlen = 0;
            }
        }
    }

    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "usage: wirec <host> [port]\n");
        return 1;
    }

    host = argv[1];
    if (argc > 2)
    {
        long n = strtol(argv[2], NULL, 10);
        if (n <= 0 || n > 65535)
        {
            fprintf(stderr, "wirec: invalid port '%s'\n", argv[2]);
            return 1;
        }
        port = (int)n;
    }
    else
    {
        port = PORT_DEFAULT;
    }

    do_connect();
    return poll_loop();
}
