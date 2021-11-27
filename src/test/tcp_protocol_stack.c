#include <network/device.h>
#include <network/util.h>
#include <network/arp.h>
#include <network/routing.h>
#include <network/ip.h>


#include <link/packetio.h>
#include <link/util.h>

#include <transport/socket.h>
#include <transport/tcp.h>
#include <transport/ring_buffer.h>

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>

int device_id[MAX_DEVICE];
int numdevice;
struct device_port_info_t *device_ports[MAX_DEVICE];
struct socket_info_t sockets[MAX_PORT_NUM];
pthread_mutex_t socket_mutex;

void alloc_socket(int socket_id, int type, int state){
    struct socket_info_t *s = &sockets[socket_id - MAX_PORT_NUM];
    memset(s, 0, sizeof(struct socket_info_t));
    s->valid = 1;
    s->type = type;
    s->state = state;
    s->sock_id = socket_id;
}


int alloc_port(int device_id, int socket_id){
    for(int i = 50000; i < MAX_PORT_NUM; i++){
        if(!device_ports[device_id]->ports[i].connect_socket_id){
            device_ports[device_id]->ports[i].connect_socket_id = socket_id;
            return i;
        }
    }
    return -1;
}

void free_socket(int socket_id, int from_timeout){
    
    struct socket_info_t *s = &sockets[socket_id - MAX_PORT_NUM];
    s->valid = 0;
    free_socket_id(socket_id);
    free_ring_buffer(&s->input_buffer);
    free_ring_buffer(&s->output_buffer);
    struct waiting_connection_t *p = s->first;
    if(p != NULL){
        while(p->next != NULL){
            p = p->next;
            free(p->prev);
        }
        free(p);
    }
    if(s->device_id != -1){
        if(device_ports[s->device_id]->ports[s->s_port].connect_socket_id == socket_id){
            device_ports[s->device_id]->ports[s->s_port].connect_socket_id = 0;
        }
        delete_data_socket(&(device_ports[s->device_id]->ports[s->s_port]), socket_id);
    }
    if(s->resend_pthread && !from_timeout){
        pthread_cancel(s->resend_pthread);
        pthread_join(s->resend_pthread, NULL);
    }
    memset(s, 0, sizeof(struct socket_info_t));
}

int find_device_by_ip(uint32_t addr){
    for(int i = 0; i < numdevice; i ++){
        if(device_ip_addr[i] == addr){
            return i;
        }
    }
    return -1;
}

int can_bind(struct socket_info_t *s) {
    if (!s->valid) {return -ENOTSOCK;}
    if (s->type != SOCKET_TYPE_CONNECTION) {return -EINVAL;}
    if (s->state != SOCKET_CLOSE) {return -EINVAL;}
    if (s->device_id < 0) {return -EADDRNOTAVAIL;}
    if (device_ports[s->device_id]->ports[s->s_port].connect_socket_id) {return -EADDRINUSE;}
    return 0;
}

int can_listen(struct socket_info_t *s) {
    if (!s->valid) {return -ENOTSOCK;}
    if (s->type != SOCKET_TYPE_CONNECTION) {return -EINVAL;}
    if (s->state != SOCKET_BINDED) {return -EINVAL;}
    return 0;
}

int can_connect(struct socket_info_t *s) {
    if (!s->valid) {return -ENOTSOCK;}
    if (s->type != SOCKET_TYPE_CONNECTION) {return -EINVAL;}
    if (s->state != SOCKET_CLOSE) {return -EINVAL;}
    if (s->device_id < 0) {return -EADDRNOTAVAIL;}
    return 0;
}

int can_accept(struct socket_info_t *s) {
    if (!s->valid) {return -ENOTSOCK;}
    if (s->type != SOCKET_TYPE_CONNECTION) {return -EINVAL;}
    if (s->state != SOCKET_LISTEN) {return -EINVAL;}
    if (s->device_id < 0) {return -EADDRNOTAVAIL;}
    return 0;
}

int can_read(struct socket_info_t *s) {
    if (!s->valid) {return -ENOTSOCK;}
    if (s->type != SOCKET_TYPE_DATA) {return -EINVAL;}
    if (s->state != SOCKET_ESTABLISHED) {return -EINVAL;}
    if (s->fin_state == SOCKET_TIME_WAIT || s->fin_state == SOCKET_LAST_ACK) {
        return -EINVAL;
    }
    if (s->device_id < 0) {return -EADDRNOTAVAIL;}
    return 0;
}

int can_write(struct socket_info_t *s) {
    if (!s->valid) {return -ENOTSOCK;}
    if (s->type != SOCKET_TYPE_DATA) {return -EINVAL;}
    if (s->state != SOCKET_ESTABLISHED) {return -EINVAL;}
    if (s->fin_state) {
        return -EINVAL;
    }
    if (s->device_id < 0) {return -EADDRNOTAVAIL;}
    return 0;
}

int can_close(struct socket_info_t *s) {
    if (!s->valid) {return -ENOTSOCK;}
    if (s->fin_state) {
        return -EINVAL;
    }
    return 0;
}

