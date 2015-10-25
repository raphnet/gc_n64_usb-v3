CC=gcc
LD=$(CC)

CFLAGS=-Wall -g --std=c99
LDFLAGS=-g

PREFIX=/usr/local

PROGRAMS=mempak_ls mempak_format mempak_extract_note mempak_insert_note
MEMPAKLIB_OBJS=mempak.o mempak_fs.o

.PHONY : clean install

all: $(PROGRAMS)

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
	rm -f *.o $(PROGRAMS)

install:
	@echo "Install not done yet. Sorry"

