CC 	    := gcc
CFLAGS  := -g3 -Wl,--wrap,socket -Wl,--wrap,bind -Wl,--wrap,listen -Wl,--wrap,connect -Wl,--wrap,accept -Wl,--wrap,read -Wl,--wrap,write -Wl,--wrap,close -Wl,--wrap,getaddrinfo
CFLAGS 	+= $(DEBUG) 
LDFLAGS := -lpcap -lpthread
INCLUDE := -I ../include

files := $(shell find ../ -name "*.o")
OBJS = arp_sender arp_receiver router ip_sender router_2 tcp_server tcp_protocol_stack tcp_client
#OBJS = tcp_server tcp_protocol_stack tcp_client
all:$(OBJS)
$(OBJS):% :%.c
	$(CC) $(CFLAGS) $< $(files) -o $@ ${LDFLAGS} ${INCLUDE}

.PHONY: clean
clean: 
	rm -f $(OBJS)