char request_pipe[256];
int request_f;
FILE *request_fd;

int request_routine();
void *TCPTimeout(void *arg);

pthread_t request_pthread;
void *request_routine_pthread(void *arg){
    request_routine();
}


void *freeSocket_pthread(void *arg){
    struct socket_info_t *s = (struct socket_info_t *)arg;
    pthread_detach(pthread_self());
    sleep(3);
    pthread_mutex_lock(&socket_mutex);
    free_socket(s->sock_id, 0);
    pthread_mutex_unlock(&socket_mutex);
}

int TCPConnectCallback(struct socket_info_t *s){
    char response_pipe[256];
    response_pipe[0] = '\0';
    strcat(response_pipe, socket_pipe_dir);
    strcat(response_pipe, socket_pipe_response);
    strcat(response_pipe, ns_name);
    sprintf(response_pipe + strlen(response_pipe), "%d", s->sock_id);
    int response_f = open(response_pipe, O_WRONLY);
    FILE * response_fd = fdopen(response_f, "w");
    fprintf(response_fd, "%d\n", 0);
    fflush(response_fd);
    close(response_f);
    s->callback = NULL;
}

int TCPAcceptCallback(struct socket_info_t *s){
    s->callback = NULL;
    char response_pipe[256];
    response_pipe[0] = '\0';
    strcat(response_pipe, socket_pipe_dir);
    strcat(response_pipe, socket_pipe_response);
    strcat(response_pipe, ns_name);
    sprintf(response_pipe + strlen(response_pipe), "%d", get_socket_val(s->sock_id));
    int response_f = open(response_pipe, O_WRONLY);
    FILE * response_fd = fdopen(response_f, "w");
    fprintf(response_fd, "%d %x %hu\n", s->sock_id, s->d_addr, s->d_port);
    fflush(response_fd);
    close(response_f);
}

int TCPAccept(struct socket_info_t *sock){
    uint32_t d_addr = sock->first->d_addr;
    uint32_t d_port = sock->first->d_port;
    delete_waiting_connection(sock, d_addr, d_port);
    int socket_id = alloc_socket_id(sock->sock_id);
    alloc_socket(socket_id, SOCKET_TYPE_DATA, SOCKET_SYN_RECV);
    insert_data_socket(&(device_ports[sock->device_id]->ports[sock->s_port]), socket_id);
    struct socket_info_t *s =  &sockets[socket_id - MAX_PORT_NUM];
    s->device_id = sock->device_id;
    s->s_addr = sock->s_addr;
    s->s_port = sock->s_port;
    s->d_addr = d_addr;
    s->d_port = d_port;
    alloc_ring_buffer(&(s->input_buffer));
    alloc_ring_buffer(&(s->output_buffer));    
    s->irs = sock->irs;
    s->rcv_nxt = sock->rcv_nxt;
    s->iss = 1;
    s->snd_una = 1;
    s->snd_nxt = 2;
    s->callback = TCPAcceptCallback;
    s->timeout = 0;
    s->reset_time = 0;
    sendTCPControlSegment(s, 1, s->iss, 1, s->rcv_nxt, 0, 0, get_buffer_free_size(s->input_buffer));
    pthread_create(&s->resend_pthread, NULL, &TCPTimeout, s);
}
int WaitTCPAccept(struct socket_info_t *sock){
    sock->callback = NULL;
    TCPAccept(sock);
}

int TCPFinCallback(struct socket_info_t *s){
    s->fin_tag = 0;
    char response_pipe[256];
    response_pipe[0] = '\0';
    strcat(response_pipe, socket_pipe_dir);
    strcat(response_pipe, socket_pipe_response);
    strcat(response_pipe, ns_name);
    sprintf(response_pipe + strlen(response_pipe), "%d", s->sock_id);
    int response_f = open(response_pipe, O_WRONLY);
    FILE * response_fd = fdopen(response_f, "w");
    fprintf(response_fd, "%d\n", 0);
    fflush(response_fd);
    close(response_f);
}

int TCPWriteCallback(struct socket_info_t *s){
    char response_pipe[256];
    response_pipe[0] = '\0';
    strcat(response_pipe, socket_pipe_dir);
    strcat(response_pipe, socket_pipe_response);
    strcat(response_pipe, ns_name);
    sprintf(response_pipe + strlen(response_pipe), "%d", s->sock_id);
    int response_f = open(response_pipe, O_WRONLY);
    FILE * response_fd = fdopen(response_f, "w");
    fprintf(response_fd, "%d\n", s->write_cnt);
    close(s->write_f);
    s->write_file = NULL;
    fflush(response_fd);
    close(response_f);
}

int TCPFailCallback(struct socket_info_t *s, int ret){
    char response_pipe[256];
    response_pipe[0] = '\0';
    strcat(response_pipe, socket_pipe_dir);
    strcat(response_pipe, socket_pipe_response);
    strcat(response_pipe, ns_name);
    sprintf(response_pipe + strlen(response_pipe), "%d", s->sock_id);
    int response_f = open(response_pipe, O_WRONLY);
    FILE * response_fd = fdopen(response_f, "w");
    fprintf(response_fd, "%d\n", ret);
    fflush(response_fd);
    close(response_f);
}

