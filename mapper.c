#include <stdint.h>
#include "cartio.h"
#include "mapper.h"

static uint8_t mapper_type = MAPPER_TYPE_SEGA;

void mapper_init(uint8_t type)
{
	mapper_type = type;

	if (mapper_type == MAPPER_TYPE_SEGA) {
		// Disable RAM, Enable ROM write
		cartWriteClk(0xFFFC, 0x80);
		// Slot 0 -> Bank 0
		cartWriteClk(0xFFFD, 0);
		// Slot 1 -> Bank 1
		cartWriteClk(0xFFFE, 1);
		// Slot 2 -> Bank 2
		cartWriteClk(0xFFFF, 2);
	}
}

void mapper_setSlot(uint8_t slot, uint8_t bank)
{
	cartWriteClk(0xFFFD+slot,bank);
}


