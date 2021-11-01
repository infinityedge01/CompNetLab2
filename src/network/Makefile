CC 	    := gcc
CFLAGS  := -g3
CFLAGS 	+= $(DEBUG)
LDFLAGS := -lpcap -lpthread
INCLUDE := -I ../include

OBJS = arp.o device.o forwarding.o ip.o routing.o util.o
all:$(OBJS)
$(OBJS):%.o:%.c
	$(CC) -c $(CFLAGS) $< -o $@ ${LDFLAGS} ${INCLUDE}


.PHONY: clean
clean: 
	rm -f *.o
