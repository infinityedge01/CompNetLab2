#include <network/device.h>
#include <network/util.h>
#include <network/arp.h>
#include <network/routing.h>
#include <network/ip.h>


#include <link/packetio.h>
#include <link/util.h>

#include <transport/socket.h>
#include <transport/tcp.h>


#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>


int insert_data_socket(struct port_info_t * port_info, int socket_id){
    if(port_info->data_socket_ids == NULL){
        port_info->data_socket_ids = malloc(sizeof(struct data_socket_t));
        port_info->data_socket_ids->data_socket_id = socket_id;
        port_info->data_socket_ids->prev = NULL;
        port_info->data_socket_ids->next = NULL;
        return 0;
    }
    struct data_socket_t *p = port_info->data_socket_ids;
    if(p->data_socket_id == socket_id){
        return -1;
    }
    while(p->next != NULL){
        p = p->next;
        if(p->data_socket_id == socket_id){
            return -1;
        }
    }
    struct data_socket_t *q = malloc(sizeof(struct data_socket_t));
    q->data_socket_id = socket_id;
    q->prev = p;
    q->next = NULL;
    p->next = q;
}

int delete_data_socket(struct port_info_t * port_info, int socket_id){
    if(port_info->data_socket_ids->next == NULL){
        if(port_info->data_socket_ids->data_socket_id != socket_id){
            return -1;
        }
        free(port_info->data_socket_ids);
        port_info->data_socket_ids = NULL;
        return 0;
    }   
    struct data_socket_t *p = port_info->data_socket_ids;
    while(p != NULL){
        if(p->data_socket_id == socket_id){
            break;
        }
        p = p->next;
    }
    if(p == NULL){
        return -1;
    }
    if(p->prev != NULL){
        p->prev->next = p->next;
    }
    if(p->next != NULL){
        p->next->prev = p->prev;
    }
    free(p);
    return 0;
}

int insert_waiting_connection(struct socket_info_t *s, uint32_t d_addr, uint16_t d_port){
    if(s->first == NULL && s->last == NULL){
        s->first = malloc(sizeof(struct waiting_connection_t));
        s->last = s->first;
        s->first->d_addr = d_addr;
        s->first->d_port = d_port;
        s->first->prev = s->first->next = NULL;
        s->waiting_connection_count = 1;
        return 0;
    }
    if(s->waiting_connection_count >= s->backlog) return -1;
    struct waiting_connection_t *p = s->last;
    s->last = malloc(sizeof(struct waiting_connection_t));
    s->last->d_addr = d_addr;
    s->last->d_port = d_port;
    p->next = s->last;
    s->last->prev = p;
    s->last->next = NULL;
    s->waiting_connection_count ++;
    return 0;
}

int delete_waiting_connection(struct socket_info_t *s, uint32_t d_addr, uint16_t d_port){
    if(s->first == NULL && s->last == NULL){
        return -1;
    }
    struct waiting_connection_t *p = s->first;
    while(p != NULL){
        if(p->d_addr == d_addr && p->d_port == d_port){
            s->waiting_connection_count --;
            if(p == s->first && p == s->last){
                free(p);
                s->first = NULL;
                s->last = NULL;
                return 0;
            }
            if(p == s->first){
                s->first = p->next;
                p->next->prev = NULL;
                free(p);
                return 0;
            }
            if(p == s->last){
                s->last = p->prev;
                p->prev->next = NULL;
                free(p);
                return 0;
            }
            p->next->prev = p->prev;
            p->prev->next = p->next;
            free(p);
            return 0;
        }
        p = p->next;
    }
    return -1;
}


int allocSegment(struct segment_t** dst, size_t len, int seq){
    *dst = malloc(sizeof(struct segment_t));
    (*dst)->len = len;
    (*dst)->seq = seq;
    (*dst)->data = malloc(len);
    memset((*dst)->data, 0, len);
    return 0;
}

