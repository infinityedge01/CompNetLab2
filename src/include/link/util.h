/**
* @file packetio.h
* @brief Library supporting parsing Ethernet II frames.
*/
#ifndef LINK_UTIL_H
#define LINK_UTIL_H
#include <pcap.h>
#include <netinet/ether.h>


/**
* @brief Get MAC address of a device.
*
* @param device Pointer to the device.
* @param result The address to save the parse result.
* @return 0 on success, -1 on error.
*/
int get_mac_addr(pcap_if_t *device, const void *result);

/**
* @brief Get source address of an Ethernet II frame.
*
* @param frame Pointer to the frame.
* @param result The address to save the parse result.
* @return 0 on success, -1 on error.
*/
int get_src_addr(const void *frame, const void *result);

/**
* @brief Get source address of an Ethernet II frame.
*
* @param frame Pointer to the frame.
* @param result The address to save the parse result.
* @return 0 on success, -1 on error.
*/
int get_dst_addr(const void *frame, const void *result);

/**
* @brief Get payload of an Ethernet II frame.
*
* @param frame Pointer to the frame.
* @param len length of the frame.
* @param result The address to save the parse result.
* @return length of payload on success, -1 on error.
*/
int get_payload(const void* frame, int len, const void* result);

#endif