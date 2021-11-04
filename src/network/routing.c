#include <network/routing.h>
#include <network/arp.h>
#include <network/util.h>
#include <link/util.h>

#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

struct routing_entry user_routing_entries[MAX_ROUTING_RECORD];
int user_routing_entry_count;

struct routing_entry *routing_entries;
int routing_entry_count;

struct routing_record record[MAX_ROUTING_RECORD];
int routing_record_count;
uint32_t routing_request_id;

pthread_mutex_t user_routing_entry_mutex;
pthread_mutex_t routing_table_mutex;
pthread_mutex_t routing_record_mutex;
pthread_mutex_t routing_reader_mutex;
pthread_t routing_pthread;

int routing_reader_cnt;

int init_routing_entry(){
    user_routing_entry_count = 0;
    routing_entry_count = cntdev;
    routing_entries = malloc(routing_entry_count * sizeof(struct routing_entry));
    for(int i = 0; i < routing_entry_count; i ++){
        routing_entries[i].device_id = i;
        memset(routing_entries[i].dest_mac_addr, 0xff, ETH_ALEN);
        routing_entries[i].ip_addr = device_ip_addr[i] & device_ip_mask_addr[i];
        routing_entries[i].ip_mask = device_ip_mask_addr[i];
        routing_entries[i].distance = 0;
    }
}

int user_add_routing_entry(uint32_t ip_addr, uint32_t ip_mask, const void *nextHopMAC, int device_id){
    pthread_mutex_lock(&user_routing_entry_mutex);
    if(user_routing_entry_count == MAX_ROUTING_RECORD){
        #ifdef DEBUG
            printf("user_routing_entry full in user_add_routing_entry().\n");
        #endif // DEBUG
        pthread_mutex_unlock(&user_routing_entry_mutex);
        return -1;
    }
    user_routing_entries[user_routing_entry_count].ip_addr = ip_addr;
    user_routing_entries[user_routing_entry_count].ip_mask = ip_mask;
    memcpy(user_routing_entries[user_routing_entry_count].dest_mac_addr, nextHopMAC, ETH_ALEN);
    user_routing_entries[user_routing_entry_count].distance = 0;
    user_routing_entries[user_routing_entry_count].device_id = device_id;
    pthread_mutex_unlock(&user_routing_entry_mutex);
}

int send_route_request(int id){
    size_t buf_size = sizeof(struct routing_header);
    unsigned char *send_buf = malloc(buf_size);
    struct routing_header *send_hdr = (struct routing_header *)(send_buf);
    memcpy(send_hdr->source_mac_addr, device_mac_addr[id], ETH_ALEN);
    memset(send_hdr->dest_mac_addr, 0xff, ETH_ALEN);
    send_hdr->opcode = htons(0x0001);
    send_hdr->request_id = routing_request_id;
    send_hdr->routing_entry_count = 0;
    int ret = sendBroadcastFrame(send_buf, buf_size, ETH_P_MYROUTE, id);
    if(ret < 0){
        #ifdef DEBUG
            printf("sendBroadcastFrame() in send_route_request() failed.\n");
        #endif // DEBUG
        free(send_buf);
        return -1;
    }
    free(send_buf);
    return 0;
}


