CC=avr-gcc
AS=$(CC)
LD=$(CC)
PROGNAME=smscprg1
CPU=atmega32u2
VERSIONSTR=1.0
VERSIONBCD=0x0100
CFLAGS=-Wall -mmcu=$(CPU) -DF_CPU=16000000L -DF_EXTERNAL=F_CPU -Os -DVERSIONSTR=$(VERSIONSTR) -DVERSIONBCD=$(VERSIONBCD)
LDFLAGS=-mmcu=$(CPU) -Wl,-Map=$(PROGNAME).map

HEXFILE=smscprg1.hex
OBJS=main.o usb.o usbcomm.o usbstrings.o menu.o cartio.o bootloader.o flash.o

all: $(HEXFILE)

clean:
	rm -f *.o *.elf *.hex

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $< -o $@

smscprg1.elf: $(OBJS)
	$(LD) $^ $(LDFLAGS) -o $@

%.hex: %.elf
	avr-objcopy -j .data -j .text -O ihex $< $@
	avr-size $< -C --mcu=$(CPU)

flash: $(all)
#	- ./scripts/enter_bootloader.sh
	./scripts/wait_then_flash.sh $(CPU) $(HEXFILE)

chip_erase:
	dfu-programmer atmega32u2 erase

reset:
	dfu-programmer atmega32u2 reset