int freeSegment(struct segment_t** dst){
    free((*dst)->data);
    free(*dst);
    *dst = NULL;
}

uint16_t tcp_checksum(struct socket_info_t *sock, struct tcphdr *hdr, uint16_t len){
    unsigned char* buf = malloc((size_t)(len) + 12);
    memcpy(buf, &(sock->s_addr), 4);
    memcpy(buf + 4, &(sock->d_addr), 4);
    *(uint16_t *)(buf + 8) = htons(IPPROTO_TCP);
    *(uint16_t *)(buf + 10) = htons(len);
    memcpy(buf + 12, hdr, (size_t)(len));
    return checksum((uint16_t *)buf, (size_t)(len) + 12);
}
int sendTCPSegment(struct segment_t *seg, struct socket_info_t *sock, uint16_t ack, uint32_t ack_seq, uint16_t window){
    int ret;
    size_t send_len = sizeof(struct tcphdr) + seg->len;
    unsigned char *send_buf = malloc(send_len);
    memset(send_buf, 0, send_len);
    struct tcphdr *hdr = (struct tcphdr *)send_buf;

    hdr->source = htons(sock->s_port);
    hdr->dest = htons(sock->d_port);
    hdr->seq = htonl(seg->seq);
    hdr->doff = 5;
    
    hdr->ack = ack;
    hdr->ack_seq = htonl(ack_seq);
    hdr->window = htons(window);

    void *data = (void *)(send_buf + ((hdr->doff) << 2));
    memcpy(data, seg->data, seg->len);

    hdr->check = 0;
    hdr->check = tcp_checksum(sock, hdr, send_len);

    struct in_addr src, dst;
    src.s_addr = sock->s_addr;
    dst.s_addr = sock->d_addr;

    ret = sendIPPacket(src, dst, IPPROTO_TCP, send_buf, send_len);

    #ifdef DEBUG
    char src_ip[20], dst_ip[20];
    inet_ntop(AF_INET, &sock->s_addr, src_ip, 20);
    inet_ntop(AF_INET, &sock->d_addr, dst_ip, 20);
    printf("send tcp packet from %s:%d to %s:%d\n", src_ip, sock->s_port, dst_ip, sock->d_port);
    #endif // DEBUG

    free(send_buf);
    return ret;
}

int sendTCPControlSegment(struct socket_info_t *sock, uint16_t syn, uint32_t seq, uint16_t ack, uint32_t ack_seq, uint16_t fin, uint16_t window){
    int ret;
    size_t send_len = sizeof(struct tcphdr);
    uint8_t *send_buf = malloc(send_len);
    memset(send_buf, 0, send_len);
    struct tcphdr *hdr = (struct tcphdr *)send_buf;

    hdr->source = htons(sock->s_port);
    hdr->dest = htons(sock->d_port);
    hdr->syn = syn;
    hdr->seq = htonl(seq);
    hdr->ack = ack;
    hdr->ack_seq = htonl(ack_seq);
    hdr->fin = fin;
    hdr->doff = 5;
    hdr->window = htons(window);
    hdr->check = 0;
    hdr->check = tcp_checksum(sock, hdr, send_len);
    
    struct in_addr src, dst;
    src.s_addr = sock->s_addr;
    dst.s_addr = sock->d_addr;

    sendIPPacket(src, dst, IPPROTO_TCP, send_buf, send_len);

    #ifdef DEBUG
    char src_ip[20], dst_ip[20];
    inet_ntop(AF_INET, &sock->s_addr, src_ip, 20);
    inet_ntop(AF_INET, &sock->d_addr, dst_ip, 20);
    printf("send tcp packet from %s:%d to %s:%d SYN=%d seq=%d ACK=%d ACKseq=%d\n", src_ip, sock->s_port, dst_ip, sock->d_port, hdr->syn, htonl(hdr->seq), hdr->ack, htonl(hdr->ack_seq));
    #endif // DEBUG

    free(send_buf);
    return 0;
}