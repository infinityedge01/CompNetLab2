#include <link/packetio.h>
#include <link/device.h>
#include <link/util.h>
#include <pcap/pcap.h>
#include <semaphore.h>
#include <pthread.h>
#include <string.h>
#include <malloc.h>
#include<sys/time.h>
typedef unsigned char uchar;
unsigned char broadcast_address[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
pthread_mutex_t sendpacket_mutex, callback_mutex;


int sendFrame(const void * buf, int len,
int ethtype, const void * destmac, int id){
    if(len > MAX_PAYLOAD_SIZE){
        #ifdef DEBUG
        fprintf(stderr, "Error at sendFrame(): Payload too large\n");
        #endif
        return -1;
    }
    size_t frame_len = sizeof(struct ethhdr) + len;
    char* framebuf = (char*)malloc(frame_len);
    struct ethhdr *hdr = (struct ethhdr *)framebuf;
    void* data = (void*)(framebuf + sizeof(struct ethhdr));
    //void* checksum = (void*)(framebuf + sizeof(struct ethhdr) + len);
    
    memcpy(&hdr->h_dest, destmac, ETH_ALEN);
    memcpy(&hdr->h_source, device_mac_addr[id], ETH_ALEN);
    hdr->h_proto = htons(ethtype); // convert to network endian
    memcpy(data, buf, len);
    //memset(checksum, 0, 4); // leave CRC zero
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

int sendBroadcastFrame(const void * buf, int len,
int ethtype, int id){
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

    memset(&hdr->h_dest, 0xff, ETH_ALEN);
    memcpy(&hdr->h_source, device_mac_addr[id], ETH_ALEN);
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

frameReceiveCallback registered_callback[65536];

int setFrameReceiveCallback(frameReceiveCallback callback, uint16_t protocol){
    pthread_mutex_lock(&callback_mutex);
    registered_callback[protocol] = callback;
    pthread_mutex_unlock(&callback_mutex);
    return 0;
}

int init_mutex(void){
    pthread_mutex_init(&sendpacket_mutex, NULL);
    pthread_mutex_init(&callback_mutex, NULL);
    return 0;
}

int start_capture(int id){
    struct pcap_pkthdr *hdr;
    uchar *packet;
    while(1){
        //struct timespec t;
        //clock_gettime(CLOCK_REALTIME, &t);
        //printf("get1:%lld:%lld\n", (long long)t.tv_sec, (long long)t.tv_nsec);

        int ret = pcap_next_ex(device_handles[id], &hdr,(const uchar **)&packet);
        //clock_gettime(CLOCK_REALTIME, &t);
        //printf("get2:%lld:%lld\n", (long long)t.tv_sec, (long long)t.tv_nsec);
        
        if(ret == 0){
            #ifdef DEBUG
            fprintf(stderr, "Packet Timeout in device %s\n", devices[id]->name);
            #endif
        }
        if(ret < 0){
            #ifdef DEBUG
            fprintf(stderr, "Packet Error in device %s\n", devices[id]->name);
            #endif
            return -1;
        }
        struct ethhdr *ehdr = (struct ethhdr *)packet;
        if(compare_mac_address(ehdr->h_dest, device_mac_addr[id]) && compare_mac_address(ehdr->h_dest, broadcast_address)) continue;
        uint16_t protocol = htons(ehdr->h_proto);
        if(registered_callback[protocol] != NULL){
            pthread_mutex_lock(&callback_mutex);
            int callback_ret = registered_callback[protocol](packet, hdr->caplen, id);
            if(callback_ret < 0){
                #ifdef DEBUG
                fprintf(stderr, "Callback Error in device %s\n", devices[id]->name);
                #endif
            }
            pthread_mutex_unlock(&callback_mutex);
        }
    }
    return 0;
}

void * start_capture_pthread(void *arg){
    int id = (int)(long)arg;
    start_capture(id);
}