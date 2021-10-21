CC 	    := gcc
CFLAGS  := -g3
LDFLAGS := -lpcap -lpthread
INCLUDE := -I ../include

files := $(shell find ../ -name "*.o")
OBJS = eth_sender eth_receiver
all:$(OBJS)
$(OBJS):% :%.c
	$(CC) $(CFLAGS) $< $(files) -o $@ ${LDFLAGS} ${INCLUDE}

.PHONY: clean
clean: 
	rm -f $(OBJS)
