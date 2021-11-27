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
char buf[1048576];
int main(int argc, char **argv){
    set_ns_name(argv[1]);
    //while(1){
        //sleep(1);
        int x = socket(AF_INET, SOCK_STREAM, 0);
        struct in_addr ip;
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        inet_pton(AF_INET, "10.100.1.1", &addr.sin_addr.s_addr);
        addr.sin_port = 1123;
        int y = bind(x, (struct sockaddr *)&addr, sizeof(addr));
        int z = listen(x, 1024);
        printf("%d %d %d\n", x, y, z);
        int socket_id;
        struct sockaddr address;
        socklen_t address_len;
        while(1){
            printf("Listening...\n");
            socket_id = accept(x, &address, &address_len);
            printf("%d\n", socket_id);
            FILE *f = fopen("download.png", "w");
            int tot_write = 0;
            while(1){
                int ret = read(socket_id, buf, 1048576);
                if(ret <= 0){
                    printf("%d\n", ret);
                    break;
                }
                fwrite(buf, ret, 1, f);
                tot_write += ret;
                printf("write %d bytes\n", tot_write);
            }
            fclose(f);
        }
    //}
    return 0;
}