/**
* @file device.h
* @brief Library supporting network device management(in network layer).
*/
#ifndef NETWORK_DEVICE_H
#define NETWORK_DEVICE_H
#include <link/device.h>
extern uint32_t device_ip_addr[MAX_DEVICE];

/**
* Get device ip address.
*
* @param id device id
* @return 0 on success, -1 on error.
*/
int getDeviceIPAddress(int id);


#endif