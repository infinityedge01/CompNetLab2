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

int print_ip_address(const void *addr){
    unsigned char* ptr = (unsigned char*)addr;
    printf("%d.%d.%d.%d", *ptr, *(ptr+1), *(ptr+2), *(ptr+3));
    return 0;
}