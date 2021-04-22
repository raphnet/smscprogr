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
#include <stdint.h>
#include "cartio.h"
#include "flash.h"

static uint16_t readSiliconID(void)
{
	uint16_t id;

	// Step 1: Write AA to address 555
	cartWrite(0x0555, 0xAA);
	// Step 2: Write 55 to address 2AA
	cartWrite(0x02AA, 0x55);
	// Step 3: Write 90 to address 555
	cartWrite(0x0555, 0x90);

	id = cartRead(0x0000);
	id |= cartRead(0x0001) << 8;

	cartWrite(0x0000, 0xF0);

	return id;
}

static char detect(void)
{
	uint16_t mem, id;

	mem = cartRead(0x0000);
	mem |= cartRead(0x0001) << 8;

	id = readSiliconID();

	return id != mem;
}

static void chipErase(void)
{
	// Step 1: Write AA to address 555
	cartWrite(0x0555, 0xAA);
	// Step 2: Write 55 to address 2AA
	cartWrite(0x02AA, 0x55);
	// Step 3: Write 80 to address 555
	cartWrite(0x0555, 0x80);
	// Step 4: Write AA to address 555
	cartWrite(0x0555, 0xAA);
	// Step 5: Write 55 to address 2AA
	cartWrite(0x02AA, 0x55);
	// Step 3: Write 10 to address 555
	cartWrite(0x0555, 0x10);

	while (!(cartRead(0x0000) & 0x80)) {
	}

	cartWrite(0x0000, 0xF0);
}

static void programBytes(uint16_t cartAddr, uint8_t *data, int len)
{
	while (len--) {
		// Step 1: Write AA to address 555
		cartWrite(0x0555, 0xAA);
		// Step 2: Write 55 to address 2AA
		cartWrite(0x02AA, 0x55);
		// Step 3: Write A0 to address 555
		cartWrite(0x0555, 0xA0);

		cartWrite(cartAddr, *data);

		// Now poll Q7 for completion. Q7 is the complement
		// of what was written until completion.
		while ((cartRead(cartAddr)&0x80) != ((*data)&0x80));

		cartAddr++;
		data++;
	}
}

static void programByte(uint16_t cartAddr, uint8_t b)
{
	programBytes(cartAddr, &b, 1);
}

struct flashops flash_29f040_ops = {
	.readSiliconID = readSiliconID,
	.detect = detect,
	.chipErase = chipErase,
	.programBytes = programBytes,
	.programByte = programByte,
};

