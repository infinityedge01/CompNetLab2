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

void alloc_socket(int socket_id, int type, int state){
    struct socket_info_t *s = &sockets[socket_id - MAX_PORT_NUM];
    s->valid = 1;
    s->type = type;
    s->state = state;
    s->sock_id = socket_id;
    s->callback = NULL;
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
    if (s->state != SOCKET_UNCONNECTED) {return -EINVAL;}
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
    if (s->state != SOCKET_UNCONNECTED) {return -EINVAL;}
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

char request_pipe[256];
int request_f;
FILE *request_fd;

int request_routine();

pthread_t request_pthread;
void *request_routine_pthread(void *arg){
    request_routine();
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
    sendTCPControlSegment(s, 1, s->iss, 1, s->rcv_nxt, 0, get_buffer_free_size(s->input_buffer));
}
int WaitTCPAccept(struct socket_info_t *sock){
    sock->callback = NULL;
    TCPAccept(sock);
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
    printf("received tcp packet from %s:%d to %s:%d SYN=%d seq=%d ACK=%d ACKseq=%d\n", src_ip, s_port, dst_ip, d_port, hdr->syn, htonl(hdr->seq), hdr->ack, htonl(hdr->ack_seq));
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
    printf("%d\n", sock_id);
    if(sock_id != -1){
        struct socket_info_t *s =  &sockets[sock_id - MAX_PORT_NUM];
        if(s->state == SOCKET_SYN_SENT){
             if(hdr->syn && hdr->ack && htonl(hdr->ack_seq) == s->snd_nxt) {
                s->irs = htonl(hdr->seq);
                s->rcv_nxt = htonl(hdr->seq) + 1;
                s->snd_una = s->snd_nxt;
                //ACK
                sendTCPControlSegment(s, 0, s->snd_nxt, 1, s->rcv_nxt, 0, get_buffer_free_size(s->input_buffer));
                s->state = SOCKET_ESTABLISHED;
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
                sendTCPControlSegment(s, 1, s->iss, 1, s->rcv_nxt, 0, get_buffer_free_size(s->input_buffer));
                s->snd_nxt += 1;
                s->state = SOCKET_SYN_RECV;
            }
        }else if(s->state == SOCKET_SYN_RECV){
                if(hdr->ack && htonl(hdr->ack_seq) == s->snd_nxt) {
                s->irs = htonl(hdr->seq);
                s->rcv_nxt = htonl(hdr->seq) + 1;
                s->snd_una = s->snd_nxt;
                //ACK
                //sendTCPControlSegment(s, 0, s->snd_nxt, 1, s->rcv_nxt, 0, get_buffer_free_size(s->input_buffer));
                s->state = SOCKET_ESTABLISHED;
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
        TCPCallback(ip_payload, (int)(htons(hdr->tot_len) - (((size_t)hdr->ihl) << 2)), hdr->saddr, hdr->daddr);
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
    setIPPacketReceiveCallback(IPCallback);
    pthread_create(&request_pthread, NULL, &request_routine_pthread, NULL);
    return 0;
}


int request_routine(){
    int opt, request_pid;
    char response_pipe[256];
    while(1){
        request_f = open(request_pipe, O_RDONLY);
        request_fd = fdopen(request_f, "r");
        int ret = fscanf(request_fd, "%d", &opt);
        if(ret == 0 || ret == EOF) continue;
        //printf("%d %d\n", ret, opt);
        if(opt == OPT_SOCKET){
            fscanf(request_fd, "%d", &request_pid);
            #ifdef DEBUG
            printf("pid %d call socket\n", request_pid);
            #endif // DEBUG
            int socket_id = alloc_socket_id(request_pid);
            alloc_socket(socket_id, SOCKET_TYPE_CONNECTION, SOCKET_UNCONNECTED);
            
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
            //printf("%d %08x %d\n", sock_id, s_addr, s_port);
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
        }else if (opt == OPT_LISTEN){
            int sock_id, backlog;
            fscanf(request_fd, "%d%d", &sock_id, &backlog);
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
                sendTCPControlSegment(s, 1, s->iss, 0, 0, 0, get_buffer_free_size(s->input_buffer));
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
            }
        }else if(opt == OPT_ACCEPT){
            int sock_id;
            fscanf(request_fd, "%d", &sock_id);
            request_pid = get_socket_val(sock_id);
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
            }
        }
        close(request_f);
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