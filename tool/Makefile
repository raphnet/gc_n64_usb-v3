CC=gcc
LD=$(CC)

UNAME := $(shell uname -s)
ifeq ($(UNAME), Linux)
HIDAPI_NAME=hidapi-hidraw
else
HIDAPI_NAME=hidapi
endif


ifeq ($(shell uname -o), Msys)
COMPAT_OBJS=sleep.o memmem.o strcasestr.o
PLATFORM_CFLAGS=-DWINDOWS
endif

CFLAGS=-Wall -g `pkg-config $(HIDAPI_NAME) --cflags` --std=c99 $(PLATFORM_CFLAGS)
LDFLAGS=`pkg-config $(HIDAPI_NAME) --libs` -g
GTK_CFLAGS=`pkg-config --cflags gtk+-3.0 gmodule-2.0`
GTK_LDFLAGS=`pkg-config --libs gtk+-3.0 gmodule-2.0`

PREFIX=/usr/local

PROGS=gcn64ctl mempak_ls mempak_format mempak_extract_note mempak_insert_note mempak_rm mempak_convert gcn64ctl_gui
MEMPAKLIB_OBJS=mempak.o mempak_fs.o $(COMPAT_OBJS)

.PHONY : clean install

all: $(PROGS)

gcn64ctl_gui: gcn64ctl_gui.o gcn64ctl_gui_mpkedit.o gcn64.o gcn64lib.o hexdump.o ihex.o mempak_gcn64usb.o $(MEMPAKLIB_OBJS)
	$(LD) $^ $(LDFLAGS) $(GTK_LDFLAGS) -o $@

gcn64ctl: main.o gcn64.o gcn64lib.o hexdump.o gc2n64_adapter.o ihex.o delay.o mempak_gcn64usb.o $(MEMPAKLIB_OBJS)
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
	rm -f *.o *.exe $(PROGS)

install:
	@echo "Install not done yet. Sorry"

