/**
* @file device.h
* @brief Library supporting network device management.
*/
#ifndef DEVICE_H
#define DEVICE_H

#include "packetio.h"
#include <pcap/pcap.h>

#define MAX_DEVICE_NAME_LENGTH  256
#define MAX_DEVICE              256

extern pcap_t *device_handles[MAX_DEVICE];

/**
* Init pcap devices.
*
* @return 0 on success, other on error.
*/
int initDevice();

/**
* Add a device to the library for sending/receiving packets.
*
* @param device Name of network device to send/receive packet on.
* @return A non-negative _device-ID_ on success, -1 on error.
*/
int addDevice(const char * device);
/**
* Find a device added by `addDevice `.
*
* @param device Name of the network device.
* @return A non-negative _device-ID_ on success, -1 if no such device
* was found.
*/
int findDevice(const char * device);
#endif