void update_routing_table(){
    pthread_mutex_lock(&routing_record_mutex);
    pthread_mutex_lock(&user_routing_entry_mutex);
    int max_record = cntdev + user_routing_entry_count;
    for(int i = 0; i < routing_record_count; i++){
        max_record += record[i].header.routing_entry_count;
    }
    struct routing_entry *cur_entries = malloc(max_record * sizeof(struct routing_entry));
    for(int i = 0; i < cntdev; i ++){
        cur_entries[i].device_id = i;
        memset(cur_entries[i].dest_mac_addr, 0xff, ETH_ALEN);
        cur_entries[i].ip_addr = device_ip_addr[i] & device_ip_mask_addr[i];
        cur_entries[i].ip_mask = device_ip_mask_addr[i];
        cur_entries[i].distance = 0;
    }
    for(int i = 0; i < user_routing_entry_count; i ++){
        cur_entries[i + cntdev].device_id = user_routing_entries[i].device_id;
        memset(cur_entries[i + cntdev].dest_mac_addr, 0xff, ETH_ALEN);
        cur_entries[i + cntdev].ip_addr = user_routing_entries[i].ip_addr;
        cur_entries[i + cntdev].ip_mask = user_routing_entries[i].ip_mask;
        cur_entries[i + cntdev].distance = 0;
    }
    int cur_record_cnt = cntdev + user_routing_entry_count;
    pthread_mutex_unlock(&user_routing_entry_mutex);
    for(int i = 0; i < routing_record_count; i ++){
        struct routing_entry *record_entries = record[i].entry;
        int devid = -1;
        for(int l = 0; l < cntdev; l ++){
            if(!compare_mac_address(record[i].header.dest_mac_addr, device_mac_addr[l])){
                devid = l;
                break;
            }
        }
        if(devid == -1) continue;
        for(int j = 0; j < record[i].header.routing_entry_count; j ++){
            if(!compare_mac_address(record_entries[j].dest_mac_addr, record[i].header.dest_mac_addr)) continue;
            if(record_entries[j].distance + 1 >= ROUTING_DISTANCE_INFINITY) continue;
            int flag = 0;
            for(int k = 0; k < cur_record_cnt; k ++){
                if(record_entries[j].ip_addr != cur_entries[k].ip_addr ||
                record_entries[j].ip_mask != cur_entries[k].ip_mask){
                    continue;
                }
                flag = 1;
                if(record_entries[j].distance + 1 >= cur_entries[k].distance) continue;
                cur_entries[k].distance = record_entries[j].distance + 1;
                memcpy(cur_entries[k].dest_mac_addr, record[i].header.source_mac_addr, ETH_ALEN);
                cur_entries[k].device_id = devid;
            }
            if(flag == 0){
                cur_entries[cur_record_cnt].ip_addr = record_entries[j].ip_addr;
                cur_entries[cur_record_cnt].ip_mask = record_entries[j].ip_mask;
                cur_entries[cur_record_cnt].distance = record_entries[j].distance + 1;
                memcpy(cur_entries[cur_record_cnt].dest_mac_addr, record[i].header.source_mac_addr, ETH_ALEN);
                cur_entries[cur_record_cnt].device_id = devid;
                cur_record_cnt ++;
            }
        }
    }
    pthread_mutex_lock(&routing_table_mutex);
    routing_entry_count = cur_record_cnt;
    free(routing_entries);
    routing_entries = malloc(routing_entry_count * sizeof(struct routing_entry));
    memcpy(routing_entries, cur_entries, routing_entry_count * sizeof(struct routing_entry));
    pthread_mutex_unlock(&routing_table_mutex);
    free(cur_entries);
    for(int i = 0; i < routing_record_count; i ++){
        free(record[i].entry);
    }
    routing_record_count = 0;
    pthread_mutex_unlock(&routing_record_mutex);
    #ifdef DEBUG
        print_routing_table();
    #endif // DEBUG
}

int route_frame_handler(const void* buf, int len, int id) {
    const struct ethhdr *hdr = buf;
    const struct routing_header *routing_hdr = (struct routing_header *)(buf + ETH_HLEN);
    #ifdef DEBUG
        printf("Received routing frame from mac address: ");
        print_mac_address(hdr->h_source);
        printf(" Source Addr: ");
        print_mac_address(routing_hdr->source_mac_addr);
        printf(" Dest Addr: ");
        print_mac_address(routing_hdr->dest_mac_addr);
        printf(" Op Code: %u", htons(routing_hdr->opcode));
        printf(" Request Id: %u", routing_hdr->request_id);
        printf(" Routing entry count: %u\n", routing_hdr->routing_entry_count);

    #endif // DEBUG
        
    if(routing_hdr->opcode == htons(0x0001)){
        pthread_mutex_lock(&routing_table_mutex);
        size_t buf_size = sizeof(struct routing_header) + routing_entry_count * sizeof(struct routing_entry);
        unsigned char *send_buf = malloc(buf_size);
        struct routing_header *send_hdr = (struct routing_header *)(send_buf);
        memcpy(send_hdr->source_mac_addr, device_mac_addr[id], ETH_ALEN);
        memcpy(send_hdr->dest_mac_addr, hdr->h_source, ETH_ALEN);
        send_hdr->opcode = htons(0x0002);
        send_hdr->request_id = routing_hdr->request_id;
        send_hdr->routing_entry_count = routing_entry_count;
        memcpy((void *)(send_buf + sizeof(struct routing_header)), routing_entries, routing_entry_count * sizeof(struct routing_entry));
        pthread_mutex_unlock(&routing_table_mutex);
        int ret = sendFrame(send_buf, buf_size, ETH_P_MYROUTE, hdr->h_source, id);
        if(ret < 0){
            #ifdef DEBUG
                printf("sendFrame() in route_frame_handler() failed.\n");
            #endif // DEBUG
            free(send_buf);
            return -1;
        }
        free(send_buf);
        return 0;
    }else{
        if(routing_hdr->request_id != routing_request_id){
            #ifdef DEBUG
                printf("Received an outdated routing response id %u, current request id %u.\n", routing_hdr->request_id, routing_request_id);
            #endif // DEBUG
            return 0;
        }
        pthread_mutex_lock(&routing_record_mutex);
        memcpy(&record[routing_record_count].header, routing_hdr, sizeof(struct routing_header));
        record[routing_record_count].entry = malloc(routing_hdr->routing_entry_count * sizeof(struct routing_entry));
        memcpy(record[routing_record_count].entry, (buf + ETH_HLEN + sizeof(struct routing_header)), routing_hdr->routing_entry_count * sizeof(struct routing_entry));
        routing_record_count ++;
        pthread_mutex_unlock(&routing_record_mutex);
    }
    return 0;
}


