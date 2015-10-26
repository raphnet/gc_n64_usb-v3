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

CFLAGS=-Wall -g `pkg-config $(HIDAPI_NAME) --cflags` --std=c99
LDFLAGS=`pkg-config $(HIDAPI_NAME) --libs` -g
GTK_CFLAGS=`pkg-config --cflags gtk+-3.0 gmodule-2.0`
GTK_LDFLAGS=`pkg-config --libs gtk+-3.0 gmodule-2.0`

PREFIX=/usr/local

MEMPAKLIB_OBJS=mempak.o mempak_fs.o

.PHONY : clean install

all: gcn64ctl mempak_ls mempak_format mempak_extract_note mempak_insert_note mempak_rm mempak_convert gcn64ctl_gui

gcn64ctl_gui: gcn64ctl_gui.o gcn64ctl_gui_mpkedit.o gcn64.o gcn64lib.o hexdump.o ihex.o $(COMPAT_OBJS) $(MEMPAKLIB_OBJS)
	$(LD) $^ $(LDFLAGS) $(GTK_LDFLAGS) -o $@

gcn64ctl: main.o gcn64.o mempak_old.o gcn64lib.o hexdump.o gc2n64_adapter.o ihex.o delay.o $(COMPAT_OBJS)
	$(LD) $^ $(LDFLAGS) -o $@

gcn64ctl_gui.o: gcn64ctl_gui.c gcn64ctl_gui.h
	$(CC) $(CFLAGS) $(GTK_CFLAGS) -c $<

gcn64ctl_gui_mpkedit.o: gcn64ctl_gui_mpkedit.c gcn64ctl_gui_mpkedit.h
	$(CC) $(CFLAGS) $(GTK_CFLAGS) -c $<

mempak_convert: mempak_convert.o $(MEMPAKLIB_OBJS)
	$(LD) $^ $(LDFLAGS) -o $@

mempak_rm: mempak_rm.o $(MEMPAKLIB_OBJS)
	$(LD) $^ $(LDFLAGS) -o $@

mempak_insert_note: mempak_insert_note.o $(MEMPAKLIB_OBJS)
	$(LD) $^ $(LDFLAGS) -o $@

mempak_extract_note: mempak_extract_note.o $(MEMPAKLIB_OBJS)
	$(LD) $^ $(LDFLAGS) -o $@

mempak_ls: mempak_ls.o $(MEMPAKLIB_OBJS)
	$(LD) $^ $(LDFLAGS) -o $@

mempak_format: mempak_format.o $(MEMPAKLIB_OBJS)
	$(LD) $^ $(LDFLAGS) -o $@


%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o gcn64ctl gcn64ctl_gui gcn64ctl.exe gcn64ctl_gui.exe

install:
	@echo "Install not done yet. Sorry"

