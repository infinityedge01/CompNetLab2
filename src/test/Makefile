CC 	    := gcc
CFLAGS  := -g3
LDFLAGS := -lpcap -lpthread
INCLUDE := -I ../include

files := $(shell find ../ -name "*.o")
OBJS = arp_sender arp_receiver router ip_sender router_2
all:$(OBJS)
$(OBJS):% :%.c
	$(CC) $(CFLAGS) $< $(files) -o $@ ${LDFLAGS} ${INCLUDE}

.PHONY: clean
clean: 
	rm -f $(OBJS)
