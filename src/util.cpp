#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdint.h>
#include <getopt.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>
#include "util.h"
#include "debug.h"
#include "locker.h"
#include "epoll.h"
#include "error.h"
#include "debug.h"
#include "util.h"
#include "threadpool.h"
#include "timer.h"
#include "http.h"

int make_socket_non_blocking(int fd)
{
  int flags, s;
  flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
      log_err("fcntl");
      return -1;
  }
  flags |= O_NONBLOCK;
  s = fcntl(fd, F_SETFL, flags);
  if (s == -1) {
      log_err("fcntl");
      return -1;
  }
  return 0;
}

void addsig( int sig, void( handler )(int), bool restart = true )
{
    struct sigaction sa;
    memset( &sa, '\0', sizeof( sa ) );
    sa.sa_handler = handler;
    if( restart )
    {
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset( &sa.sa_mask );
    assert( sigaction( sig, &sa, NULL ) != -1 );
}

void show_error( int connfd, const char* info )
{
    printf( "%s", info );
    send( connfd, info, strlen( info ), 0 );
    close( connfd );
}

int read_conf(char *filename, http_conf_t *cf, char *buf, int len)
{
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        log_err("cannot open config file: %s", filename);
        return CONFIG_ERROR;
    }

    int pos = 0;
    char *delim_pos;
    int line_len;
    char *cur_pos = buf + pos;

    while (fgets(cur_pos, len-pos, fp)) {
        delim_pos = strstr(cur_pos, DELIM);
        line_len = strlen(cur_pos);

        if (!delim_pos)
            return CONFIG_ERROR;

        if (cur_pos[strlen(cur_pos) - 1] == '\n') {
            cur_pos[strlen(cur_pos) - 1] = '\0';
        }

        if (strncmp("root", cur_pos, 4) == 0) {
            cf->root = delim_pos + 1; //cf->root == "root"
        }

        if (strncmp("port", cur_pos, 4) == 0) {
            cf->port = atoi(delim_pos + 1); //cf->port == 3000
        }

        if (strncmp("threadnum", cur_pos, 9) == 0) {
            cf->thread_num = atoi(delim_pos + 1); //cf->thread_num == 4
        }

        cur_pos += line_len;
    }

    fclose(fp);
    return CONFIG_OK;
}

int open_listenfd(int port)
{
	if(port <= 0)
		port = 8080;
	int listenfd, optval;

	if((listenfd = socket(AF_INET, SOCK_STREAM,0)) < 0)
		return -1;
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int)) < 0)
  		return -1;
    struct linger tmp = { 1, 0 };
    if(setsockopt( listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof( tmp )) < 0 )
		return -1;
	struct sockaddr_in serveraddr;
    bzero( &serveraddr, sizeof( serveraddr ) );
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)port);
    if (bind(listenfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
	    return -1;
    if (listen(listenfd, LISTENQ) < 0)
	    return -1;

    return listenfd;
}

int64_t get_currentmsec()
{
    struct timeval cur_time;
    int rc = gettimeofday(&cur_time, NULL);
    check(rc == 0, "gettimeofday error");
    int64_t current_msec = cur_time.tv_sec * 1000 + cur_time.tv_usec/1000;  //ms
    return current_msec;
}

