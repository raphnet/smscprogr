CC=avr-gcc
AS=$(CC)
LD=$(CC)
PROGNAME=smscprg1
CPU=atmega32u2
VERSIONSTR=\"1.3\"
VERSIONBCD=0x0103
CFLAGS=-Wall -mmcu=$(CPU) -DF_CPU=16000000L -DF_EXTERNAL=F_CPU -Os -DVERSIONSTR=$(VERSIONSTR) -DVERSIONBCD=$(VERSIONBCD)
LDFLAGS=-mmcu=$(CPU) -Wl,-Map=$(PROGNAME).map

HEXFILE=smscprogr.hex
OBJS=main.o usb.o usbcomm.o usbstrings.o menu.o cartio.o mapper.o bootloader.o flash.o flash_29f040.o flash_29lv320.o

all: $(HEXFILE)

clean:
	rm -f *.o *.elf *.hex

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $< -o $@

smscprogr.elf: $(OBJS)
	$(LD) $^ $(LDFLAGS) -o $@

%.hex: %.elf
	avr-objcopy -j .data -j .text -O ihex $< $@
	avr-size $< -C --mcu=$(CPU)

flash: $(HEXFILE)
#	- ./scripts/enter_bootloader.sh
	./scripts/wait_then_flash.sh $(CPU) $(HEXFILE)

chip_erase:
	dfu-programmer atmega32u2 erase

reset:
	dfu-programmer atmega32u2 reset