ssize_t processWrite(struct socket_info_t *s){
    if(s->write_file != NULL){
        ssize_t push_len = s->write_len - s->write_cnt;
        ssize_t free_size = get_buffer_free_size(s->output_buffer);
        if(free_size < push_len){
            push_len = free_size;
        }
        unsigned char* buf = malloc(push_len);
        ssize_t read_len = fread(buf, 1, push_len, s->write_file);
        push_ring_buffer(s->output_buffer, buf, push_len);
        s->write_cnt += push_len;
        return push_len;
    }
    return 0;
}

ssize_t processSend(struct socket_info_t *s){
    ssize_t window_len = get_buffer_size(s->output_buffer);
    if(window_len > s->peer_window_size){
        window_len = s->peer_window_size;
    }
    uint32_t send_end = s->snd_una + window_len;
    ssize_t write_len = send_end - s->snd_nxt;
    while(s->snd_nxt < send_end) {
        ssize_t offset = s->snd_nxt - s->snd_una;
        ssize_t segment_len = MAX_SEGMENT_LENGTH;
        if(send_end - s->snd_nxt <= segment_len){
            segment_len = send_end - s->snd_nxt;
        }
        struct segment_t* seg;
        allocSegment(&seg, segment_len, s->snd_nxt);
        get_ring_buffer(s->output_buffer, seg->data, offset, segment_len);
        sendTCPSegment(seg, s, 1, s->rcv_nxt, get_buffer_free_size(s->input_buffer));
        freeSegment(&seg);
        s->snd_nxt += segment_len;
    }
    return write_len;
}

int processWriteSend(struct socket_info_t *s){
    processSend(s);
    while(processWrite(s) > 0){
        processSend(s);
    }
    if(s->write_file != NULL && s->write_cnt == s->write_len){
        TCPWriteCallback(s);
    }
    //printf("%ld\n", get_buffer_size(s->output_buffer));
    if(get_buffer_size(s->output_buffer) == 0 && s->fin_tag){
        s->fin_state = SOCKET_FIN_WAIT1;
        TCPFinCallback(s);
        sendTCPControlSegment(s, 0, s->snd_nxt, 1, s->rcv_nxt, 1, 0, get_buffer_free_size(s->input_buffer));
    }
}

int TCPReadCallback(struct socket_info_t *s){
    char response_pipe[256];
    response_pipe[0] = '\0';
    strcat(response_pipe, socket_pipe_dir);
    strcat(response_pipe, socket_pipe_response);
    strcat(response_pipe, ns_name);
    sprintf(response_pipe + strlen(response_pipe), "%d", s->sock_id);
    int response_f = open(response_pipe, O_WRONLY);
    FILE * response_fd = fdopen(response_f, "w");
    fprintf(response_fd, "%d\n", s->read_cnt);
    fflush(response_fd);
    close(response_f);
}

int processReadWithoutAck(struct socket_info_t *s){
    //printf("read_flag: %d\n", s->read_flag);
    if(s->read_flag == 1){
        ssize_t read_size = s->read_len - s->read_cnt;
        ssize_t buffer_size = get_buffer_size(s->input_buffer);
        if(buffer_size < read_size){
            read_size = buffer_size;
        }
        if(read_size > 0){
            s->read_cnt += read_size;
            TCPReadCallback(s);
            unsigned char* buf = malloc(read_size);
            consume_ring_buffer(s->input_buffer, buf, read_size);
            char read_pipe[256];
            read_pipe[0] = '\0'; 
            strcat(read_pipe, socket_pipe_dir);
            strcat(read_pipe, socket_pipe_read);
            strcat(read_pipe, ns_name);
            sprintf(read_pipe + strlen(read_pipe), "%d", s->sock_id);
            int rw_f = open(read_pipe, O_WRONLY);
            FILE *rw_fd = fdopen(rw_f, "w");
            fwrite(buf, read_size, 1, rw_fd);
            fflush(rw_fd);
            close(rw_f);
            s->read_flag = 0;
        }
    }
}
int processRead(struct socket_info_t *s){
    //printf("%d\n", s->read_flag);
    processReadWithoutAck(s);
    sendTCPControlSegment(s, 0, s->snd_nxt, 1, s->rcv_nxt, 0, 0, get_buffer_free_size(s->input_buffer));
}


int shutdownSocket(struct socket_info_t *s, int from_timeout){
    if(s->state == SOCKET_SYN_SENT && s->callback){
        TCPFailCallback(s, -ECONNRESET);
        s->callback = NULL;
    }else if(s->state == SOCKET_ESTABLISHED){
        if(s->write_file){ 
            TCPFailCallback(s, -ECONNRESET);
            close(s->write_f);
            s->write_file = NULL;
        }
        if(s->read_flag){
            TCPFailCallback(s, -ECONNRESET);
            s->read_flag = 0;
        }
    }
    free_socket(s->sock_id, from_timeout);
}

