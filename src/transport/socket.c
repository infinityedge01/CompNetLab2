#include <transport/socket.h>
#include <pcap/pcap.h>

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

int __real_setsockopt (int __fd, int __level, int __optname,
		       const void *__optval, socklen_t __optlen);
int __real_socket(int domain, int type, int protocol);
int __real_bind(int socket, const struct sockaddr * address ,
socklen_t address_len);
int __real_listen(int socket, int backlog);
int __real_connect(int socket, const struct sockaddr * address ,
socklen_t address_len);
int __real_accept(int socket, struct sockaddr * address ,
socklen_t * address_len);
ssize_t __real_read(int fildes, void *buf, size_t nbyte);
ssize_t __real_write(int fildes, const void *buf, size_t nbyte);
int __real_close(int fildes);
int __real_getaddrinfo(const char *node, const char * service, const struct addrinfo *hints, struct addrinfo ** res);

char socket_pipe_dir[] = "/root/.mytcppipe/";
char socket_pipe_request[] = "pipe_req_";
char socket_pipe_response[] = "pipe_resp_";
char socket_pipe_read[] = "pipe_read_";
char socket_pipe_write[] = "pipe_write_";

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

int request_f, response_f, read_f, write_f;
FILE *request_fd, *response_fd, *read_fd, *write_fd;
char ns_name[256];
char request_pipe[256];
char response_pipe[256];
char read_pipe[256];
char write_pipe[256];
char errbuf[256];
int initialize_response_pipe(int response_id){
    response_pipe[0] = '\0';
    strcat(response_pipe, socket_pipe_dir);
    strcat(response_pipe, socket_pipe_response);
    strcat(response_pipe, ns_name);
    sprintf(response_pipe + strlen(response_pipe), "%d", response_id);
    int ret = mkfifo(response_pipe, 0666);
    //printf("%s\n", response_pipe);
    if(ret == -1 && errno != EEXIST){
        #ifdef SOCKDEBUG
        printf("mkfifo failed with errno %d\n", errno);
        #endif
        return ret;
    }
    //printf("%d\n", ret);
    return 0;
}
int write_request_pipe(char* buf){
    if(request_pipe[0] == '\0'){
        request_pipe[0] = '\0';
        strcat(request_pipe, socket_pipe_dir);
        strcat(request_pipe, socket_pipe_request);
        if(ns_name[0] == '\0'){
            pcap_if_t *p;
            int ret = pcap_findalldevs(&p, errbuf);
            if(ret != 0){
                #ifdef SOCKDEBUG
                fprintf(stderr, "Error at pcap_findalldevs() in write_request_pipe(): %s\n", errbuf);
                #endif
                return ret;
            }
            set_ns_name(p->name);
        }
        strcat(request_pipe, ns_name);
    }
    request_f = open(request_pipe, O_WRONLY);
    if(request_f < 0){
        #ifdef SOCKDEBUG
        printf("open error with errno %d\n", errno);
        #endif
        return -1;
    }
    request_fd = fdopen(request_f, "w");
    if(request_fd == NULL){
        #ifdef SOCKDEBUG
        printf("fdopen error with errno %d\n", errno);
        #endif
        return -1;
    }
    fputs(buf, request_fd);
    fflush(request_fd);
    int ret = close(request_f);
    request_f = 0;
    if(ret < 0){
        #ifdef SOCKDEBUG
        printf("close error with errno %d\n", errno);
        #endif
        return -1;
    }
    return 0;
}


int initialize_read_pipe(int socket_id){
    read_pipe[0] = '\0';
    strcat(read_pipe, socket_pipe_dir);
    strcat(read_pipe, socket_pipe_read);
    strcat(read_pipe, ns_name);
    sprintf(read_pipe + strlen(read_pipe), "%d", socket_id);
    int ret = mkfifo(read_pipe, 0666);
    if(ret == -1 && errno != EEXIST){
        #ifdef SOCKDEBUG
        printf("mkfifo failed with errno %d\n", errno);
        #endif
        return ret;
    }
    return 0;
}

ssize_t read_read_pipe(char* buf, ssize_t read_len){
    read_f = open(read_pipe, O_RDONLY);
    if(read_f < 0){
        #ifdef SOCKDEBUG
        printf("open error with errno %d\n", errno);
        #endif
        return -1;
    }
    read_fd = fdopen(read_f, "r");
    if(read_fd == NULL){
        #ifdef SOCKDEBUG
        printf("fdopen error with errno %d\n", errno);
        #endif
        return -1;
    }
    ssize_t read_size = fread(buf, 1, read_len, read_fd);
    int ret = close(read_f);
    read_f = 0;
    if(ret < 0){
        #ifdef SOCKDEBUG
        printf("close error with errno %d\n", errno);
        #endif
        return -1;
    }
    return read_size;
}

