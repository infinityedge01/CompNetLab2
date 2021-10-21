#include <link/packetio.h>
#include <link/device.h>

#include <pcap/pcap.h>
#include <semaphore.h>
#include <pthread.h>
#include <string.h>
#include <malloc.h>

typedef unsigned char uchar;

pthread_mutex_t sendpacket_mutex, callback_mutex;


int sendFrame(const void * buf, int len,
int ethtype, const void * destmac, int id){
    if(len > MAX_PAYLOAD_SIZE){
        #ifdef DEBUG
        fprintf(stderr, "Error at sendFrame(): Payload too large\n");
        #endif
        return -1;
    }
    size_t frame_len = sizeof(struct ethhdr) + len + 4;
    char* framebuf = (char*)malloc(frame_len);
    struct ethhdr *hdr = (struct ethhdr *)framebuf;
    void* data = (void*)(framebuf + sizeof(struct ethhdr));
    void* checksum = (void*)(framebuf + sizeof(struct ethhdr) + len);
    
    memcpy(&hdr->h_dest, destmac, ETH_ALEN);
    memset(&hdr->h_source, 0, ETH_ALEN);
    hdr->h_proto = htons(ethtype); // convert to network endian
    memcpy(data, buf, len);
    memset(checksum, 0, 4); // leave CRC zero
    pthread_mutex_lock(&sendpacket_mutex);
    int ret = pcap_sendpacket(device_handles[id], framebuf, frame_len);
    if (ret != 0){
        pthread_mutex_unlock(&sendpacket_mutex);
        #ifdef DEBUG
        fprintf(stderr, "Error at pcap_inject() in sendFrame()\n");
        #endif
        return -1;
    }
    pthread_mutex_unlock(&sendpacket_mutex);
    return 0;
}

frameReceiveCallback registered_callback = NULL;

int setFrameReceiveCallback(frameReceiveCallback callback){
    pthread_mutex_lock(&callback_mutex);
    registered_callback = callback;
    pthread_mutex_unlock(&callback_mutex);
    return 0;
}

int start_capture(int id){
    struct pcap_pkthdr *hdr;
    uchar *packet;
    while(1){
        int ret = pcap_next_ex(device_handles[id], &hdr,(const uchar **)&packet);
        if(ret == 0){
            #ifdef DEBUG
            fprintf(stderr, "Packet Timeout in device %s\n", device[id]->name);
            #endif
        }
        if(ret < 0){
            #ifdef DEBUG
            fprintf(stderr, "Packet Error in device %s\n", device[id]->name);
            #endif
            return -1;
        }	
        if(registered_callback != NULL){
            pthread_mutex_lock(&callback_mutex);
            int callback_ret = registered_callback(packet, hdr->caplen, id);
            if(callback_ret < 0){
                #ifdef DEBUG
                fprintf(stderr, "Callback Error in device %s\n", device[id]->name);
                #endif
            }
            pthread_mutex_unlock(&callback_mutex);
        }
    }
    return 0;
}