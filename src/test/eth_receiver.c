#include <link/device.h>
#include <link/packetio.h>
#include <link/util.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
char buf[MAX_ETH_PACKET_SIZE];
int device_id;
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
    char send_data[] = "Avava AvA!";
    sendFrame((const void *)send_data, strlen(send_data), 0x0000, (const void *)mac, device_id);
    return 0;
}

int main(int argc, char **argv){
    initDevice();
    char device_name[] = "veth2-1";
    device_id = addDevice((const char *)device_name);
    int ret = setFrameReceiveCallback(eth_callback, 0x0000);
    while(1){
        sleep(1);
    }
    return 0;
}