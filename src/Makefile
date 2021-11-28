CC 		= gcc
CFLAGS 	= -g3
LDFLAGS = -lpcap -lpthread
INCLUDE = -I src/include

files := $(shell find . -name "makefile" -o -name "Makefile")
files := $(filter-out ./Makefile,$(files))
files := $(filter-out ./test/Makefile,$(files))
files := $(filter-out ./checkpoint/Makefile,$(files))
.PHONY: all
all:
	for i in $(files); do make -C `dirname $$i`; done
.PHONY: debug
debug:
	for i in $(files); do make -C `dirname $$i` DEBUG=-DDEBUG; done
.PHONY: clean
clean: 
	for i in $(files); do make -C `dirname $$i` clean; done

