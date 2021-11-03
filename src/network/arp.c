#include <link/packetio.h>
#include <link/util.h>
#include <network/device.h>
#include <network/arp.h>
#include <network/util.h>

#include <pthread.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

struct arp_record arprecord[256];
pthread_t arp_pthread;

uint64_t gettime_ms() {
    struct timespec t;
    timespec_get(&t, TIME_UTC);
    return (t.tv_nsec + ((uint64_t)t.tv_sec * 1000000000)) / 1000000;
}

struct arp_record *find_arp_record(uint32_t ip){
    uint64_t current_time = gettime_ms();
    for(int i = 0; i < MAX_ARP_RECORD; i ++){
        if(ip == arprecord[i].ip_addr && current_time - arprecord[i].timestamp < ARP_TIMEOUT_MS){
            return &arprecord[i];
        }
    }
    return NULL;
}

struct arp_record *find_evict_arp_record(){
    struct arp_record *ret = &arprecord[0];
    for(int i = 0; i < MAX_ARP_RECORD; i ++){
        if(arprecord[i].timestamp < ret->timestamp){
            ret = &arprecord[i];
        }
    }
    return ret;
}

int send_arp_reply(const void* dst_mac_addr, const void* dst_ip_addr, int id){
    struct arp_header *data = malloc(sizeof(struct arp_header));
    data->hardware_type = htons(0x0001);
    data->protocol_type = htons(ETH_P_IP);
    data->hardware_size = ETH_ALEN;
    data->protocol_size = 0x04;
    data->opcode = htons(0x02);
    memcpy(data->source_ip_addr, &device_ip_addr[id], 0x04);
    memcpy(data->source_mac_addr, device_mac_addr[id], ETH_ALEN);
    memcpy(data->target_ip_addr, dst_ip_addr, 0x04);
    memcpy(data->target_mac_addr, dst_mac_addr, ETH_ALEN);
    int ret = sendFrame((const void *)data, sizeof(struct arp_header), ETH_P_ARP, dst_mac_addr, id);
    if(ret == -1){
        #ifdef DEBUG
            printf("sendFrame() in send_arp_reply() failed.\n");
        #endif // DEBUG
        free(data);
        return -1;
    }
    free(data);
    return 0;
}

int send_arp_broadcast(const void* dst_ip_addr, int id){
    struct arp_header *data = malloc(sizeof(struct arp_header));
    data->hardware_type = htons(0x0001);
    data->protocol_type = htons(ETH_P_IP);
    data->hardware_size = ETH_ALEN;
    data->protocol_size = 0x04;
    data->opcode = htons(0x01);
    memcpy(data->source_ip_addr, &device_ip_addr[id], 0x04);
    memcpy(data->source_mac_addr, device_mac_addr[id], ETH_ALEN);
    memcpy(data->target_ip_addr, dst_ip_addr, 0x04);
    memset(data->target_mac_addr, 0, ETH_ALEN);
    int ret = sendBroadcastFrame((const void *)data, sizeof(struct arp_header), ETH_P_ARP, id);
    if(ret == -1){
        #ifdef DEBUG
            printf("sendBroadcastFrame() in send_arp_reply() failed.\n");
        #endif // DEBUG
        free(data);
        return -1;
    }
    free(data);
    return 0;
}

int arp_frame_handler(const void* buf, int len, int id) {
    const struct ethhdr *hdr = buf;
    const struct arp_header *data = (struct arp_header *)(buf + ETH_HLEN);

    #ifdef DEBUG
        printf("Received ARP Packet from Mac:"); print_mac_address(data->source_mac_addr);
        printf(" IP:"); print_ip_address(data->source_ip_addr);
        printf("\n");
    #endif
    
    struct arp_record *rec;
    rec = find_arp_record(*((uint32_t *)data->source_ip_addr));
    if (rec) {
        memcpy(rec->mac_addr, data->source_mac_addr, ETH_ALEN);
    } else {
        rec = find_evict_arp_record();
        rec->ip_addr = *((uint32_t *)data->source_ip_addr);
        memcpy(rec->mac_addr, data->source_mac_addr, ETH_ALEN);
    }
    rec->timestamp = gettime_ms();
    /*
    if(data->opcode == htons(0x01) && *((uint32_t *)data->target_ip_addr) == device_ip_addr[id]){
        int ret = send_arp_reply((const void *)device_mac_addr[id], (const void *)(&device_mac_addr[id]), id);
        if(ret == -1){
            #ifdef DEBUG
                printf("send_arp_reply() in arp_frame_handler() failed.\n");
            #endif // DEBUG
            return -1;
        }
    }
    */
    return 0;
}

void arp_routine(){
    while(1){
        sleep(5);
        for(int i = 0; i < cntdev; i ++){
            send_arp_broadcast(&device_ip_addr[i], i);
        }
    }
}

void * arp_routine_pthread(void *arg){
    arp_routine();
}

int arp_init() {
    setFrameReceiveCallback(arp_frame_handler, ETH_P_ARP);
    pthread_create(&arp_pthread, NULL, &arp_routine_pthread, NULL);
    return 0;
}

