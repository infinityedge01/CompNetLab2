#include <network/device.h>
#include <network/util.h>
#include <network/arp.h>

#include <link/packetio.h>
#include <link/util.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
int device_id[MAX_DEVICE];
int numdevice;

static void sleep_ms(unsigned int secs){
    struct timeval tval;
    tval.tv_sec=secs/1000;
    tval.tv_usec=(secs*1000)%1000000;
    select(0,NULL,NULL,NULL,&tval);

}
int main(int argc, char **argv){
    initDevice();
    for (pcap_if_t *p = pcap_devices; p; p = p->next) {
        if(strncmp("veth", p->name, 4) == 0) {
            device_id[numdevice++] = addDevice(p->name);
            getDeviceIPAddress(numdevice - 1);
            printf("Device ID %d, Mac Address: ", numdevice - 1);
            print_mac_address(device_mac_addr[numdevice - 1]);
            printf(",IP Address: "); print_ip_address(&device_ip_addr[numdevice - 1]);
            printf("\n");
        }
    }
    arp_init();
    while(1){
        sleep(1);
    }
    return 0;
}