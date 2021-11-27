/**
* @file tcp.h
* @brief header for tcp protocol stack
*/
#ifndef TRANSPORT_TCP_H
#define TRANSPORT_TCP_H

#include <netinet/tcp.h>
#include <network/ip.h>
#include <transport/ring_buffer.h>

#include <pthread.h>

#define SOCKET_TYPE_CONNECTION 0
#define SOCKET_TYPE_DATA 1

#define SOCKET_UNCONNECTED 0
#define SOCKET_CLOSE 0
#define SOCKET_BINDED 1
#define SOCKET_LISTEN 2
#define SOCKET_SYN_SENT 3
#define SOCKET_SYN_RECV 4
#define SOCKET_ESTABLISHED 5

#define SOCKET_FIN_WAIT1 1
#define SOCKET_FIN_WAIT2 2
#define SOCKET_TIME_WAIT 3
#define SOCKET_CLOSE_WAIT 4
#define SOCKET_LAST_ACK 5

#define MAX_SEGMENT_LENGTH 1460

struct data_socket_t{
    int data_socket_id;
    struct data_socket_t *prev;
    struct data_socket_t *next;
};

struct port_info_t{
    int connect_socket_id;
    struct data_socket_t *data_socket_ids;
};

int insert_data_socket(struct port_info_t * port_info, int socket_id);

int delete_data_socket(struct port_info_t * port_info, int socket_id);


struct device_port_info_t{
    uint32_t s_addr;
    struct port_info_t ports[MAX_PORT_NUM];
};

struct  waiting_connection_t{
    uint32_t d_addr;
    uint16_t d_port;
    struct waiting_connection_t *prev;
    struct waiting_connection_t *next;
};

struct socket_info_t{
    int valid;
    int type;
    int state;
    int sock_id;
    int device_id;
    uint32_t s_addr;
    uint16_t s_port;
    uint32_t d_addr;
    uint16_t d_port;
    struct ring_buffer_t *input_buffer;
    struct ring_buffer_t *output_buffer;
    uint32_t snd_una, snd_nxt, iss;
    uint32_t rcv_nxt, irs;
    int (*callback)(struct socket_info_t*);
    struct waiting_connection_t *first;
    struct waiting_connection_t *last;
    int waiting_connection_count;
    int backlog;
    FILE *write_file;
    int write_f, read_flag;
    int read_cnt, read_len, write_cnt, write_len, peer_window_size;
    pthread_t resend_pthread, fin_pthread;
    uint32_t timeout;
    int fin_state, fin_tag;
};

struct segment_t {
    void *data;
    size_t len;
    int seq;
};

int allocSegment(struct segment_t** dst, size_t len, int seq);

int freeSegment(struct segment_t** dst);

int insert_waiting_connection(struct socket_info_t *s, uint32_t d_addr, uint16_t d_port);
int delete_waiting_connection(struct socket_info_t *s, uint32_t d_addr, uint16_t d_port);

int sendTCPSegment(struct segment_t *seg, struct socket_info_t *sock, uint16_t ack, uint32_t ack_seq, uint16_t window);

int sendTCPControlSegment(struct socket_info_t *sock, uint16_t syn, uint32_t seq, uint16_t ack, uint32_t ack_seq, uint16_t fin, uint16_t window);

#endif