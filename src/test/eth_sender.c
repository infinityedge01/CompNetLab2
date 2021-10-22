#include <link/device.h>
#include <link/packetio.h>
#include <link/util.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

char buf[MAX_ETH_PACKET_SIZE];
int eth_callback(const void *data, int len, int dev) {
    printf("Receive from device id %d, length %d\n", dev, len);
    printf("Source address: ");
    char mac[6];
    get_src_addr(data, mac);
    print_mac_address(mac);
    printf("\n");
    printf("Data: \n");
    int payload_len = get_payload(data, len, buf);
    buf[payload_len] = '\0';
    printf("%s\n", buf);
    return 0;
}
int main(int argc, char **argv){
    initDevice();
    char device_name[] = "veth1-2";
    int id = addDevice((const char *)device_name);
    int ret = setFrameReceiveCallback(eth_callback);
    char data[] = "Hello, I'm Diana!";
    char mac[] = {0x4a, 0x11, 0x3e, 0xc7, 0x40, 0x3f};
    while(1){
        sendFrame((const void *)data, strlen(data), 0x0000, (const void *)mac, id);
        sleep(1);
    }
    return 0;
}