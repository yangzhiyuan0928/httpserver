#ifndef EPOLL_H_
#define EPOLL_H_

#include <sys/epoll.h>

#define MAX_EVENTS 1024

int http_epoll_create(int flags);
void http_epoll_add(int epfd, int fd, bool oneshot);
void http_epoll_mod(int epfd, int fd, int ev);
void http_epoll_del(int epfd, int fd);
int http_epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);

#endif

