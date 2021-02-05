/*	smsprogr : Programmer for SMS and GG cartridges.
 *	Copyright (C) 2020-2021  Raphael Assenat <raph@raphnet.net>
 *
 *	This program is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 3 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <avr/pgmspace.h>
#include <util/crc16.h>
#include <util/delay.h>

#include "menu.h"
#include "bootloader.h"
#include "cartio.h"
#include "usbcomm.h"
#include "flash.h"

static uint32_t s_rom_size = 0x8000;

static void setROMsize(uint32_t rom_size)
{
	printf_P(PSTR("ROM size set to %" PRIu32 "\n"), rom_size);
	s_rom_size = rom_size;
}

static void printHex(uint8_t *buf, int len)
{
	while (len--) {
		printf_P(PSTR("%02x "), *buf);
		buf++;
	}
}

static void error() {
	puts_P(PSTR("ERROR"));
}

static void newline()
{
	puts("");
}

static void printPrompt()
{
	printf_P(PSTR("> "));
}

static void boot(const char *line, int length)
{
	enterBootLoader();
}

static void reset(const char *line, int length)
{
	resetFirmware();
}

static uint8_t cartrange_is_all_ff(uint16_t addr_start, uint16_t len)
{
	uint8_t b;

	while (len--) {
		b = cartRead(addr_start);
		if (b != 0xff)
			return 0;
		addr_start++;
	}

	return 1;
}


static uint16_t crc16_cartrange(uint16_t addr_start, uint16_t len)
{
	uint16_t crc = 0;
	uint8_t b;

	while (len--) {
		b = cartRead(addr_start);
		crc = _crc_xmodem_update(crc, b);
		addr_start++;
	}

	return crc;
}

static void initMapper(void)
{
	// Disable RAM
	cartWrite(0xFFFC, 0);
	// Slot 0 -> Bank 0
	cartWrite(0xFFFD, 0);
	// Slot 1 -> Bank 1
	cartWrite(0xFFFE, 1);
	// Slot 2 -> Bank 2
	cartWrite(0xFFFF, 2);
}

static void debug2()
{
	uint8_t i;
	uint8_t tmpbuf[16] = { };

	initMapper();
	for (i=0; i<6; i++) {
		cartWrite(0xFFFF, i);
		tmpbuf[0] = i;
		tmpbuf[1] = i+2;
		flash_programBytes(0x8000, tmpbuf, 16);
	}
	newline();
}

static void debug1()
{
	uint8_t tmpbuf[16];
	uint8_t i;

	initMapper();
	printf_P(PSTR("0000[ ]"));
	cartReadBytes(0x0000, 8, tmpbuf);
	printHex(tmpbuf, 8);
	newline();

	printf_P(PSTR("4000[ ]"));
	cartReadBytes(0x4000, 8, tmpbuf);
	printHex(tmpbuf, 8);
	newline();

	for (i=0; i<6; i++) {
		cartWrite(0xFFFF, i);
		cartReadBytes(0x8000, 8, tmpbuf);
		printf_P(PSTR("8000[%d]"), i);
		printHex(tmpbuf, 8);
		newline();
	}

}

static void initCart(const char *line, int length)
{
	uint8_t rom_header[16];
	uint16_t bank0_crc, crc;
	uint8_t bank0_first, first_byte;
	uint16_t id, read_addr;
	int i;

	initMapper();

	putchar(' ');
	cartReadBytes(0x7FF0, 16, rom_header);
	printHex(rom_header, 16);
	newline();

	cartWrite(0xFFFF, 0);
	bank0_crc = crc16_cartrange(0x0000, 16384);
	bank0_first = cartRead(0x00);
	printf_P(PSTR("Bank %d CRC: %04x, first byte %02x\n"), 0, bank0_crc, bank0_first);

	for (i=1; i<64; i<<=1) {

		// For bank 1 (as for bank 0 above), read directly instead
		// of using the mapper and the page 2 window.
		if (i==1) {
			read_addr = 0x4000;
		} else {
			read_addr = 0x8000;
			cartWrite(0xFFFF, i);
		}
		first_byte = cartRead(read_addr);

		// Only bother computing and comparing the CRC if the first
		// bytes are equal. This speeds things a lot.
		if (first_byte == bank0_first) {
			crc = crc16_cartrange(read_addr, 16384);

			printf_P(PSTR("Bank %d CRC: %04x\n"), i, crc);
			if (crc == bank0_crc) {
				break;
			}
		} else {
			printf_P(PSTR("Bank %d first byte: %02x\n"), i, first_byte);
		}

		// If bank 2 (and beyond) is all FF, this may be a mapper-less cartridge
		// or card.
		if ((i == 2) && cartrange_is_all_ff(read_addr, 16384)) {
			printf_P(PSTR("Bank 2 is all FF, assuming 32K mapperless cartridge\n"));
			break;
		}
	}
	setROMsize(i * 16384UL);

	if (flash_detect()) {
		uint8_t manufacturer, device;

		id = flash_readSiliconID();

		manufacturer = id;
		device = id >> 8;

		printf_P(PSTR("Cartridge type: FLASH. Manufacturer ID=0x%02x, Device=0x%02x => "), manufacturer, device);

		if (id == 0xa4c2) {
			puts_P(PSTR("MX29F040 (supported)\n"));
		} else {
			puts_P(PSTR(" (unknown/unsupported)\n"));
		}
	} else {
		puts_P(PSTR("Cartridge type: ROM"));
	}

	initMapper();
}

static void readaddress(const char *line, int length)
{
	uint16_t addr;
	int i;
	int len;
	uint8_t b;

	i = sscanf_P(line, PSTR("r %04x %d"), &addr, &len);
	if (i < 1) {
		error();
		return;
	}
	if (i < 2)
		len = 1;

	newline();

	printf_P(PSTR("Read %d bytes from 0x%04x : "), len, addr);

	for (i=0; i<len; i++) {
		b = cartRead(addr);
		printHex(&b, 1);
		addr++;
	}
	newline();
}

void chiperase(const char *line, int length)
{
	newline();
	puts_P(PSTR("Erasing chip..."));
	usbcomm_drain();
	flash_chipErase();
	puts_P(PSTR("Done."));
}

void flashWrite(const char *line, int length)
{
	uint16_t addr;
	int i;
	uint16_t b;

	i = sscanf_P(line, PSTR("flashwrite %04x %02x"), &addr, &b);
	if (i < 1) {
		error();
		return;
	}

	newline();
	printf_P(PSTR("Program 0x%02x at address 0x%04x\n"), b, addr);
	usbcomm_drain();

	// Access the flash through slot 2
	cartWrite(0xFFFF, addr >> 14);
	flash_programByte(0x8000 | (addr & 0x3FFF), b);

	// Slot 2 -> Bank 2
	cartWrite(0xFFFF, 2);

}

static int waitChar(int timeout_cs)
{
	int i;

	for (i=0; i<timeout_cs; i++) {
		if (usbcomm_hasData()) {
			return usbcomm_rxbyte();
		}
		_delay_ms(1);
	}
	return -1;
}

static uint8_t s_packetbuf[133];

#define STATE_WAIT_SOH			0
#define STATE_RX_DATA			1
#define STATE_PROCESS_PACKET	2

void uploadXmodem(const char *line, int length)
{
	uint8_t state = STATE_WAIT_SOH;
	uint8_t send_nack;
	uint8_t packet_size = 132;
	uint8_t datpos = 0;
	uint32_t rom_addr = 0;
	uint8_t last_packet_id = 0;
	int c, b;

	newline();
	puts_P(PSTR("READY. Please start uploading."));

	send_nack = 1;
	while (1)
	{
		b = -1;
		for (c=0; c<15; c++) {
			if (state == STATE_WAIT_SOH) {
				putchar(send_nack ? 0x15 : 0x06);
				usbcomm_drain();
			}

			b = waitChar(5000); // 5 seconds
			if (b >= 0)
				break;
		}
		if (b < 0) {
			puts_P(PSTR("Timeout"));
			return;
		}

		switch (state)
		{
			case STATE_WAIT_SOH:
				if (b == 0x01) {
					state = STATE_RX_DATA;
					datpos = 1;
					s_packetbuf[0] = b;
				} else if ((b == 0x03)||(b == 0x18)) {
					newline();
					puts_P(PSTR("Upload interrupted"));
					return;
				} else if ((b == 0x04)) { // End of transmission
					putchar(0x06); // ACK
					newline();
					puts_P(PSTR("End of transmission - done"));
					return;
				}
				break;

			case STATE_RX_DATA:
				s_packetbuf[datpos] = b;
				datpos++;
				if (datpos < packet_size) {
					// keep receiving...
					break;
				}
				state = STATE_PROCESS_PACKET;
				// fallthrough...

			case STATE_PROCESS_PACKET:
				// Todo : Check sequence numbers, CRC...
				if (s_packetbuf[1] != last_packet_id) {
					// New packet

					// Access the flash through slot 2
					cartWrite(0xFFFF, rom_addr >> 14);
					flash_programBytes(0x8000 | (rom_addr & 0x3FFF), &s_packetbuf[3], 128);
					rom_addr += 128;
					send_nack = 0;

					last_packet_id = s_packetbuf[1];
				} else {
					// Just ignore a duplicate packet
					send_nack = 0;
				}

				state = STATE_WAIT_SOH;
				break;
		}
	}
}

void downloadXmodem(const char *line, int length)
{
	uint8_t packetno = 1, b;
	uint16_t crc;
	uint32_t rom_addr = 0;
	int i, j, n_blocks;
	char crc_mode;
	uint8_t packet_size;

	n_blocks = s_rom_size / 128;

	printf_P(PSTR("Dumping the rom using XMmodem. %d blocks.\n"), n_blocks);
	puts_P(PSTR("Please start the download... CTRL+C to cancel."));

	while (1) {
		usbcomm_doTasks();

		if (usbcomm_hasData()) {
			b = usbcomm_rxbyte();
			if (b == 0x03) {
				newline();
				puts_P(PSTR("Transfer cancelled."));
				newline();
				return;
			}
			if (b == 'C') {
				crc_mode = 1;
				packet_size = 133;
				break;
			}
			if (b == 0x15) { // NACK
				crc_mode = 0;
				packet_size = 132;
				break;
			}
		}

	}

	s_packetbuf[0] = 0x01; // SOH

	// Slot 0 -> Bank 0
	cartWrite(0xFFFD, 0);
	// Slot 1 -> Bank 1
	cartWrite(0xFFFE, 1);

	for (i=0; i<n_blocks; i++)
	{

		if (rom_addr < 0x8000) {
			// Read bank0/1 using slot0/1 to support mapperless cartridges.
			cartReadBytes(rom_addr, 128, s_packetbuf + 3);
		} else {
			// Use slot2 as a window in the ROM
			cartWrite(0xFFFF, rom_addr >> 14);
			cartReadBytes(0x8000 | (rom_addr & 0x3FFF), 128, s_packetbuf + 3);
		}

		// Prepare the X-modem packet
		s_packetbuf[1] = packetno;	// Packet number
		s_packetbuf[2] = ~packetno;	// packet number complement

		if (crc_mode) {
			crc = 0;
			for (j=3; j<131; j++) {
				crc = _crc_xmodem_update(crc, s_packetbuf[j]);
			}
			s_packetbuf[131] = crc >> 8;
			s_packetbuf[132] = crc;
		} else {
			uint8_t chk = 0;
			// Sum of data bytes only
			for (j=3; j<131; j++) {
				chk += s_packetbuf[j];
			}
			s_packetbuf[131] = chk;
		}

		// Send it
		usbcomm_txbytes(s_packetbuf, packet_size);
		usbcomm_drain();

		// Wait ack
		//
		while (1) {
			usbcomm_doTasks();
			if (usbcomm_hasData()) {
				b = usbcomm_rxbyte();
				if (b == 0x06) // ACK
					break;
				if (b == 0x15) { // NACK
					usbcomm_txbytes(s_packetbuf, packet_size);
					usbcomm_drain();
				}
				if (b == 0x18) { // CAN
					newline();
					puts(PSTR("Transfer cancelled"));
					newline();
					goto done;
				}
			}
		}


		rom_addr += 128;
		packetno++;
	}

	// EOT
	putchar(0x04);

	while (1) {
		usbcomm_doTasks();

		if (usbcomm_hasData()) {
			b = usbcomm_rxbyte();
			if (b == 0x06) // ACK
				break;
		}
	}


done:
	// Slot 2 -> Bank 2
	cartWrite(0xFFFF, 2);

}

void menu_handleLine(const uint8_t *line, int length)
{
	uint8_t i;

	struct commandHandler {
		const char *cmd;
		void (*handler)(const char *line, int length);
		const char *help;
	} handlers[] = {
		{ PSTR("boot"), boot, PSTR("Enter DFU bootloader") },
		{ PSTR("reset"), reset, PSTR("Reset the firmware") },
		{ PSTR("init"), initCart, PSTR("Init. mapper hw, detect cart size, detect flash...") },
		{ PSTR("r "), readaddress, PSTR("addresshex [length]") },
		{ PSTR("dx"), downloadXmodem, PSTR("Download the ROM with XModem") },
		{ PSTR("ux"), uploadXmodem, PSTR("Upload and program FLASH with XModem") },
		{ PSTR("ce"), chiperase, PSTR("Perform a chip erase operation") },
		{ PSTR("fw"), flashWrite, PSTR("addresshex hexbyte") },
		{ PSTR("d1"), debug1, PSTR("Debug 1") },
		{ PSTR("d2"), debug2, PSTR("Debug 2") },
		{ }
	};


	for (i=0; handlers[i].handler; i++) {
		if (strncmp_P((const char *)line, handlers[i].cmd, strlen_P(handlers[i].cmd)) == 0) {
			handlers[i].handler((const char *)line, length);
			goto done;
		}
	}

	newline();
	if (length == 0) {
		goto done;
	}
	else if (line[0] == '?') {
		puts_P(PSTR("Supported commands:"));
		for (i=0; handlers[i].handler; i++) {
			printf_P(PSTR("  "));
			printf_P(handlers[i].cmd);
			if (handlers[i].help) {
				printf_P(PSTR("    "));
				printf_P(handlers[i].help);
			}
			newline();
		}
		newline();
	} else {
		error();
		newline();
	}

done:
	printPrompt();
}

