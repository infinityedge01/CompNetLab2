/**
* @file socket.h
* @brief POSIX- compatible socket library supporting TCP protocol on
IPv4.
*/
#ifndef TRANSPORT_SOCKET_H
#define TRANSPORT_SOCKET_H
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

#define MAX_PORT_NUM 65536

#define OPT_SOCKET 0
#define OPT_BIND 1
#define OPT_LISTEN 2
#define OPT_CONNECT 3
#define OPT_ACCEPT 4
#define OPT_READ 5
#define OPT_WRITE 6
#define OPT_CLOSE 7




extern char socket_pipe_dir[];
extern char socket_pipe_request[];
extern char socket_pipe_response[];
extern char socket_pipe_read[];
extern char socket_pipe_write[];
extern char ns_name[256];
extern int socket_id_list[MAX_PORT_NUM];

int set_ns_name(char * name);

int alloc_socket_id(int val);

int get_socket_val(int socket_id);

int free_socket_id(int socket_id);

/**
* @see [POSIX.1-2017:socket](http://pubs.opengroup.org/onlinepubs/
* 9699919799/functions/socket.html)
*/
int __wrap_socket(int domain, int type, int protocol);
/**
* @see [POSIX.1-2017:bind](http://pubs.opengroup.org/onlinepubs/
* 9699919799/functions/bind.html)
*/
int __wrap_bind(int socket, const struct sockaddr * address ,
socklen_t address_len);
/**
* @see [POSIX.1-2017:listen](http://pubs.opengroup.org/onlinepubs/
* 9699919799/functions/listen.html)
*/
int __wrap_listen(int socket, int backlog);
/**
* @see [POSIX.1-2017:connect](http://pubs.opengroup.org/onlinepubs/
* 9699919799/functions/connect.html)
*/
int __wrap_connect(int socket, const struct sockaddr * address ,
socklen_t address_len);
/**
* @see [POSIX.1-2017:accept](http://pubs.opengroup.org/onlinepubs/
* 9699919799/functions/accept.html)
*/
int __wrap_accept(int socket, struct sockaddr * address ,
socklen_t * address_len);
/**
* @see [POSIX.1-2017:read](http://pubs.opengroup.org/onlinepubs/
* 9699919799/functions/read.html)
*/
ssize_t __wrap_read(int fildes, void *buf, size_t nbyte);
/**
* @see [POSIX.1-2017:write](http://pubs.opengroup.org/onlinepubs/
* 9699919799/functions/write.html)
*/
ssize_t __wrap_write(int fildes, const void *buf, size_t nbyte);
/**
* @see [POSIX.1-2017:close](http://pubs.opengroup.org/onlinepubs/
* 9699919799/functions/close.html)
*/
int __wrap_close(int fildes);
/**
* @see [POSIX.1-2017:getaddrinfo](http://pubs.opengroup.org/
onlinepubs/
* 9699919799/functions/getaddrinfo.html)
*/
int __wrap_getaddrinfo(const char *node, const char * service, const struct addrinfo *hints, struct addrinfo ** res);

#endif