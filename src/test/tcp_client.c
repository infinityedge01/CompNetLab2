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
        int y = __wrap_connect(x, (struct sockaddr *)&addr, sizeof(addr));
        char send_buf[] = "Subscribe to Diana and your hunger will be satisfied.";
        __wrap_write(x, send_buf, sizeof(send_buf));
        __wrap_close(x);
    //}
    return 0;
}