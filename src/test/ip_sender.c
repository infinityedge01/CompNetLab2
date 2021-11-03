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
    setIPPacketReceiveCallback(IPCallback);
    struct in_addr src, dst;
    inet_pton(AF_INET, "10.100.1.1", &src.s_addr);
    inet_pton(AF_INET, "10.100.3.2", &dst.s_addr);
    char send_data[] = "Good night. And see you tomorrow, Miss Diana.";
    while(1){
        sleep(1);
        sendIPPacket(src, dst, 192, send_data, strlen(send_data));
    }
    return 0;
}