void *TCPTimeout(void *arg){
    struct socket_info_t *s = (struct socket_info_t *)arg;
    while(1){
        usleep(100000);
        pthread_mutex_lock(&socket_mutex);
        if(!s->valid){
            pthread_mutex_unlock(&socket_mutex);
            break;
        }
        if(s->fin_state == SOCKET_FIN_WAIT1 || s->fin_state == SOCKET_FIN_WAIT2 || s->fin_state == SOCKET_TIME_WAIT || s->fin_state == SOCKET_LAST_ACK){
            pthread_mutex_unlock(&socket_mutex);
            break;
        }
        s->timeout ++;
        if(s->timeout == 15){
            if(s->state == SOCKET_SYN_SENT){
                sendTCPControlSegment(s, 1, s->iss, 0, 0, 0, 0, get_buffer_free_size(s->input_buffer));
            }else if(s->state == SOCKET_SYN_RECV){
                sendTCPControlSegment(s, 1, s->iss, 1, s->rcv_nxt, 0, 0, get_buffer_free_size(s->input_buffer));
            }else{
                s->snd_nxt = s->snd_una;
                processWriteSend(s);
            }
            s->timeout = 0;
            s->reset_time += 1;
            #ifdef DEBUG
            printf("socket %d TCP timeout.\n", s->sock_id);
            #endif // DEBUG
            if(s->reset_time == 3){
                #ifdef DEBUG
                printf("socket %d %d %d reset.\n", s->sock_id, s->write_f, s->read_flag);
                #endif // DEBUG
                sendTCPControlSegment(s, 0, s->snd_nxt, 1, s->rcv_nxt, 0, 1, get_buffer_free_size(s->input_buffer));
                shutdownSocket(s, 1);
                pthread_mutex_unlock(&socket_mutex);
                break;
            }
        }
        pthread_mutex_unlock(&socket_mutex);
    }
}


