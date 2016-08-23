CC=avr-gcc
AS=$(CC)
LD=$(CC)

include Makefile.inc

PROGNAME=gcn64usb
OBJDIR=objs-$(PROGNAME)
CPU=atmega32u2
CFLAGS=-Wall -mmcu=$(CPU) -DF_CPU=16000000L -Os -DUART1_STDOUT -DVERSIONSTR=$(VERSIONSTR) -DVERSIONSTR_SHORT=$(VERSIONSTR_SHORT)
LDFLAGS=-mmcu=$(CPU) -Wl,-Map=$(PROGNAME).map
HEXFILE=$(PROGNAME).hex

all: $(HEXFILE)

clean:
	rm -f $(PROGNAME).elf $(PROGNAME).hex $(PROGNAME).map $(OBJS)

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
	avr-size $(PROGNAME).elf

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