int write_write_pipe(int socket_id, char* buf, ssize_t write_len){
    write_pipe[0] = '\0';
    strcat(write_pipe, socket_pipe_dir);
    strcat(write_pipe, socket_pipe_write);
    strcat(write_pipe, ns_name);
    sprintf(write_pipe + strlen(write_pipe), "%d", socket_id);
    int ret = mkfifo(write_pipe, 0666);
    write_f = open(write_pipe, O_WRONLY);
    if(write_f < 0){
        #ifdef SOCKDEBUG
        printf("open error with errno %d\n", errno);
        #endif
        return -1;
    }
    write_fd = fdopen(write_f, "w");
    if(write_fd == NULL){
        #ifdef SOCKDEBUG
        printf("fdopen error with errno %d\n", errno);
        #endif
        return -1;
    }
    fwrite(buf, write_len, 1, write_fd);
    fflush(write_fd);
    ret = close(write_f);
    write_f = 0;
    if(ret < 0){
        #ifdef SOCKDEBUG
        printf("close error with errno %d\n", errno);
        #endif
        return -1;
    }
    return 0;
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


int __wrap_setsockopt(int __fd, int __level, int __optname,
		       const void *__optval, socklen_t __optlen){
    if (__fd >= MAX_PORT_NUM) {
        return 0;
    } else {
        return __real_setsockopt (__fd, __level, __optname, __optval, __optlen);
    }
 }

int __wrap_socket(int domain, int type, int protocol){
    if(domain == AF_INET && type == SOCK_STREAM && (protocol == 0 || protocol == IPPROTO_TCP)){
        char request_buf[256];
        sprintf(request_buf, "%d %d\n", OPT_SOCKET, getpid());
        //puts(request_buf);
        write_request_pipe(request_buf);
        initialize_response_pipe(getpid());
        int ret = read_response_pipe();
        int socket_id = alloc_socket_id(ret);
        #ifdef SOCKDEBUG
        printf("process %d get socket id %d local id %d\n", getpid(), ret, socket_id);
        #endif // SOCKDEBUG
        return socket_id;
    }else{
        return __real_socket(domain, type, protocol);
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
        write_request_pipe(request_buf);
        initialize_response_pipe(get_socket_val(socket));
        int ret = read_response_pipe();
        #ifdef SOCKDEBUG
            printf("process %d bind socket id %d local id %d with address %08x port %hu with ret %d\n", getpid(),   get_socket_val(socket), socket, addr->sin_addr.s_addr, addr->sin_port, ret);
        #endif // SOCKDEBUG
        if(ret < 0){
            errno = -ret;
            return ret;
        }
        return ret;
    } else {
        return __real_bind(socket, address, address_len);
    }
}

int __wrap_listen(int socket, int backlog){
    int ret;
    if (socket >= MAX_PORT_NUM) {
        char request_buf[256];
        sprintf(request_buf, "%d %d %d\n", OPT_LISTEN, get_socket_val(socket), backlog);
        initialize_response_pipe(get_socket_val(socket));
        write_request_pipe(request_buf);
        int ret = read_response_pipe();
        #ifdef SOCKDEBUG
            printf("process %d listen socket id %d local id %d backlog %d with ret %d\n", getpid(), get_socket_val(socket), socket, backlog, ret);
        #endif // SOCKDEBUG
        if(ret < 0){
            errno = -ret;
            return ret;
        }
        return ret;
    } else {
        return __real_listen(socket, backlog);
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
        write_request_pipe(request_buf);
        int ret = read_response_pipe();
        #ifdef SOCKDEBUG
            printf("process %d connect socket id %d local id %d with address %08x port %hu with ret %d\n", getpid(),   get_socket_val(socket), socket, addr->sin_addr.s_addr, addr->sin_port, ret);
        #endif // SOCKDEBUG
        if(ret < 0){
            errno = -ret;
            return ret;
        }
        return ret;
    } else {
        return __real_connect(socket, address, address_len);
    }
}

int __wrap_accept(int socket, struct sockaddr *address, socklen_t *address_len) {
    int ret;
    if (socket >= MAX_PORT_NUM) {
        char request_buf[256];
        sprintf(request_buf, "%d %d\n", OPT_ACCEPT, get_socket_val(socket));
        initialize_response_pipe(get_socket_val(socket));
        write_request_pipe(request_buf);
        struct sockaddr_in *addr = (struct sockaddr_in*)address;
        int ret;
        read_response_pipe_accept(&ret, &(addr->sin_addr.s_addr), &(addr->sin_port));
        if(ret < 0){
            errno = -ret;
            return ret;
        }
        int socket_id = alloc_socket_id(ret);
        #ifdef SOCKDEBUG
            printf("process %d accept socket id %d local id %d with address %08x port %hu with new socket id %d local id %d\n", getpid(),   get_socket_val(socket), socket, addr->sin_addr.s_addr, addr->sin_port, ret, socket_id);
        #endif // SOCKDEBUG
        return socket_id;
    } else {
        return __real_accept(socket, address, address_len);
    }
}

ssize_t __wrap_read(int fildes, void *buf, size_t nbyte){
    ssize_t ret;
    if (fildes >= MAX_PORT_NUM) {
        char request_buf[256];
        sprintf(request_buf, "%d %d %ld\n", OPT_READ, get_socket_val(fildes), nbyte);
        initialize_response_pipe(get_socket_val(fildes));
        initialize_read_pipe(get_socket_val(fildes));
        #ifdef SOCKDEBUG
            printf("process %d call read socket id %d local id %d nbyte %ld\n", getpid(),   get_socket_val(fildes), fildes, nbyte);
        #endif // SOCKDEBUG
        write_request_pipe(request_buf);
        ret = read_response_pipe();
        //printf("%ld\n", ret);
        
        if(ret < 0){
            errno = -ret;
            return ret;
        }
        if(ret == 0){
            //free_socket_id(fildes);
        }
        if(ret > 0){
            ssize_t len = read_read_pipe(buf, ret);
            #ifdef SOCKDEBUG
                printf("process %d read socket id %d local id %d nbyte %ld with ret %ld\n", getpid(),   get_socket_val(fildes), fildes, nbyte, ret);
            #endif // SOCKDEBUG
            return len;
        }
        return ret;
    } else {
        return __real_read(fildes, buf, nbyte);
    }
}

ssize_t __wrap_write(int fildes, const void *buf, size_t nbyte){
    int ret;
    if (fildes >= MAX_PORT_NUM) {
        char request_buf[256];
        sprintf(request_buf, "%d %d %ld\n", OPT_WRITE, get_socket_val(fildes), nbyte);
        #ifdef SOCKDEBUG
            printf("process %d call write socket id %d local id %d nbyte %ld\n", getpid(),   get_socket_val(fildes), fildes, nbyte);
        #endif // SOCKDEBUG
        initialize_response_pipe(get_socket_val(fildes));
        write_request_pipe(request_buf);
        ret = read_response_pipe();
        if(ret < 0){
            errno = -ret;
            return ret;
        }
        if(ret > 0){
            int ret2 = write_write_pipe(get_socket_val(fildes), (char *)buf, ret);
            if(ret2 < 0){
                errno = -ret2;
                return ret2;
            }
        }
        #ifdef SOCKDEBUG
            printf("process %d write socket id %d local id %d nbyte %ld with ret %d\n", getpid(),   get_socket_val(fildes), fildes, nbyte, ret);
        #endif // SOCKDEBUG
        return ret;
    } else {
        return __real_write(fildes, buf, nbyte);
    }
}

int __wrap_close(int fildes){
    int ret;
    if (fildes >= MAX_PORT_NUM) {
        char request_buf[256];
        sprintf(request_buf, "%d %d\n", OPT_CLOSE, get_socket_val(fildes));
        #ifdef SOCKDEBUG
            printf("process %d call close socket id %d local id %d\n", getpid(), get_socket_val(fildes), fildes);
        #endif // SOCKDEBUG
        if(get_socket_val(fildes) == 0){
            return 0;
        }
        initialize_response_pipe(get_socket_val(fildes));
        write_request_pipe(request_buf);
        ret = read_response_pipe();
        if(ret < 0){
            errno = -ret;
            return ret;
        }else{
            free_socket_id(fildes);
        }
        #ifdef SOCKDEBUG
            printf("process %d close socket id %d local id %d with ret %d\n", getpid(), get_socket_val(fildes), fildes, ret);
        #endif // SOCKDEBUG
        return ret;
    } else {
        return __real_close(fildes);
    }
}

int __wrap_getaddrinfo(const char *node, const char * service, const struct addrinfo *hints, struct addrinfo ** res){
    return __real_getaddrinfo(node, service, hints, res);
}