#include <transport/socket.h>

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

char socket_pipe_dir[] = "/root/.mytcppipe/";
char socket_pipe_request[] = "pipe_req_";
char socket_pipe_response[] = "pipe_resp_";

char ns_name[256];
int socket_id_list[MAX_PORT_NUM];

int set_ns_name(char * name){
    strcpy(ns_name, name);
    return 0;
}

int alloc_socket_id(int val){
    if(val == 0){
        return -1;
    }
    for(int i = 0; i < MAX_PORT_NUM; i++){
        if(socket_id_list[i] == 0){
            socket_id_list[i] = val;
            return i + MAX_PORT_NUM;
        }
    }
    return -1;
}

int get_socket_val(int socket_id){
    return socket_id_list[socket_id - MAX_PORT_NUM];
}

int free_socket_id(int socket_id){
    if(socket_id_list[socket_id - MAX_PORT_NUM] == 0) return -1;
    socket_id_list[socket_id - MAX_PORT_NUM] = 0;
    return 0;
}

int request_f, response_f;
FILE *request_fd, *response_fd;
char ns_name[256];
char request_pipe[256];
char response_pipe[256];
int initialize_response_pipe(int response_id){
    response_pipe[0] = '\0';
    strcat(response_pipe, socket_pipe_dir);
    strcat(response_pipe, socket_pipe_response);
    strcat(response_pipe, ns_name);
    sprintf(response_pipe + strlen(response_pipe), "%d", response_id);
    int ret = mkfifo(response_pipe, 0666);
    //printf("%s\n", response_pipe);
    if(ret == -1 && errno != EEXIST){
        #ifdef DEBUG
        printf("mkfifo failed with errno %d\n", errno);
        #endif
        return ret;
    }
    //printf("%d\n", ret);
    return 0;
}
int wirte_request_pipe(char* buf){
    if(!request_f){
        if(request_pipe[0] == '\0'){
            strcat(request_pipe, socket_pipe_dir);
            strcat(request_pipe, socket_pipe_request);
            strcat(request_pipe, ns_name);
        }
        request_f = open(request_pipe, O_WRONLY);
        if(request_f < 0){
            #ifdef DEBUG
            printf("open error with errno %d\n", errno);
            #endif
            return -1;
        }
        request_fd = fdopen(request_f, "w");
        if(request_fd == NULL){
            #ifdef DEBUG
            printf("fdopen error with errno %d\n", errno);
            #endif
            return -1;
        }
    }
    fputs(buf, request_fd);
    fflush(request_fd);
    int ret = close(request_f);
    request_f = 0;
    if(ret < 0){
        #ifdef DEBUG
        printf("close error with errno %d\n", errno);
        #endif
        return -1;
    }
}

int read_response_pipe(){
    response_f = open(response_pipe, O_RDONLY);
    response_fd = fdopen(response_f, "r");
    int ret;
    fscanf(response_fd, "%d", &ret);
    close(response_f);
    response_f = 0;
    return ret;
}

int read_response_pipe_accept(int *sock_id, uint32_t *d_addr, uint16_t *d_port){
    response_f = open(response_pipe, O_RDONLY);
    response_fd = fdopen(response_f, "r");
    fscanf(response_fd, "%d%x%hu", sock_id, d_addr, d_port);
    close(response_f);
    response_f = 0;
    return 0;
}


int __wrap_socket(int domain, int type, int protocol){
    if(domain == AF_INET && type == SOCK_STREAM && (protocol == 0 || protocol == IPPROTO_TCP)){
        char request_buf[256];
        sprintf(request_buf, "%d %d\n", OPT_SOCKET, getpid());
        //puts(request_buf);
        wirte_request_pipe(request_buf);
        initialize_response_pipe(getpid());
        int ret = read_response_pipe();
        int socket_id = alloc_socket_id(ret);
        #ifdef DEBUG
        printf("process %d get socket id %d local id %d\n", getpid(), ret, socket_id);
        #endif // DEBUG
        return socket_id;
    }else{
        return -1;
    }
}

