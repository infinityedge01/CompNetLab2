/**
* @file arp.h
* @brief Library supporting arp service.
*/
#ifndef NETWORK_ARP_H
#define NETWORK_ARP_H
#include <netinet/ether.h>
#include <netinet/ip.h>

#define ARP_TIMEOUT_MS 6000
#define MAX_ARP_RECORD 64

struct arp_header{
    uint16_t hardware_type; // 0x0001
    uint16_t protocol_type; // 0x0800
    uint8_t hardware_size; // 0x06
    uint8_t protocol_size; // 0x04
    uint16_t opcode; // request 0x01 reply 0x02
    unsigned char source_mac_addr[6];
    unsigned char source_ip_addr[4];
    unsigned char target_mac_addr[6];
    unsigned char target_ip_addr[4];
};

struct arp_record{
    uint64_t timestamp;
    uint32_t ip_addr;
    unsigned char mac_addr[6];
};

uint64_t gettime_ms();
struct arp_record *find_arp_record(uint32_t ip);
struct arp_record *find_evict_arp_record();
int send_arp_reply(const void* dst_mac_addr, const void* dst_ip_addr, int id);
int send_arp_broadcast(const void* dst_ip_addr, int id);
int arp_init();

#endif