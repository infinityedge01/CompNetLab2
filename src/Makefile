CC 		= gcc
CFLAGS 	= -g3
LDFLAGS = -lpcap -lpthread
INCLUDE = -I src/include

files := $(shell find . -name "makefile" -o -name "Makefile")
files := $(filter-out ./Makefile,$(files))

.PHONY: all
all:
	for i in $(files); do make -C `dirname $$i`; done
.PHONY: clean
clean: 
	for i in $(files); do make -C `dirname $$i` clean; done

