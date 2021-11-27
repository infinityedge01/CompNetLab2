#include <transport/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
char buf[1024];
int main(int argc, char **argv){
    set_ns_name(argv[1]);
    //while(1){
        //sleep(1);
        int x = __wrap_socket(AF_INET, SOCK_STREAM, 0);
        struct in_addr ip;
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        inet_pton(AF_INET, "10.100.1.1", &addr.sin_addr.s_addr);
        addr.sin_port = 1123;
        int y = __wrap_bind(x, (struct sockaddr *)&addr, sizeof(addr));
        int z = __wrap_listen(x, 1024);
        printf("%d %d %d\n", x, y, z);
        int socket_id;
        struct sockaddr address;
        socklen_t address_len;
        socket_id = __wrap_accept(x, &address, &address_len);
        printf("%d\n", socket_id);
        while(1){
            int ret = __wrap_read(socket_id, buf, 1024);
            if(ret <= 0){
                printf("%d\n", ret);
                break;
            }
            printf("%d\n", ret);
            for(int i = 0; i < ret; i ++){
                putchar(buf[i]);
            }
            printf("\n");
        }
    //}
    return 0;
}