#include <link/device.h>

#include <pcap/pcap.h>
#include <string.h>

char pcap_errbuf[PCAP_ERRBUF_SIZE];
pcap_if_t *pcap_devices;
pcap_if_t *devices[MAX_DEVICE];
pcap_t *device_handles[MAX_DEVICE];
int cntdev;

int initDevice(){
    int ret = pcap_findalldevs(&pcap_devices, pcap_errbuf);
    if(ret != 0){
        #ifdef DEBUG
        fprintf(stderr, "Error at pcap_findalldevs() in initDevice(): %s\n", pcap_errbuf);
        #endif
        return ret;
    }
    cntdev = 0;
    return 0;
}

int addDevice(const char * device){
    size_t len = strlen(device);
    if(len == 0 || len >= MAX_DEVICE_NAME_LENGTH){
        return -1;
    }
    for (pcap_if_t *p = pcap_devices; p; p = p->next) {
        if(strlen(p->name) != len) continue;
        if(strcmp(device, p->name) == 0) {
            devices[cntdev] = p;
            break;
        }
    }
    pcap_t *handle = pcap_open_live(device, 65536, 0, 1000, pcap_errbuf);
    if(handle == NULL){
        #ifdef DEBUG
        fprintf(stderr, "Error at pcap_open_live() in addDevice(): %s\n", pcap_errbuf);
        #endif
        return -1;
    }
    device_handles[cntdev] = handle;
    return cntdev++;
}

int findDevice(const char * device){
    size_t len = strlen(device);
    if(len == 0 || len >= MAX_DEVICE_NAME_LENGTH){
        return -1;
    }
    for (int i = 0; i < cntdev; i ++) { 
        if(strlen(devices[cntdev]->name) != len) continue;
        if(strcmp(device, devices[cntdev]->name) == 0) {
            return i;
        }
    }
    return -1;
}