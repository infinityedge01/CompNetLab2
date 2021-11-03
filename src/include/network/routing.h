/**
* @file routing.h
* @brief Library supporting routing service.
*/
#ifndef NETWORK_ROUTING_H
#define NETWORK_ROUTING_H

#include <network/device.h>

#include <netinet/ether.h>
#include <netinet/ip.h>

#define ETH_P_MYROUTE 0x1102
#define MAX_ROUTING_RECORD MAX_DEVICE
#define ROUTING_DISTANCE_INFINITY 16
struct routing_entry {
    uint32_t ip_addr;
    uint32_t ip_mask;
    unsigned char dest_mac_addr[6];
    uint16_t distance;
    int device_id;
};

struct routing_header{
    unsigned char source_mac_addr[6];
    unsigned char dest_mac_addr[6];
    uint16_t opcode; // 0x01 request 0x02 reply
    uint32_t request_id;
    int routing_entry_count; // leave zero for request
};

struct routing_record{
    struct routing_header header;
    struct routing_entry *entry;
};
int user_add_routing_entry(uint32_t ip_addr, uint32_t ip_mask, const void *nextHopMAC, int device_id);
int route_init();
int routing_query(uint32_t ip_addr, void* dest_mac_addr, int* device_id);
void print_routing_table();
#endif