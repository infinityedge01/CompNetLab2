CC 	    = gcc
CFLAGS  = -g3
LDFLAGS = -lpcap -lpthread
INCLUDE = -I ../include

OBJS = device.o packetio.o
all:$(OBJS)
$(OBJS):%.o:%.c
	$(CC) -c $(CFLAGS) $< -o $@ ${LDFLAGS} ${INCLUDE}

.PHONY: clean
clean: 
	rm -f *.o
