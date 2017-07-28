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
#include <pthread.h>
#include <iostream>
#include <vector>

#include "epoll.h"
#include "error.h"
#include "debug.h"
#include "util.h"
#include "threadpool.h"
#include "timer.h"
#include "http.h"

#define MAX_FD 32  //
#define MAX_EVENT_NUMBER 10000
#define CONF "http.conf"
#define PROGRAM_VERSION "0.1"

using std::vector;
extern struct epoll_event *events;

static void usage()
{
	fprintf(stderr,
		"httpserver [option]...\n"
		"  -c|--conf <config file>	Specify config file. Default ./http.conf.\n"
		"  -?|-h|--help 			Help information.\n"
		"  -V|--version 			Display program version.\n"
		);
}

static const struct option long_options[]=
{
    {"help",no_argument,NULL,'?'},
    {"version",no_argument,NULL,'V'},
    {"conf",required_argument,NULL,'c'},
    {NULL,0,NULL,0}
};

void http_close_conn()
{

}


int main( int argc, char* argv[] )
{
	char *config = CONF;
	int rc, opt = 0, options_index = 0;
    if( argc == 1 )
    {
		usage();
		exit(1);
    }
    while ((opt=getopt_long(argc, argv,"Vc:?h",long_options,&options_index)) != EOF)
	{
	    switch (opt)
		{
	        case  0 : break;
	        case 'c':
	            config = optarg;
	            break;
	        case 'V':
	            printf(PROGRAM_VERSION "\n");
	            return 0;
	        case ':':
	        case 'h':
	        case '?':
	            usage();
	            return 0;
	    }
    }
	debug("Config file: %s", config);
    if (optind < argc) {
        log_err("non-option argv-elements: ");
        while (optind < argc)
            log_err("%s ", argv[optind++]);
        return 0;
    }

    char conf_buf[BUFSIZE];
    http_conf_t cf;
    rc = read_conf(config, &cf, conf_buf, BUFSIZE);
    check(rc == CONFIG_OK, "read conf err");

    addsig( SIGPIPE, SIG_IGN, true );

    threadpool< http_conn >* pool = NULL; //创建线程池，处理http请求
    try
    {
        pool = new threadpool< http_conn >;
    }
    catch( ... )
    {
        return -1;
    }

    //http_conn *users = new http_conn[ MAX_FD ];
    //assert( users );

    int listenfd;
    struct sockaddr_in clientaddr;
    socklen_t inlen = 1;
    memset(&clientaddr, 0, sizeof(struct sockaddr_in));
    listenfd = open_listenfd(cf.port);
    assert( listenfd >= 0 );

    int epollfd = http_epoll_create(0);  //内核事件表
    assert( epollfd != -1 );

    http_epoll_add( epollfd, listenfd, false );  //将listenfd设置为非阻塞IO,并注册事件到epollfd
	rc = make_socket_non_blocking(listenfd);
	check(rc == 0, "make_socket_non_blocking");
    http_conn::m_epollfd = epollfd;
    vector<http_conn> users;

	time_heap *http_timer = new time_heap(); //

	while( true )
    {
        int msec = http_timer->get_top_msec();
        int number = epoll_wait( epollfd, events, MAX_EVENT_NUMBER, msec );
        if ( ( number < 0 ) && ( errno != EINTR ) )
        {
            printf( "epoll failure\n" );
            break;
        }
        http_timer->tick();  //handler expire timers
        for ( int i = 0; i < number; i++ )
        {
            int sockfd = events[i].data.fd;
            if( sockfd == listenfd )  //客户端新连接请求
            {
                int connfd;
                while(1)
                {
                    connfd = accept( listenfd, ( struct sockaddr* )&clientaddr, &inlen );
                    if(connfd < 0)
                    {
                        if((errno == EAGAIN) || (errno == EWOULDBLOCK))
                            break;
                        else
                        {
                            log_err("accept");
                            break;
                        }
                        if( http_conn::m_user_count >= MAX_FD )
                        {
                            show_error( connfd, "Internal server busy" );
                            continue;
                        }
                        http_conn user;  //
                        user.init( connfd, clientaddr );
                        users.push_back(user);

                        struct timer_node timer(TIMEOUT_DEFAULT, http_close_conn);
                        http_timer->add_timer(timer);
                    }
                }
            }
            else if( events[i].events & ( EPOLLRDHUP | EPOLLHUP | EPOLLERR ) )  //本端出错会触发EPOLLERR和EPOLLHUP
            {
                log_err("epoll error fd: %d", sockfd);
                close(sockfd);
                continue;
            }

            else if( events[i].events & EPOLLIN )  //有数据可读
            {
                if( users[sockfd].read() )  //主线程读出数据，然后往请求队列中添加任务，唤醒等待线程，进行处理
                {
                    pool->append( &users[sockfd] );
                }
                else
                {
                    users[sockfd].close_conn();
                }
            }
            else if( events[i].events & EPOLLOUT )  //有数据可写
            {
                if( !users[sockfd].write() )
                {
                    users[sockfd].close_conn();
                }
            }
            else
            {}
        }
    }

    close( epollfd );
    close( listenfd );
    delete http_timer;
    delete pool;
    return 0;
}

