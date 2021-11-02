#include <network/device.h>
#include <network/util.h>
uint32_t device_ip_addr[MAX_DEVICE];
uint32_t device_ip_mask_addr[MAX_DEVICE];
int getDeviceIPAddress(int id){
    int ret = get_ip_addr(devices[id], &device_ip_addr[id]);
    if(ret != 0) {
        #ifdef DEBUG
        fprintf(stderr, "Error at get_ip_addr() in getDeviceIPAddress()\n");
        #endif
        return -1;
    }
    ret = get_ip_mask_addr(devices[id], &device_ip_mask_addr[id]);
    if(ret != 0) {
        #ifdef DEBUG
        fprintf(stderr, "Error at get_ip_mask_addr() in getDeviceIPAddress()\n");
        #endif
        return -1;
    }
    return 0;
}