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

static struct flashops *ops = &flash_29f040_ops;

void flash_init(void)
{
	uint16_t id;

	ops = &flash_29f040_ops;

	id = flash_29lv320_ops.readSiliconID();
	if (id == FLASH_ID_MX29LV320) {
		ops = &flash_29lv320_ops;
    return;
	}
	if (id == FLASH_ID_MX29LV640) {
		ops = &flash_29lv320_ops;
    return;
	}

	if (id == FLASH_ID_S29JL032) {
		ops = &flash_29lv320_ops;
    return;
	}

  id = flash_39sf040_ops.readSiliconID();
  if ((id == FLASH_ID_SST39SF040) || (id == FLASH_ID_SST39SF020A) || (id == FLASH_ID_SST39F010A)) {
    ops = &flash_39sf040_ops;
    return;
  }
}

uint16_t flash_readSiliconID(void)
{
	return ops->readSiliconID();
}

char flash_detect(void)
{
	return ops->detect();
}

void flash_chipErase(void)
{
	ops->chipErase();
}

void flash_programBytes(uint16_t cartAddr, uint8_t *data, int len)
{
	ops->programBytes(cartAddr, data, len);
}

void flash_programByte(uint16_t cartAddr, uint8_t b)
{
	ops->programByte(cartAddr, b);
}

uint32_t flash_getMaxSize(uint16_t flash_id)
{
	switch(flash_id)
	{
		case FLASH_ID_MX29F040:    return 524288;  // 512K
		case FLASH_ID_MX29LV320:   return 4194304; // 4M
		case FLASH_ID_MX29LV640:   return 8388608; // 8M
		case FLASH_ID_S29JL032:    return 4194304; // 4M
    case FLASH_ID_SST39SF040:  return 524288;  // 512K
    case FLASH_ID_SST39SF020A: return 262144;  // 256K
    case FLASH_ID_SST39SF040A: return 131072;  // 128K
	}

	return 4194304; // assume the max possible with Sega mapper
}
