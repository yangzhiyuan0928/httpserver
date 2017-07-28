#ifndef UTIL_H
#define UTIL_H

#define LISTENQ     1024
#define BUFSIZE     4*1024  //4KiB
#define DELIM       "="

struct http_conf_t {
    void *root;
    int port;
    int thread_num;
};

int make_socket_non_blocking(int fd);
void addsig( int sig, void( handler )(int), bool restart );  //restart == true
void show_error( int connfd, const char* info );
int read_conf(char *filename, http_conf_t *cf, char *buf, int len);
int open_listenfd(int port);
int64_t get_currentmsec();

#endif
