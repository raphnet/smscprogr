#ifndef _cartio_h__
#define _cartio_h__

void setCartAddress(uint16_t addr);
uint8_t cartRead(uint16_t addr);
void cartWrite(uint16_t addr, uint8_t b);
void cartWriteClk(uint16_t addr, uint8_t b);

void cartReadBytes(uint16_t startaddr, uint16_t length, uint8_t *dst);

#endif // _cartio_h__

