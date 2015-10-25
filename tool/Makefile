CC=gcc
LD=$(CC)

UNAME := $(shell uname -s)
ifeq ($(UNAME), Linux)
HIDAPI_NAME=hidapi-hidraw
else
HIDAPI_NAME=hidapi
endif


ifeq ($(shell uname -o), Msys)
COMPAT_OBJS=sleep.o memmem.o
endif

CFLAGS=-Wall -g `pkg-config $(HIDAPI_NAME) --cflags`
LDFLAGS=`pkg-config $(HIDAPI_NAME) --libs` -g
GTK_CFLAGS=`pkg-config --cflags gtk+-3.0 gmodule-2.0`
GTK_LDFLAGS=`pkg-config --libs gtk+-3.0 gmodule-2.0`

PREFIX=/usr/local


.PHONY : clean install

all: gcn64ctl gcn64ctl_gui

gcn64ctl_gui: gcn64ctl_gui.o gcn64.o gcn64lib.o mempak.o hexdump.o ihex.o $(COMPAT_OBJS)
	$(LD) $^ $(LDFLAGS) $(GTK_LDFLAGS) -o $@

gcn64ctl: main.o gcn64.o mempak.o gcn64lib.o hexdump.o gc2n64_adapter.o ihex.o delay.o $(COMPAT_OBJS)
	$(LD) $^ $(LDFLAGS) -o $@

gcn64ctl_gui.o: gcn64ctl_gui.c
	$(CC) $(CFLAGS) $(GTK_CFLAGS) -c $<

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o gcn64ctl gcn64ctl_gui gcn64ctl.exe gcn64ctl_gui.exe

install:
	@echo "Install not done yet. Sorry"

