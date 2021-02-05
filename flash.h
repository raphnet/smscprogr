#ifndef _flash_h__
#define _flash_h__

uint16_t flash_readSiliconID(void);
char flash_detect(void);
void flash_chipErase(void);
void flash_programBytes(uint16_t cartAddr, uint8_t *data, int len);
void flash_programByte(uint16_t cartAddr, uint8_t b);

#endif // _flash_h__

