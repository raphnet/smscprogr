#ifndef _mapper_h__
#define _mapper_h__

enum {
	MAPPER_TYPE_NONE = 0,
	MAPPER_TYPE_SEGA = 1,
};

#define SLOT0	0
#define SLOT1	1
#define SLOT2	2

void mapper_init(uint8_t type);
void mapper_setSlot(uint8_t slot, uint8_t bank);

#endif // _mapper_h__

