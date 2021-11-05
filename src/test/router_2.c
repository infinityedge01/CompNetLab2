#include <network/device.h>
#include <network/util.h>
#include <network/arp.h>
#include <network/routing.h>
#include <network/ip.h>

#include <link/packetio.h>
#include <link/util.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

int device_id[MAX_DEVICE];
int numdevice;

int IPCallback(const void* buf, int len){
    struct iphdr *hdr = (struct iphdr *)buf;
    void *ip_payload = ((void *)buf) + (hdr->ihl << 2);
    printf("Received IP Packet Form Src ");
    print_ip_address(&hdr->saddr);
    printf(", Dst ");
    print_ip_address(&hdr->daddr);
    printf(", TTL=%d, Payload:\n%s\n", hdr->ttl, (const char *)ip_payload);
    return 0;
}

int main(int argc, char **argv){
    initDevice();
    for (pcap_if_t *p = pcap_devices; p; p = p->next) {
        if(strncmp("veth", p->name, 4) == 0) {
            device_id[numdevice] = addDevice(p->name);
            getDeviceIPAddress(numdevice);
            printf("Device ID %d, Mac Address: ", numdevice);
            print_mac_address(device_mac_addr[numdevice]);
            printf(",IP Address: "); print_ip_address(&device_ip_addr[numdevice]);
            printf(",IP Mask: "); print_ip_address(&device_ip_mask_addr[numdevice]);
            printf("\n");
            numdevice++;
        }
    }
    arp_init();
    route_init();
    IP_init();
    struct in_addr addr, mask, dest;
    inet_pton(AF_INET, "0.0.0.0", &addr.s_addr);
    inet_pton(AF_INET, "0.0.0.0", &mask.s_addr);
    inet_pton(AF_INET, "10.100.4.2", &dest.s_addr);
    int devid = -1;
    for(int i = 0; i < cntdev; i ++){
        if((device_ip_addr[i] & device_ip_mask_addr[i]) == (dest.s_addr & device_ip_mask_addr[i])){
            devid = i;
        }
    }
    sleep(6);
    struct arp_record * record = find_arp_record(dest.s_addr);
    setRoutingTable(addr, mask, record->mac_addr, devices[devid]->name);
    setIPPacketReceiveCallback(IPCallback);
    while(1){
        sleep(1000);
    }
    return 0;
}