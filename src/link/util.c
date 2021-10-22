#include <link/util.h>

#include <netinet/ether.h>
#include <pcap/pcap.h>
#include <string.h>


int get_mac_addr(pcap_if_t *device, void *result){
    if(!device) return -1;
    if(!device->addresses) return -1;
    if(!device->addresses->addr) return -1;
    if(!device->addresses->addr->sa_data) return -1;
    void* p = ((void *)device->addresses->addr->sa_data) + 10;
    memcpy(result, (const void *)p, ETH_ALEN);
    return 0;
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