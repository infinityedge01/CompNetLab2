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
char send_buf[1048576 * 20];
int main(int argc, char **argv){
    //set_ns_name(argv[1]);
    //while(1){
        //sleep(1);
        int x = socket(AF_INET, SOCK_STREAM, 0);
        struct in_addr ip;
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        inet_pton(AF_INET, "10.100.1.1", &addr.sin_addr.s_addr);
        addr.sin_port = 1123;
        int y = connect(x, (struct sockaddr *)&addr, sizeof(addr));
        FILE *f = fopen("diana.png", "r");
        int len = fread(send_buf, 1, 1048576 * 20, f);
        fclose(f);
        write(x, send_buf, len);
        close(x);
    //}
    return 0;
}