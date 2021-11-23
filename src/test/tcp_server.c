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
    //}
    return 0;
}