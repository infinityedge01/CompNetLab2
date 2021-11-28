CFLAGS  := -g3 -Wl,--wrap,setsockopt -Wl,--wrap,socket -Wl,--wrap,bind -Wl,--wrap,listen -Wl,--wrap,connect -Wl,--wrap,accept -Wl,--wrap,read -Wl,--wrap,write -Wl,--wrap,close -Wl,--wrap,getaddrinfo
CFLAGS 	+= $(DEBUG) 
LDFLAGS := -lpcap -lpthread
INCLUDE := -I ../include
files := $(shell find ../ -name "*.o")
OBJS := echo_client echo_server perf_client perf_server
all: $(OBJS)

%: %.c
	gcc $(CFLAGS) $(files) -o $@ $^ unp.c  ${LDFLAGS} ${INCLUDE}

.PHONY: clean
clean: 
	rm -f $(OBJS)