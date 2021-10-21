#include <link/device.h>
#include <link/packetio.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv){
    initDevice();
    char device_name[] = "veth1-2";
    int id = addDevice((const char *)device_name);
    char data[] = "Hello, I'm Diana!";
    char mac[] = {0x7e, 0x6d, 0x95, 0x84, 0x74, 0xb9};
    while(1){
        sendFrame((const void *)data, strlen(data), 0x0800, (const void *)mac, id);
        sleep(1);
    }
    return 0;
}