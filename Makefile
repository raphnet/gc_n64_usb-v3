CC=avr-gcc
AS=$(CC)
LD=$(CC)

include Makefile.inc

PROGNAME=gcn64usb
OBJDIR=objs-$(PROGNAME)
CPU=atmega32u2
CFLAGS=-Wall -mmcu=$(CPU) -DF_CPU=16000000L -Os -DUART1_STDOUT -DVERSIONSTR=$(VERSIONSTR) -DVERSIONSTR_SHORT=$(VERSIONSTR_SHORT) -DVERSIONBCD=$(VERSIONBCD) -std=gnu99
LDFLAGS=-mmcu=$(CPU) -Wl,-Map=$(PROGNAME).map
HEXFILE=$(PROGNAME).hex

all: $(HEXFILE)

clean:
	rm -f $(PROGNAME).elf $(PROGNAME).hex $(PROGNAME).map $(OBJS)

gcn64txrx0.o: gcn64txrx.S
	$(CC) $(CFLAGS) -c $< -o $@ -DSUFFIX=0 -DGCN64_DATA_BIT=0

gcn64txrx1.o: gcn64txrx.S
	$(CC) $(CFLAGS) -c $< -o $@ -DSUFFIX=1 -DGCN64_DATA_BIT=2

gcn64txrx2.o: gcn64txrx.S
	$(CC) $(CFLAGS) -c $< -o $@ -DSUFFIX=2 -DGCN64_DATA_BIT=1

gcn64txrx3.o: gcn64txrx.S
	$(CC) $(CFLAGS) -c $< -o $@ -DSUFFIX=3 -DGCN64_DATA_BIT=3

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $< -o $@

$(PROGNAME).elf: $(OBJS)
	$(LD) $(OBJS) $(LDFLAGS) -o $(PROGNAME).elf

$(PROGNAME).hex: $(PROGNAME).elf
	avr-objcopy -j .data -j .text -O ihex $(PROGNAME).elf $(PROGNAME).hex
	avr-size $(PROGNAME).elf -C --mcu=$(CPU)

fuse:

flash: $(HEXFILE)
	- ./scripts/enter_bootloader.sh
	./scripts/wait_then_flash.sh $(CPU) $(HEXFILE)

justflash: $(HEXFILE)
	./scripts/wait_then_flash.sh $(CPU) $(HEXFILE)

chip_erase:
	dfu-programmer atmega32u2 erase

reset:
	dfu-programmer atmega32u2 reset

restart:
	- ./scripts/enter_bootloader.sh
	./scripts/start.sh $(CPU)
