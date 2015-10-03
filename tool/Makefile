CC=gcc
LD=$(CC)

CFLAGS=-Wall -g `pkg-config hidapi-hidraw --cflags`
LDFLAGS=`pkg-config hidapi-hidraw --libs` -g

PREFIX=/usr/local

PROG=gcn64ctl

OBJS=main.o gcn64.o mempak.o gcn64lib.o hexdump.o gc2n64_adapter.o ihex.o

.PHONY : clean install

all: $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $(PROG)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o $(PROG)

install:
	@echo "Install not done yet. Sorry"

