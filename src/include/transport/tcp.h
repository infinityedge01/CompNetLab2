/**
* @file tcp.h
* @brief header for tcp protocol stack
*/
#ifndef TRANSPORT_TCP_H
#define TRANSPORT_TCP_H

#include <network/ip.h>

#define SOCKET_TYPE_CONNECTION 0
#define SOCKET_TYPE_DATA 1

#define SOCKET_UNCONNECTED 0
#define SOCKET_BINDED 1
#define SOCKET_LISTEN 2
#define SOCKET_SYN_SENT 3
#define SOCKET_SYN_RECV 4
#define SOCKET_ESTABLISHED 5

struct port_info{
    int connect_socket_id;
};

struct device_port_info{
    uint32_t s_addr;
    struct port_info ports[MAX_PORT_NUM];
};

struct socket_info_t{
    int valid;
    int type;
    int state;
    int device_id;
    uint32_t s_addr;
    uint16_t s_port;
    uint32_t t_addr;
    uint16_t t_port;
};



#endif