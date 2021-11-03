/**
* @file forwarding.h
* @brief Library supporting IP forwarding.
*/
#ifndef NETWORK_FORWARDING_H
#define NETWORK_FORWARDING_H

int forwardPacket(const void* send_buf, int len);
int IPForwarding(const void* buf, int len);

#endif