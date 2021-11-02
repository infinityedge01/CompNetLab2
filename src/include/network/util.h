/**
* @file util.h
* @brief Library supporting sending/receiving IP packets encapsulated
* in an Ethernet II frame.
*/
#ifndef NETWORK_UTIL_H
#define NETWORK_UTIL_H
#include <pcap/pcap.h>
#include <netinet/ether.h>
#include <netinet/ip.h>


/**
* @brief Get IP address of a device.
*
* @param device Pointer to the device.
* @param result The address to save the parse result.
* @return 0 on success, -1 on error.
*/
int get_ip_addr(pcap_if_t *device, void *result);

/**
* @brief Get IP mask address of a device.
*
* @param device Pointer to the device.
* @param result The address to save the parse result.
* @return 0 on success, -1 on error.
*/
int get_ip_mask_addr(pcap_if_t *device, void *result);

/**
* @brief Get source address of an IP packet.
*
* @param packet Pointer to the packet.
* @param result The address to save the parse result.
* @return 0 on success, -1 on error.
*/
int get_ip_src_addr(const void *packet, void *result);

/**   
* @brief Get source address of an IP packet.
*
* @param packet Pointer to the packet.
* @param result The address to save the parse result.
* @return 0 on success, -1 on error.
*/
int get_ip_dst_addr(const void *packet, void *result);

/**
* @brief Get payload of an IP packet.
*
* @param packet Pointer to the packet.
* @param len length of the packet.
* @param result The address to save the parse result.
* @return length of payload on success, -1 on error.
*/
int get_ip_payload(const void* packet, int len, void* result);

/**
* @brief Print IP address
*
* @param addr Pointer to the IP address.
* @return 0 on success, -1 on error.
*/
int print_ip_address(const void *addr);

#endif