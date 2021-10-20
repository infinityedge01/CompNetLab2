CC 		= gcc
CFLAGS 	= -g3
LDFLAGS = -lpcap -lpthread
INCLUDE = -I./src/include

all: build/main build/a build/b build/router

build/main: main.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

build/a: a.c $(LIB_FILE)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

build/b: b.c $(LIB_FILE)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

build/router: router.c $(LIB_FILE)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

.PHONY: clean
clean: 
	rm -f ./build/*

$(shell mkdir -p build)