int TCPCallback(const void* buf, int len, uint32_t s_addr, uint32_t d_addr){
    struct tcphdr *hdr = (struct tcphdr *)buf;
    void *tcp_payload = (void *)buf + (((size_t)hdr->doff) << 2);
    int segment_len = (int)(len - (((size_t)hdr->doff) << 2));
    uint16_t s_port = htons(hdr->source);
    uint16_t d_port = htons(hdr->dest);
    #ifdef DEBUG
    char src_ip[20], dst_ip[20];
    inet_ntop(AF_INET, &s_addr, src_ip, 20);
    inet_ntop(AF_INET, &d_addr, dst_ip, 20);
    printf("received tcp packet from %s:%d to %s:%d SYN=%d seq=%d ACK=%d ACKseq=%d len=%d\n", src_ip, s_port, dst_ip, d_port, hdr->syn, htonl(hdr->seq), hdr->ack, htonl(hdr->ack_seq), segment_len);
    #endif // DEBUG
    int device_id = -1;
    for(int i = 0; i < cntdev; i ++){
        if(device_ip_addr[i] == d_addr){
            device_id = i;
        }
    }
    if(device_id == -1) return 0;
    int sock_id = -1;
    int connect_socket_id = device_ports[device_id]->ports[d_port].connect_socket_id;
    struct data_socket_t *p = device_ports[device_id]->ports[d_port].data_socket_ids;
    while(p != NULL){
        struct socket_info_t *s =  &sockets[p->data_socket_id - MAX_PORT_NUM];
        if(s->d_addr == s_addr && s->d_port == s_port){
            sock_id = p->data_socket_id;
            break;
        }
    }
    //printf("%d\n", sock_id);
    if(sock_id != -1){
        struct socket_info_t *s =  &sockets[sock_id - MAX_PORT_NUM];
        if(hdr->rst){
            if(s->read_flag != 0){
                s->fin_state = SOCKET_LAST_ACK;
                sendTCPControlSegment(s, 0, s->snd_nxt, 1, s->rcv_nxt + 1, 1, 0, get_buffer_free_size(s->input_buffer));
                TCPReadCallback(s);
                s->read_flag = 0;
            }
            shutdownSocket(s, 0);
            return 0;
        }
        if(s->state == SOCKET_ESTABLISHED){
            //printf("%d %d, %d %d %d %d %d, %ld %ld\n", s->snd_una, htonl(hdr->ack_seq), s->snd_nxt, s->rcv_nxt, s->fin_state, s->peer_window_size, hdr->window, get_buffer_size(s->input_buffer), get_buffer_size(s->output_buffer));
            int fin_flag = 0;
            if(hdr->ack && htonl(hdr->ack_seq) > s->snd_una && htonl(hdr->ack_seq) <= s->snd_nxt){
                s->timeout = 0;
                s->reset_time = 0;
                consume_ring_buffer(s->output_buffer, NULL, htonl(hdr->ack_seq) - s->snd_una);
                s->snd_una = htonl(hdr->ack_seq);
                s->peer_window_size = htons(hdr->window);
                processWriteSend(s);
            }else if(hdr->ack && htonl(hdr->ack_seq) == s->snd_una){
                s->reset_time = 0;
                s->peer_window_size = htons(hdr->window);
                processWriteSend(s);
            } else if(hdr->ack && htonl(hdr->ack_seq) > s->snd_nxt){
                if(s->fin_state == SOCKET_FIN_WAIT1){
                    s->fin_state = SOCKET_FIN_WAIT2;
                    fin_flag = 1;
                }
            }
            if(segment_len > 0 && htonl(hdr->seq) == s->rcv_nxt){
                ssize_t push_len = segment_len;
                ssize_t free_size = get_buffer_free_size(s->input_buffer);
                if(push_len >= free_size){
                    push_len = free_size;
                }
                //printf("%ld\n", push_len);
                push_ring_buffer(s->input_buffer, tcp_payload, push_len);
                s->rcv_nxt += push_len;
                processRead(s);
            }
            if(hdr->fin && s->fin_state == 0){
                s->reset_time = 0;
                s->fin_state = SOCKET_CLOSE_WAIT;
                sendTCPControlSegment(s, 0, s->snd_nxt, 1, s->rcv_nxt + 1, 0, 0, get_buffer_free_size(s->input_buffer));
                if(s->read_flag != 0){
                    s->fin_state = SOCKET_LAST_ACK;
                    sendTCPControlSegment(s, 0, s->snd_nxt, 1, s->rcv_nxt + 1, 1, 0, get_buffer_free_size(s->input_buffer));
                    TCPReadCallback(s);
                    s->read_flag = 0;
                }
            }else if(hdr-> fin && hdr->ack && s->fin_state == SOCKET_FIN_WAIT2 && !fin_flag){
                s->reset_time = 0;
                sendTCPControlSegment(s, 0, s->snd_nxt + 1, 1, htonl(hdr->seq) + 1, 0, 0, get_buffer_free_size(s->input_buffer));
                pthread_create(&s->fin_pthread, NULL, &freeSocket_pthread, s);
            }else if(hdr->ack && s->fin_state == SOCKET_LAST_ACK){
                s->reset_time = 0;
                free_socket(s->sock_id, 0);
            }
        }else if(s->state == SOCKET_SYN_SENT){
             if(hdr->syn && hdr->ack && htonl(hdr->ack_seq) == s->snd_nxt) {
                s->irs = htonl(hdr->seq);
                s->rcv_nxt = htonl(hdr->seq) + 1;
                s->snd_una = s->snd_nxt;
                s->peer_window_size = htons(hdr->window);
                //ACK
                sendTCPControlSegment(s, 0, s->snd_nxt, 1, s->rcv_nxt, 0, 0, get_buffer_free_size(s->input_buffer));
                s->state = SOCKET_ESTABLISHED;
                s->timeout = 0;
                s->reset_time = 0;
                if(s->callback) {
                    s->callback(s);
                }
            } else if(hdr->syn){
                s->irs = htonl(hdr->seq);
                s->rcv_nxt = htonl(hdr->seq) + 1;
                s->iss = 1;
                s->snd_una = 1;
                s->snd_nxt = 2;
                //SYNACK
                sendTCPControlSegment(s, 1, s->iss, 1, s->rcv_nxt, 0, 0, get_buffer_free_size(s->input_buffer));
                s->snd_nxt += 1;
                s->state = SOCKET_SYN_RECV;
            }
        }else if(s->state == SOCKET_SYN_RECV){
            if(hdr->ack && htonl(hdr->ack_seq) == s->snd_nxt) {
                //s->irs = htonl(hdr->seq);
                s->rcv_nxt = htonl(hdr->seq) + segment_len;
                s->snd_una = s->snd_nxt;
                s->peer_window_size = htons(hdr->window);
                //ACK
                //sendTCPControlSegment(s, 0, s->snd_nxt, 1, s->rcv_nxt, 0, get_buffer_free_size(s->input_buffer));
                s->state = SOCKET_ESTABLISHED;
                s->timeout = 0;
                s->reset_time = 0;
                if(s->callback) {
                    s->callback(s);
                }
            }
        }
    }else{
        struct socket_info_t *s =  &sockets[connect_socket_id - MAX_PORT_NUM];
        if(hdr->syn){
            s->irs = htonl(hdr->seq);
            s->rcv_nxt = htonl(hdr->seq) + 1;
            insert_waiting_connection(s, s_addr, s_port);
            if(s->callback) {
                s->callback(s);
            }
        }
    }
}

int IPCallback(const void* buf, int len){
    struct iphdr *hdr = (struct iphdr *)buf;
    void *ip_payload = ((void *)buf) + (hdr->ihl << 2);
    #ifdef DEBUG
    printf("Received IP Packet Form Src ");
    print_ip_address(&hdr->saddr);
    printf(", Dst ");
    print_ip_address(&hdr->daddr);
    printf(", TTL=%d, Payload:\n%s\n", hdr->ttl, (const char *)ip_payload);
    #endif // DEBUG
    if(hdr->protocol == IPPROTO_TCP){
        pthread_mutex_lock(&socket_mutex);
        TCPCallback(ip_payload, (int)(htons(hdr->tot_len) - (((size_t)hdr->ihl) << 2)), hdr->saddr, hdr->daddr);
        pthread_mutex_unlock(&socket_mutex);
    }
    return 0;
}


