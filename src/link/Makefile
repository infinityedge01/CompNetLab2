CC 	    := gcc
CFLAGS  := -g3
CFLAGS 	+= $(DEBUG)
LDFLAGS := -lpcap -lpthread
INCLUDE := -I ../include

OBJS = device.o packetio.o util.o
all:$(OBJS)
$(OBJS):%.o:%.c
	$(CC) -c $(CFLAGS) $< -o $@ ${LDFLAGS} ${INCLUDE}


.PHONY: clean
clean: 
	rm -f *.o