int __wrap_bind(int socket, const struct sockaddr * address ,
socklen_t address_len){
    int ret;
    if (socket >= MAX_PORT_NUM) {
        const struct sockaddr_in *addr = (const struct sockaddr_in*)address;
        if(addr->sin_family != AF_INET){
            return -1;
        }
        char request_buf[256];
        sprintf(request_buf, "%d %d %08x %hu\n", OPT_BIND, get_socket_val(socket), addr->sin_addr.s_addr, addr->sin_port);
        //puts(request_buf);
        wirte_request_pipe(request_buf);
        initialize_response_pipe(get_socket_val(socket));
        int ret = read_response_pipe();
        #ifdef DEBUG
            printf("process %d bind socket id %d local id %d with address %08x port %hu with ret %d\n", getpid(),   get_socket_val(socket), socket, addr->sin_addr.s_addr, addr->sin_port, ret);
        #endif // DEBUG
        if(ret < 0){
            errno = -ret;
            return -1;
        }
        return ret;
    } else {
        return -1;
    }
}

int __wrap_listen(int socket, int backlog){
    int ret;
    if (socket >= MAX_PORT_NUM) {
        if(backlog != 1024){
            return -1;
        }
        char request_buf[256];
        sprintf(request_buf, "%d %d %d\n", OPT_LISTEN, get_socket_val(socket), backlog);
        initialize_response_pipe(get_socket_val(socket));
        wirte_request_pipe(request_buf);
        int ret = read_response_pipe();
        #ifdef DEBUG
            printf("process %d listen socket id %d local id %d backlog %d with ret %d\n", getpid(), get_socket_val(socket), socket, backlog, ret);
        #endif // DEBUG
        if(ret < 0){
            errno = -ret;
            return -1;
        }
        return ret;
    } else {
        return -1;
    }
}

int __wrap_connect(int socket, const struct sockaddr * address ,
socklen_t address_len){
    int ret;
    if (socket >= MAX_PORT_NUM) {
        const struct sockaddr_in *addr = (const struct sockaddr_in*)address;
        if(addr->sin_family != AF_INET){
            return -1;
        }
        char request_buf[256];
        sprintf(request_buf, "%d %d %08x %hu\n", OPT_CONNECT, get_socket_val(socket), addr->sin_addr.s_addr, addr->sin_port);
        //puts(request_buf);
        initialize_response_pipe(get_socket_val(socket));
        wirte_request_pipe(request_buf);
        int ret = read_response_pipe();
        #ifdef DEBUG
            printf("process %d connect socket id %d local id %d with address %08x port %hu with ret %d\n", getpid(),   get_socket_val(socket), socket, addr->sin_addr.s_addr, addr->sin_port, ret);
        #endif // DEBUG
        if(ret < 0){
            errno = -ret;
            return -1;
        }
        return ret;
    } else {
        return -1;
    }
}

int __wrap_accept(int socket, struct sockaddr *address, socklen_t *address_len) {
    int ret;
    if (socket >= MAX_PORT_NUM) {
        char request_buf[256];
        sprintf(request_buf, "%d %d\n", OPT_ACCEPT, get_socket_val(socket));
        initialize_response_pipe(get_socket_val(socket));
        wirte_request_pipe(request_buf);
        struct sockaddr_in *addr = (struct sockaddr_in*)address;
        int ret;
        read_response_pipe_accept(&ret, &(addr->sin_addr.s_addr), &(addr->sin_port));
        if(ret < 0){
            errno = -ret;
            return -1;
        }
        int socket_id = alloc_socket_id(ret);
        #ifdef DEBUG
            printf("process %d accept socket id %d local id %d with address %08x port %hu with new socket id %d local id %d\n", getpid(),   get_socket_val(socket), socket, addr->sin_addr.s_addr, addr->sin_port, ret, socket_id);
        #endif // DEBUG
        return socket_id;
    } else {
        return -1;
    }
}