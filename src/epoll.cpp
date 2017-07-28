#include "epoll.h"
#include "debug.h"

struct epoll_event *events;  

int http_epoll_create(int flags) //flags == 0
{
    int fd = epoll_create1(flags); //flag == 0时，与epoll_create()相同
    check(fd > 0, "http_epoll_create: epoll_create1");

    events = (struct epoll_event *)malloc(sizeof(struct epoll_event) * MAX_EVENTS);  
    check(events != NULL, "http_epoll_create: malloc");
    return fd;
}

void http_epoll_add(int epfd, int fd, bool oneshot)
{
    epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
	if(oneshot)
		event.events |= EPOLLONESHOT;
    int rc = epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event);
    check(rc == 0, "http_epoll_add: epoll_ctl");
    return;
}

void http_epoll_mod(int epfd, int fd, int ev)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLET | EPOLLRDHUP | EPOLLONESHOT;
    int rc = epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &event);
    check(rc == 0, "http_epoll_mod: epoll_ctl");
    return;
}

void http_epoll_del(int epfd, int fd)
{
    int rc = epoll_ctl(epfd, EPOLL_CTL_DEL, fd, 0);
    check(rc == 0, "http_epoll_del: epoll_ctl");
    return;
}

int http_epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout)
{
    int n = epoll_wait(epfd, events, maxevents, timeout);  
    check(n >= 0, "http_epoll_wait: epoll_wait");
    return n;
}
