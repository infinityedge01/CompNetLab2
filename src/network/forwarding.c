#include <network/ip.h>
#include <network/arp.h>
#include <network/routing.h>
#include <network/device.h>
#include <network/forwarding.h>
#include <network/util.h>
#include <link/util.h>
#include <link/packetio.h>

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int forwardPacket(const void* send_buf, int len){
    struct iphdr *hdr = (struct iphdr *)send_buf;
    unsigned char nextHopMAC[6];
    int device_id = -1;
    int ret = routing_query(hdr->daddr, nextHopMAC, &device_id);
    if(ret == -1){
        #ifdef DEBUG
            printf("routing_query() cannot found route in IPForwarding() send dst ");
            print_ip_address(&(hdr->daddr));
            printf("\n");
        #endif // DEBUG
        return -1;
    }
    if(!compare_mac_address(nextHopMAC, broadcast_address)){ // only when distance is 0
        if(((hdr->daddr & (~device_ip_mask_addr[device_id])) == (~device_ip_mask_addr[device_id]) && 
        (hdr->daddr & (device_ip_mask_addr[device_id])) == (device_ip_addr[device_id] & device_ip_mask_addr[device_id]))){ // IP broadcast
            memcpy(nextHopMAC, broadcast_address, ETH_ALEN); 
        }else{
            struct arp_record * rec;
            rec = find_arp_record(hdr->daddr);
            if(rec == NULL){
                ret = send_arp_broadcast(&(hdr->daddr), device_id);
                if(ret == -1){
                    #ifdef DEBUG
                        printf("send_arp_broadcast() in sendIPPacket() failed.\n");
                    #endif // DEBUG
                    return -1;
                }
            }else{
                memcpy(nextHopMAC, rec->mac_addr, ETH_ALEN);
            }
        }
    }
    if(htons(hdr->frag_off) & IP_DF || htons(hdr->tot_len) - ((size_t)(hdr->ihl) << 2) <= IP_FRAG_SIZE){
        hdr->check = 0;
        hdr->check = checksum((uint16_t *)hdr, ((size_t)(size_t)(hdr->ihl)) << 2);
        sendFrame(send_buf, len, ETH_P_IP, nextHopMAC, device_id);
    }else{
        for(int i = 0; i < htons(hdr->tot_len) - ((size_t)(hdr->ihl) << 2); i += IP_FRAG_SIZE){
            int flen = IP_FRAG_SIZE;
            if(htons(hdr->tot_len) - ((size_t)(hdr->ihl) << 2) - i < flen){
                flen = htons(hdr->tot_len) - ((size_t)(hdr->ihl) << 2) - i;
            }
            unsigned char * ip_data = (unsigned char *)malloc(((size_t)(hdr->ihl) << 2) + flen);
            memcpy(ip_data, hdr, ((size_t)(hdr->ihl) << 2));
            memcpy(ip_data + ((size_t)(hdr->ihl) << 2), ((void *)hdr) + ((size_t)(hdr->ihl) << 2) + i, flen);
            struct iphdr *ndata = (struct iphdr *)ip_data; 
            ndata->tot_len = htons(flen + ((size_t)(hdr->ihl) << 2));
            if(i + IP_FRAG_SIZE >= htons(hdr->tot_len) - ((size_t)(hdr->ihl) << 2)){
                ndata->frag_off = htons(htons(ndata->frag_off) + i / 8);
            }else{
                ndata->frag_off = htons((htons(ndata->frag_off) + i / 8) | IP_MF);
            }
            ndata->check = 0;
            ndata->check = checksum((uint16_t *)ndata, ((size_t)ndata->ihl) << 2);
            sendFrame(ip_data, flen + ((size_t)(hdr->ihl) << 2), ETH_P_IP, nextHopMAC, device_id);
            free(ip_data);
        }
    }
    #ifdef DEBUG
        printf("Forward IP Packet Form Src ");
        print_ip_address(&(hdr->saddr));
        printf(", Dst ");
        print_ip_address(&(hdr->daddr));
        printf(", TTL=%d, Next Hop MAC: ", hdr->ttl);
        print_mac_address(nextHopMAC);
        printf(" Len=%d\n", len);
    #endif // DEBUG
    return 0;
}

int IPForwarding(const void* buf, int len){
    unsigned char *send_buf = malloc(len);
    memcpy(send_buf, buf, len);
    struct iphdr *hdr = (struct iphdr *)send_buf;
    hdr->ttl -= 1;
    if(hdr->ttl == 0){
        #ifdef DEBUG
            printf("ttl == 0 in IPForwarding()\n");
        #endif // DEBUG
        return -1;
    }
    forwardPacket(send_buf, len);
    free(send_buf);
    return 0;
}