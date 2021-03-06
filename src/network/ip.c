#include <network/ip.h>
#include <network/arp.h>
#include <network/routing.h>
#include <network/forwarding.h>
#include <network/device.h>
#include <network/util.h>
#include <link/util.h>
#include <link/packetio.h>

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
uint16_t cur_packet_id = 0;

IPPacketReceiveCallback ip_callback;
pthread_mutex_t ip_callback_mutex, cur_packet_id_mutex;

unsigned char ip_fragment_buf[MAX_DEVICE][1600];

int ip_fragment_len[MAX_DEVICE];

int IPEthCallback(const void* buf, int len, int id);
 
int IP_init(){
    ip_callback = NULL;
    pthread_mutex_init(&ip_callback_mutex, NULL);
    pthread_mutex_init(&cur_packet_id_mutex, NULL);
    
    setFrameReceiveCallback(IPEthCallback, ETH_P_IP);
    return 0;
}

int sendIPPacket(const struct in_addr src, const struct in_addr dest,
int proto, const void *buf, int len){
    int ret;
    size_t send_len = sizeof(struct iphdr) + len;
    unsigned char *send_buf = malloc(send_len);
    struct iphdr *hdr = (struct iphdr *)send_buf;

    hdr->ihl = 5;
    hdr->version = 4;
    hdr->tos = 0;
    hdr->tot_len = htons(send_len);
    pthread_mutex_lock(&cur_packet_id_mutex);
    cur_packet_id ++;
    hdr->id = htons(cur_packet_id);
    pthread_mutex_unlock(&cur_packet_id_mutex);
    //hdr->frag_off = htons(IP_DF);
    hdr->frag_off = 0;
    hdr->ttl = 64;
    hdr->protocol = proto;
    hdr->saddr = src.s_addr;
    hdr->daddr = dest.s_addr;
    void *ip_payload = ((void *)send_buf) + (((size_t)hdr->ihl) << 2);
    memcpy(ip_payload, buf, len);

    ret = forwardPacket(send_buf, send_len);
    free(send_buf);
    return ret;
}

int IPEthCallback(const void* buf, int len, int id) {
    int ret;
    const struct ethhdr *hdr = buf;
    const struct iphdr *data = (struct iphdr *)(buf + ETH_HLEN);
    int device_id = -1;
    for(int i = 0; i < cntdev; i ++){
        if(data->daddr == device_ip_addr[i] || 
        ((data->daddr & (~device_ip_mask_addr[i])) == (~device_ip_mask_addr[i]) && 
        (data->daddr & (device_ip_mask_addr[i])) == (device_ip_addr[i] & device_ip_mask_addr[i]))){
            device_id = i;
            break;
        }
    }
    if(device_id != -1){
        pthread_mutex_lock(&ip_callback_mutex);
        int valid_flag = 1;
        if((htons(data->frag_off) & IP_OFFMASK) == 0){
            memcpy(ip_fragment_buf[device_id], ((void *)data) + ((size_t)(data->ihl) << 2), htons(data->tot_len) - ((size_t)(data->ihl) << 2));
            ip_fragment_len[device_id] = htons(data->tot_len) - ((size_t)(data->ihl) << 2);
        }else{
            if((htons(data->frag_off) & IP_OFFMASK) * 8 != ip_fragment_len[device_id]){
                ip_fragment_len[device_id] = 0;
                valid_flag = 0;
            }else{
                memcpy(ip_fragment_buf[device_id] + ip_fragment_len[device_id], ((void *)data) + ((size_t)(data->ihl) << 2), htons(data->tot_len) - ((size_t)(data->ihl) << 2));
                ip_fragment_len[device_id] += htons(data->tot_len) - ((size_t)(data->ihl) << 2);
            }
        }
        if(!(htons(data->frag_off) & IP_MF) && valid_flag){
            if(ip_callback == NULL) ret = 0;
            else{
                unsigned char * ip_data = (unsigned char *)malloc(((size_t)(data->ihl) << 2) + ip_fragment_len[device_id]);
                memcpy(ip_data, data, ((size_t)(data->ihl) << 2));
                memcpy(ip_data + ((size_t)(data->ihl) << 2), ip_fragment_buf[device_id], ip_fragment_len[device_id]);
                struct iphdr *ndata = (struct iphdr *)ip_data; 
                ndata->tot_len = htons(((size_t)(data->ihl) << 2) + ip_fragment_len[device_id]);
                ret = ip_callback((void *)ndata, htons(ndata->tot_len));
                free(ip_data);
            }
        }
        pthread_mutex_unlock(&ip_callback_mutex);
        return ret;
    }
    if(!compare_mac_address(hdr->h_dest, broadcast_address)){ // distance = 0
        return 0;
    }
    ret = IPForwarding(buf + ETH_HLEN, htons(data->tot_len));
    return ret;
}

int setIPPacketReceiveCallback(IPPacketReceiveCallback callback) {
    pthread_mutex_lock(&ip_callback_mutex);
    ip_callback = callback;
    pthread_mutex_unlock(&ip_callback_mutex);
    return 0;
}

int setRoutingTable(const struct in_addr dest,
const struct in_addr mask,
const void * nextHopMAC, const char * device){
    int device_id = -1;
    for(int i = 0; i < cntdev; i ++){
        if(strcmp(device, devices[i]->name) == 0) {
            device_id = i;
            break;
        }
    }
    if(device_id == -1){
        #ifdef DEBUG
            printf("cannot find device %s.\n", device);
        #endif // DEBUG
        return -1;
    }
    int ret = user_add_routing_entry(dest.s_addr, mask.s_addr, nextHopMAC, device_id);
    if(ret == -1){
        #ifdef DEBUG
            printf("user_add_routing_entry() in setRoutingTable() failed\n");
        #endif // DEBUG
        return -1;
    }
    return 0;
}