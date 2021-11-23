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

int device_id[MAX_DEVICE];
int numdevice;

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
    return 0;
}
struct device_port_info *device_ports[MAX_DEVICE];
struct socket_info_t sockets[MAX_PORT_NUM];

void alloc_socket(int socket_id, int type, int state){
    struct socket_info_t *s = &sockets[socket_id - MAX_PORT_NUM];
    s->valid = 1;
    s->type = type;
    s->state = state;
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

char request_pipe[256];
int request_f;
FILE *request_fd;

int request_routine();

pthread_t request_pthread;
void *request_routine_pthread(void *arg){
    request_routine();
}


int tcp_init(){
    for(int i = 0; i < numdevice; i ++){
        device_ports[i] = (struct device_port_info *)malloc(sizeof(struct device_port_info));
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
            sprintf(response_pipe + strlen(response_pipe), "%d", request_pid);
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
            }
            #ifdef DEBUG
                printf("pid %d listen socket id %d with ip addr %08x port %hu backlog %d with ret %d\n", request_pid, sock_id, s->s_addr, s->s_port, backlog, ret);
            #endif // DEBUG

            response_pipe[0] = '\0';
            strcat(response_pipe, socket_pipe_dir);
            strcat(response_pipe, socket_pipe_response);
            strcat(response_pipe, ns_name);
            sprintf(response_pipe + strlen(response_pipe), "%d", request_pid);
            int response_f = open(response_pipe, O_WRONLY);
            FILE * response_fd = fdopen(response_f, "w");
            fprintf(response_fd, "%d\n", ret);
            fflush(response_fd);
            close(response_f);
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

    setIPPacketReceiveCallback(IPCallback);

    while(1){
        sleep(1000);
    }
    return 0;
}