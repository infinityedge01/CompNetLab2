/**
* @file packetio.h
* @brief Library supporting sending/receiving Ethernet II frames.
*/
#ifndef PACKETIO_H
#define PACKETIO_H

#include <netinet/ether.h>

#define MAX_ETH_PACKET_SIZE     2048
#define MAX_PAYLOAD_SIZE        1500
#define MIN_PAYLOAD_SIZE        46

extern unsigned char broadcast_address[6];

/**
* @brief Encapsulate some data into an Ethernet II frame and send it.
*
* @param buf Pointer to the payload.
* @param len Length of the payload.
* @param ethtype EtherType field value of this frame.
* @param destmac MAC address of the destination.
* @param id ID of the device(returned by `addDevice`) to send on.
* @return 0 on success, -1 on error.
* @see addDevice
*/
int sendFrame(const void * buf, int len,
int ethtype, const void * destmac, int id);
/**
* @brief Encapsulate some data into an Ethernet II frame and broadcast it.
*
* @param buf Pointer to the payload.
* @param len Length of the payload.
* @param ethtype EtherType field value of this frame.
* @param id ID of the device(returned by `addDevice`) to send on.
* @return 0 on success, -1 on error.
* @see addDevice
*/
int sendBroadcastFrame(const void * buf, int len,
int ethtype, int id);
/**
* @brief Process a frame upon receiving it.
*
* @param buf Pointer to the frame.
* @param len Length of the frame.
* @param id ID of the device(returned by `addDevice`) receiving
* current frame.
* @return 0 on success, -1 on error.
* @see addDevice
*/
typedef int (*frameReceiveCallback)(const void *, int, int);
/**
* @brief Register a callback function to be called each time an
* Ethernet II frame was received.
*
* @param callback the callback function.
* @return 0 on success, -1 on error.
* @see frameReceiveCallback
*/
int setFrameReceiveCallback(frameReceiveCallback callback, uint16_t protocol);

/**
* @brief Init the mutexes.
*
* @return 0 on success, -1 on error.
*/
int init_mutex(void);

/**
* @brief Start capture a device.
*
* @param id the id of the device.
* @return 0 on success, -1 on error.
*/
int start_capture(int id);

/**
* @brief start_capture for pthread use.
*
* @param id the id of the device.
* @see start_capture
*/
void * start_capture_pthread(void *arg);

#endif