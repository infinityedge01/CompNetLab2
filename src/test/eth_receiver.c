#include <link/device.h>
#include <link/packetio.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

int eth_callback(const void *data, int len, int dev) {
    printf("Receive from device id %d, length %d\n", dev, len);
    for(int i = 0; i < len; i++) {
        printf("%02x ", *(unsigned char *)(data + i));
    }
    return 0;
}

int main(int argc, char **argv){
    initDevice();
    char device_name[] = "veth2-1";
    int id = addDevice((const char *)device_name);
    int ret = setFrameReceiveCallback(eth_callback);
    while(1){
        sleep(1);
    }
    return 0;
}