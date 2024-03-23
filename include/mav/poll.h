#ifndef __POLL_H
#define __POLL_H

#include <io.h>
#include <winsock.h>

#include <cstdint>
#define POLLIN 0x0001
#define POLLPRI 0x0002 /* not used */
#define POLLOUT 0x0004
#define POLLERR 0x0008
#define POLLHUP 0x0010 /* not used */
#define POLLNVAL 0x0020 /* not used */

struct pollfd {
int fd;
int events = 0;  /* in param: what to poll for */
int revents = 0; /* out param: what events occured */
};

int poll (struct pollfd *p, int num, int timeout) 
{
    struct timeval tv;
    fd_set read, write, except;
    int i, n, ret;

    FD_ZERO(&read);
    FD_ZERO(&write);
    FD_ZERO(&except);

    n = -1;
    for (i = 0; i < num; i++) {
        if (p[i].fd < 0) continue;
        if (p[i].events & POLLIN) FD_SET(p[i].fd, &read);
        if (p[i].events & POLLOUT) FD_SET(p[i].fd, &write);
        if (p[i].events & POLLERR) FD_SET(p[i].fd, &except);
        if (p[i].fd > n) n = p[i].fd;
    }

    if (n == -1) return (0);

    if (timeout < 0)
        ret = select(n + 1, &read, &write, &except, NULL);
    else {
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = 1000 * (timeout % 1000);
        ret = select(n + 1, &read, &write, &except, &tv);
    }

    for (i = 0; ret >= 0 && i < num; i++) {
        p[i].revents = 0;
        if (FD_ISSET(p[i].fd, &read)) p[i].revents |= POLLIN;
        if (FD_ISSET(p[i].fd, &write)) p[i].revents |= POLLOUT;
        if (FD_ISSET(p[i].fd, &except)) p[i].revents |= POLLERR;
    }
    return (ret);
};

int read(int one, uint8_t two, int three) {
    return 0;
}

#endif /* __POLL_H */