int tcp_init(){
    for(int i = 0; i < numdevice; i ++){
        device_ports[i] = (struct device_port_info_t *)malloc(sizeof(struct device_port_info_t));
        device_ports[i]->s_addr = device_ip_addr[i];
    }
    strcat(request_pipe, socket_pipe_dir);
    strcat(request_pipe, socket_pipe_request);
    strcat(request_pipe, ns_name);
    int ret = mkfifo(request_pipe, 0666);
    if(ret == -1 && errno != EEXIST){
        #ifdef DEBUG
        printf("mkfifo failed with errno %d\n", errno);
        #endif
        return ret;
    }
    pthread_mutex_init(&socket_mutex, NULL);
    setIPPacketReceiveCallback(IPCallback);
    pthread_create(&request_pthread, NULL, &request_routine_pthread, NULL);
    return 0;
}


int request_routine(){
    int opt, request_pid;
    char response_pipe[256], rw_pipe[256];
    while(1){
        request_f = open(request_pipe, O_RDONLY);
        request_fd = fdopen(request_f, "r");
        int ret = fscanf(request_fd, "%d", &opt);
        if(ret == 0 || ret == EOF) continue;
        //printf("%d %d\n", ret, opt);
        if(opt == OPT_SOCKET){
            fscanf(request_fd, "%d", &request_pid);
            close(request_f);
            #ifdef DEBUG
            printf("pid %d call socket\n", request_pid);
            #endif // DEBUG
            pthread_mutex_lock(&socket_mutex);
            int socket_id = alloc_socket_id(request_pid);
            alloc_socket(socket_id, SOCKET_TYPE_CONNECTION, SOCKET_CLOSE);
            pthread_mutex_unlock(&socket_mutex);
            #ifdef DEBUG
            printf("pid %d alloc socket %d\n", request_pid, socket_id);
            #endif // DEBUG

            response_pipe[0] = '\0';
            strcat(response_pipe, socket_pipe_dir);
            strcat(response_pipe, socket_pipe_response);
            strcat(response_pipe, ns_name);
            sprintf(response_pipe + strlen(response_pipe), "%d", request_pid);
            int response_f = open(response_pipe, O_WRONLY);
            FILE * response_fd = fdopen(response_f, "w");
            fprintf(response_fd, "%d\n", socket_id);
            fflush(response_fd);
            close(response_f);
        }else if (opt == OPT_BIND){
            int sock_id;
            uint32_t s_addr;
            uint16_t s_port;
            fscanf(request_fd, "%d%x%hu", &sock_id, &s_addr, &s_port);
            close(request_f);
            //printf("%d %08x %d\n", sock_id, s_addr, s_port);
            pthread_mutex_lock(&socket_mutex);
            request_pid = get_socket_val(sock_id);
            #ifdef DEBUG
                printf("pid %d call bind with socket id %d ip addr %08x port %hu\n", request_pid, sock_id, s_addr, s_port);
            #endif // DEBUG
            struct socket_info_t *s =  &sockets[sock_id - MAX_PORT_NUM];
            s->s_addr = s_addr;
            s->s_port = s_port;
            s->device_id = find_device_by_ip(s_addr);
            int ret = can_bind(s);
            if(ret == 0){
                s->state = SOCKET_BINDED;
                device_ports[s->device_id]->ports[s->s_port].connect_socket_id = sock_id;
            }
            #ifdef DEBUG
                printf("pid %d bind socket id %d with ip addr %08x port %hu with ret %d\n", request_pid, sock_id, s_addr, s_port, ret);
            #endif // DEBUG
            response_pipe[0] = '\0';
            strcat(response_pipe, socket_pipe_dir);
            strcat(response_pipe, socket_pipe_response);
            strcat(response_pipe, ns_name);
            sprintf(response_pipe + strlen(response_pipe), "%d", sock_id);
            int response_f = open(response_pipe, O_WRONLY);
            FILE * response_fd = fdopen(response_f, "w");
            fprintf(response_fd, "%d\n", ret);
            fflush(response_fd);
            close(response_f);
            pthread_mutex_unlock(&socket_mutex);
        }else if (opt == OPT_LISTEN){
            int sock_id, backlog;
            fscanf(request_fd, "%d%d", &sock_id, &backlog);
            close(request_f);
            pthread_mutex_lock(&socket_mutex);
            request_pid = get_socket_val(sock_id);
            struct socket_info_t *s =  &sockets[sock_id - MAX_PORT_NUM];
            #ifdef DEBUG
                printf("pid %d call listen with socket id %d ip addr %08x port %hu backlog %d\n", request_pid, sock_id, s->s_addr, s->s_port, backlog);
            #endif // DEBUG
            int ret = can_listen(s);
            if(ret == 0){
                s->state = SOCKET_LISTEN;
                s->backlog = backlog;
            }
            #ifdef DEBUG
                printf("pid %d listen socket id %d with ip addr %08x port %hu backlog %d with ret %d\n", request_pid, sock_id, s->s_addr, s->s_port, backlog, ret);
            #endif // DEBUG
            pthread_mutex_unlock(&socket_mutex);
            response_pipe[0] = '\0';
            strcat(response_pipe, socket_pipe_dir);
            strcat(response_pipe, socket_pipe_response);
            strcat(response_pipe, ns_name);
            sprintf(response_pipe + strlen(response_pipe), "%d", sock_id);
            int response_f = open(response_pipe, O_WRONLY);
            FILE * response_fd = fdopen(response_f, "w");
            fprintf(response_fd, "%d\n", ret);
            fflush(response_fd);
            close(response_f);
        }else if (opt == OPT_CONNECT){
            int sock_id;
            uint32_t d_addr;
            uint16_t d_port;
            fscanf(request_fd, "%d%x%hu", &sock_id, &d_addr, &d_port);
            close(request_f);
            pthread_mutex_lock(&socket_mutex);
            request_pid = get_socket_val(sock_id);
            struct socket_info_t *s =  &sockets[sock_id - MAX_PORT_NUM];
            #ifdef DEBUG
                printf("pid %d call connect with socket id %d ip addr %08x port %hu\n", request_pid, sock_id, d_addr, d_port);
            #endif // DEBUG
            s->device_id = -1;
            routing_query(d_addr, NULL, &(s->device_id));
            int ret = can_connect(s);
            if(ret == 0){
                s->s_addr = device_ip_addr[s->device_id];
                s->s_port = alloc_port(s->device_id, sock_id);
                s->state = SOCKET_BINDED;
                s->d_addr = d_addr;
                s->d_port = d_port;
                s->type = SOCKET_TYPE_DATA;
                s->state = SOCKET_SYN_SENT;
                alloc_ring_buffer(&(s->input_buffer));
                alloc_ring_buffer(&(s->output_buffer));    
                insert_data_socket(&(device_ports[s->device_id]->ports[s->s_port]), sock_id);
                s->iss = 1;
                s->snd_una = 1;
                s->snd_nxt = 2;
                s->callback = TCPConnectCallback;
                s->timeout = 0;
                s->reset_time = 0;
                sendTCPControlSegment(s, 1, s->iss, 0, 0, 0, 0, get_buffer_free_size(s->input_buffer));
                pthread_create(&s->resend_pthread, NULL, &TCPTimeout, s);
                pthread_mutex_unlock(&socket_mutex);
            }else{
                response_pipe[0] = '\0';
                strcat(response_pipe, socket_pipe_dir);
                strcat(response_pipe, socket_pipe_response);
                strcat(response_pipe, ns_name);
                sprintf(response_pipe + strlen(response_pipe), "%d", sock_id);
                int response_f = open(response_pipe, O_WRONLY);
                FILE * response_fd = fdopen(response_f, "w");
                fprintf(response_fd, "%d\n", ret);
                fflush(response_fd);
                close(response_f);
                pthread_mutex_unlock(&socket_mutex);
            }
        }else if(opt == OPT_ACCEPT){
            int sock_id;
            fscanf(request_fd, "%d", &sock_id);
            close(request_f);
            request_pid = get_socket_val(sock_id);
            pthread_mutex_lock(&socket_mutex);
            struct socket_info_t *s =  &sockets[sock_id - MAX_PORT_NUM];
            #ifdef DEBUG
                printf("pid %d call accept with socket id %d ip addr %08x port %hu\n", request_pid, sock_id, s->s_addr, s->s_port);
            #endif // DEBUG
            int ret = can_accept(s);
            if(ret == 0){
                if(s->waiting_connection_count){
                    TCPAccept(s);
                }else{
                    s->callback = WaitTCPAccept;
                }
                pthread_mutex_unlock(&socket_mutex);
            }else{
                response_pipe[0] = '\0';
                strcat(response_pipe, socket_pipe_dir);
                strcat(response_pipe, socket_pipe_response);
                strcat(response_pipe, ns_name);
                sprintf(response_pipe + strlen(response_pipe), "%d", sock_id);
                int response_f = open(response_pipe, O_WRONLY);
                FILE * response_fd = fdopen(response_f, "w");
                fprintf(response_fd, "%d\n", ret);
                fflush(response_fd);
                close(response_f);
                pthread_mutex_unlock(&socket_mutex);
            }
        }else if(opt == OPT_READ){
            int sock_id, len;
            fscanf(request_fd, "%d%d", &sock_id, &len);
            close(request_f);
            
            #ifdef DEBUG
                printf("socket %d call read with len %d\n", sock_id, len);
            #endif // DEBUG
            pthread_mutex_lock(&socket_mutex);
            struct socket_info_t *s =  &sockets[sock_id - MAX_PORT_NUM];
            #ifdef DEBUG
                printf("socket %d call read with len %d ip addr %08x port %hu\n", sock_id, len, s->s_addr, s->s_port);
            #endif // DEBUG
            int ret = can_read(s);
            if(ret == 0 && s->fin_state != SOCKET_CLOSE_WAIT){
                s->read_flag = 1;
                s->read_cnt = 0;
                s->read_len = len;
                
                if(get_buffer_size(s->input_buffer)){
                    processRead(s);
                }
                pthread_mutex_unlock(&socket_mutex);
            }else{
                if(ret == 0 && s->fin_state == SOCKET_CLOSE_WAIT){
                    s->fin_state = SOCKET_LAST_ACK;
                    sendTCPControlSegment(s, 0, s->snd_nxt, 1, s->rcv_nxt + 1, 1, 0, get_buffer_free_size(s->input_buffer));
                }
                response_pipe[0] = '\0';
                strcat(response_pipe, socket_pipe_dir);
                strcat(response_pipe, socket_pipe_response);
                strcat(response_pipe, ns_name);
                sprintf(response_pipe + strlen(response_pipe), "%d", sock_id);
                int response_f = open(response_pipe, O_WRONLY);
                FILE * response_fd = fdopen(response_f, "w");
                fprintf(response_fd, "%d\n", ret);
                fflush(response_fd);
                close(response_f);
                pthread_mutex_unlock(&socket_mutex);
            }     
        }else if(opt == OPT_WRITE){
            int sock_id, len;
            fscanf(request_fd, "%d%d", &sock_id, &len);
            close(request_f);
            rw_pipe[0] = '\0';
            strcat(rw_pipe, socket_pipe_dir);
            strcat(rw_pipe, socket_pipe_write);
            strcat(rw_pipe, ns_name);
            sprintf(rw_pipe + strlen(rw_pipe), "%d", sock_id);
            int rw_f = open(rw_pipe, O_RDONLY);
            FILE *rw_fd = fdopen(rw_f, "r");
            pthread_mutex_lock(&socket_mutex);
            struct socket_info_t *s =  &sockets[sock_id - MAX_PORT_NUM];
            #ifdef DEBUG
                printf("socket %d call write with len %d ip addr %08x port %hu\n", sock_id, len, s->s_addr, s->s_port);
            #endif // DEBUG
            int ret = can_write(s);
            if(ret == 0){
                s->write_f = rw_f;
                s->write_file = rw_fd;
                s->write_cnt = 0;
                s->write_len = len;
                pthread_mutex_unlock(&socket_mutex);
            }else{
                response_pipe[0] = '\0';
                strcat(response_pipe, socket_pipe_dir);
                strcat(response_pipe, socket_pipe_response);
                strcat(response_pipe, ns_name);
                sprintf(response_pipe + strlen(response_pipe), "%d", s->sock_id);
                int response_f = open(response_pipe, O_WRONLY);
                FILE * response_fd = fdopen(response_f, "w");
                fprintf(response_fd, "%d\n", ret);
                fflush(response_fd);
                close(response_f);
                pthread_mutex_unlock(&socket_mutex);
            }
        }else if(opt == OPT_CLOSE){
            int sock_id;
            fscanf(request_fd, "%d", &sock_id);
            close(request_f);
            pthread_mutex_lock(&socket_mutex);
            struct socket_info_t *s =  &sockets[sock_id - MAX_PORT_NUM];
            #ifdef DEBUG
                printf("socket %d call close with ip addr %08x port %hu\n", sock_id, s->s_addr, s->s_port);
            #endif // DEBUG
            int ret = can_close(s);
            if(ret == 0){
                if(s->state == SOCKET_ESTABLISHED || s->state == SOCKET_SYN_RECV){
                    s->fin_tag = 1;
                    processWriteSend(s);
                }else{
                    free_socket(s->sock_id, 0);
                    TCPFinCallback(s);
                }
                pthread_mutex_unlock(&socket_mutex);
            }else{
                response_pipe[0] = '\0';
                strcat(response_pipe, socket_pipe_dir);
                strcat(response_pipe, socket_pipe_response);
                strcat(response_pipe, ns_name);
                sprintf(response_pipe + strlen(response_pipe), "%d", sock_id);
                int response_f = open(response_pipe, O_WRONLY);
                FILE * response_fd = fdopen(response_f, "w");
                fprintf(response_fd, "%d\n", ret);
                fflush(response_fd);
                close(response_f);
                pthread_mutex_unlock(&socket_mutex);
            }
        }
        
    }
}
int main(int argc, char **argv){
    initDevice();
    for (pcap_if_t *p = pcap_devices; p; p = p->next) {
        if(strncmp("veth", p->name, 4) == 0) {
            device_id[numdevice] = addDevice(p->name);
            getDeviceIPAddress(numdevice);
            printf("Device ID %d, Mac Address: ", numdevice);
            print_mac_address(device_mac_addr[numdevice]);
            printf(",IP Address: "); print_ip_address(&device_ip_addr[numdevice]);
            printf(",IP Mask: "); print_ip_address(&device_ip_mask_addr[numdevice]);
            printf("\n");
            numdevice++;
        }
    }
    arp_init();
    route_init();
    IP_init();
    set_ns_name(argv[1]);
    tcp_init();

    while(1){
        sleep(1000);
    }
    return 0;
}