#ifndef _flash_h__
#define _flash_h__

struct flashops {
	uint16_t (*readSiliconID)(void);
	char (*detect)(void);
	void (*chipErase)(void);
	void (*programBytes)(uint16_t cartAddr, uint8_t *data, int len);
	void (*programByte)(uint16_t cartAddr, uint8_t b);
};

uint16_t flash_readSiliconID(void);
void flash_init(void);
char flash_detect(void);
void flash_chipErase(void);
void flash_programBytes(uint16_t cartAddr, uint8_t *data, int len);
void flash_programByte(uint16_t cartAddr, uint8_t b);

// based on a known flash ID, return the size of the chip
uint32_t flash_getMaxSize(uint16_t flash_id);

extern struct flashops flash_29f040_ops;
extern struct flashops flash_29lv320_ops;

#endif // _flash_h__

