#include <link/util.h>

#include <netinet/ether.h>
#include <pcap/pcap.h>
#include <string.h>


int get_mac_addr(pcap_if_t *device, void *result){
    if(!device) return -1;
    if(!device->addresses) return -1;
    for (pcap_addr_t *p=device->addresses; p; p=p->next) {
        if (p->addr->sa_family != AF_PACKET)  continue;
        memcpy(result, (const void *)(p->addr->sa_data+10), ETH_ALEN);
        return 0;
    }
    return -1;
}

int get_src_addr(const void *frame, void *result){
    memcpy(result, (const void *)(frame + ETH_ALEN), ETH_ALEN);
    return 0;
}

int get_dst_addr(const void *frame, void *result){
    memcpy(result, (const void *)frame, ETH_ALEN);
    return 0;
}

int get_payload(const void* frame, int len, void* result){
    int ret = len - 2 * ETH_ALEN - 2 - 4;
    memcpy(result, (const void *)(frame + 2 * ETH_ALEN + 2), ret);
    return ret;
}

int print_mac_address(const void *addr){
    unsigned char* ptr = (unsigned char*)addr;
    printf("%02x:%02x:%02x:%02x:%02x:%02x", *ptr, *(ptr+1), *(ptr+2), *(ptr+3), *(ptr+4), *(ptr+5));
    return 0;
}

int compare_mac_address(const void *addr1, const void *addr2){
    unsigned char* ptr1 = (unsigned char*)addr1;
    unsigned char* ptr2 = (unsigned char*)addr2;
    for(int i = 0; i < ETH_ALEN; i ++){
        if(*ptr1 != *ptr2) return 1;
        ptr1++;
        ptr2++;
    }
    return 0;
}