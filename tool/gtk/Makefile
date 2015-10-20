CC=gcc
LD=$(CC)

CFLAGS=-Wall -g `pkg-config --cflags hidapi-hidraw gtk+-3.0 gmodule-2.0`
LDFLAGS=`pkg-config --libs hidapi-hidraw gtk+-3.0 gmodule-2.0`

PREFIX=/usr/local

PROG=gcn64ctl_gui

OBJS=main.o ../gcn64.o ../mempak.o ../gcn64lib.o ../hexdump.o ../ihex.o

.PHONY : clean install

all: $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $(PROG)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o $(PROG)

install:
	@echo "Install not done yet. Sorry"