void routing_routine(){
    while(1){
        sleep(1);
        for(int i = 0; i < cntdev; i ++){
            send_route_request(i);
        }
        sleep(9);
        update_routing_table();
        routing_request_id ++;
    }
}

void * routing_routine_pthread(void *arg){
    routing_routine();
}

int route_init() {
    int ret = init_routing_entry();
    if(ret < 0){
        #ifdef DEBUG
            printf("init_routing_entry() in route_init() failed.\n");
        #endif // DEBUG
        return -1;
    }
    pthread_mutex_init(&user_routing_entry_mutex, NULL);
    pthread_mutex_init(&routing_table_mutex, NULL);
    pthread_mutex_init(&routing_record_mutex, NULL);
    pthread_mutex_init(&routing_reader_mutex, NULL);
    routing_reader_cnt = 0;
    setFrameReceiveCallback(route_frame_handler, ETH_P_MYROUTE);
    pthread_create(&routing_pthread, NULL, &routing_routine_pthread, NULL);
    // set function in event loop
    return 0;
}

int routing_query(uint32_t ip_addr, void* dest_mac_addr, int* device_id){
    uint32_t max_mask = 0;
    int entry_id = -1;
    pthread_mutex_lock(&routing_reader_mutex);
    routing_reader_cnt ++;
    if(routing_reader_cnt == 1){
        pthread_mutex_lock(&routing_table_mutex);
    }
    pthread_mutex_unlock(&routing_reader_mutex);
    for(int i = 0; i < routing_entry_count; i ++){
        if((ip_addr & routing_entries[i].ip_mask) == routing_entries[i].ip_addr){
            if(entry_id == -1 || routing_entries[i].ip_mask > max_mask){
                entry_id = i;
                max_mask = routing_entries[i].ip_mask;
            }
        }
    }
    if(entry_id != -1){
        memcpy(dest_mac_addr, routing_entries[entry_id].dest_mac_addr, ETH_ALEN);
        *device_id = routing_entries[entry_id].device_id;
    }
    pthread_mutex_lock(&routing_reader_mutex);
    routing_reader_cnt --;
    if(routing_reader_cnt == 0){
        pthread_mutex_unlock(&routing_table_mutex);
    }
    pthread_mutex_unlock(&routing_reader_mutex);
    if(entry_id == -1) return -1;
    return 0;
}

void print_routing_table(){
    pthread_mutex_lock(&routing_reader_mutex);
    routing_reader_cnt ++;
    if(routing_reader_cnt == 1){
        pthread_mutex_lock(&routing_table_mutex);
    }
    pthread_mutex_unlock(&routing_reader_mutex);
    printf("Routing table length: %d\n", routing_entry_count);
    for(int i = 0; i < routing_entry_count; i++){
        printf("Routing entry %d: \n", i);
        printf("IP Addr: ");
        print_ip_address(&routing_entries[i].ip_addr);
        printf("\nIP Mask: ");
        print_ip_address(&routing_entries[i].ip_mask);
        printf("\nDst Mac Addr: ");
        print_mac_address(&routing_entries[i].dest_mac_addr);
        printf("\nDistance: %u\nDevice ID: %d\n", routing_entries[i].distance, routing_entries[i].device_id);
    }
    printf("\n");
    pthread_mutex_lock(&routing_reader_mutex);
    routing_reader_cnt --;
    if(routing_reader_cnt == 0){
        pthread_mutex_unlock(&routing_table_mutex);
    }
    pthread_mutex_unlock(&routing_reader_mutex);
}