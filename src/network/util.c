#include <network/util.h>

#include <netinet/ether.h>
#include <netinet/ip.h>
#include <pcap/pcap.h>
#include <string.h>


int get_ip_addr(pcap_if_t *device, void *result){
    if(!device) return -1;
    if(!device->addresses) return -1;
    for (pcap_addr_t *p=device->addresses; p; p=p->next) {
        if (p->addr->sa_family != AF_INET)  continue;
        memcpy(result, (const void *)(p->addr->sa_data+2), 4);
        return 0;
    }
    return -1;
}

int get_ip_mask_addr(pcap_if_t *device, void *result){
    if(!device) return -1;
    if(!device->addresses) return -1;
    for (pcap_addr_t *p=device->addresses; p; p=p->next) {
        if (p->addr->sa_family != AF_INET)  continue;
        memcpy(result, (const void *)(p->netmask->sa_data+2), 4);
        return 0;
    }
    return -1;
}

uint16_t checksum(uint16_t *addr, size_t len) {
    unsigned long sum = 0;
    while (len > 1) {
        sum += *addr++;
        len -= 2;
    }
    if (len > 0) {
        sum += *addr & 0xFF00;
    }
    while (sum >> 16) {
        sum = (sum & 0xffff) + (sum >> 16);
    }
    sum = ~sum;
    return ((uint16_t)sum);
}


int print_ip_address(const void *addr){
    unsigned char* ptr = (unsigned char*)addr;
    printf("%d.%d.%d.%d", *ptr, *(ptr+1), *(ptr+2), *(ptr+3));
    return 0;
}