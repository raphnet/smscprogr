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
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <stdio.h>

#define CPLD_SET_LE() PORTC |= 0x80
#define CPLD_CLR_LE() PORTC &= 0x7F
#define CPLD_PULSE_LE() do { CPLD_SET_LE(); _delay_us(0.5); CPLD_CLR_LE(); _delay_us(1); } while(0)

#define CE_LOW()	PORTC &= ~0x20
#define CE_HIGH()	PORTC |= 0x20
#define RD_LOW()	PORTC &= ~0x04
#define RD_HIGH()	PORTC |= 0x04
#define WR_LOW()	PORTC &= ~0x10
#define WR_HIGH()	PORTC |= 0x10

#define CLK_HIGH()	PORTC |= 0x40
#define CLK_LOW()	PORTC &= ~0x40

#define SET_DATA(v)	PORTB = v
#define GET_DATA()	PINB
#define FLOAT_DATA() DDRB = 0
#define DRIVE_DATA() DDRB = 0xff

#define WR_DLY()	_delay_us(0.5)
#define RD_DLY()	_delay_us(0.5)

#define CLK_DLY()	_delay_us(0.5)

static uint8_t s_first = 1;
static uint16_t s_cur_address;

static void setCartAddress(uint16_t addr)
{
	uint8_t nib;

	if (s_first) {
		s_first = 0;
	} else if (addr == s_cur_address) {
		_delay_us(5);
		return;
	}

	// Save what will be the new address before
	// addr gets modified!
	s_cur_address = addr;

	nib = addr & 0xf;
	addr >>= 4;
	PORTD &= ~0x3F;
	PORTD |= 0x00 | nib;
	CPLD_PULSE_LE();

	nib = addr & 0xf;
	addr >>= 4;
	PORTD &= ~0x3F;
	PORTD |= 0x10 | nib;
	CPLD_PULSE_LE();

	nib = addr & 0xf;
	addr >>= 4;
	PORTD &= ~0x3F;
	PORTD |= 0x20 | nib;
	CPLD_PULSE_LE();

	nib = addr & 0xf;
	PORTD &= ~0x3F;
	PORTD |= 0x30 | nib;
	CPLD_PULSE_LE();

}

void cartWrite(uint16_t addr, uint8_t b)
{
	setCartAddress(addr);
	SET_DATA(b);
	DRIVE_DATA();

	CE_LOW();
	WR_LOW();

	WR_DLY();

	WR_HIGH();
	CE_HIGH();

	// Make sure internal pull-ups are ON
	SET_DATA(0xff);
	FLOAT_DATA();

}

void cartWriteClk(uint16_t addr, uint8_t b)
{
	setCartAddress(addr);
	SET_DATA(b);
	DRIVE_DATA();

	CE_LOW();
	WR_LOW();

	CLK_HIGH();
	CLK_DLY();
	CLK_LOW();
	CLK_DLY();

	WR_HIGH();
	CE_HIGH();

	// Make sure internal pull-ups are ON
	SET_DATA(0xff);
	FLOAT_DATA();
}


uint8_t cartRead(uint16_t addr)
{
	uint8_t b;

	setCartAddress(addr);

	// Leave pins as input
	FLOAT_DATA();
	// Make sure internal pull-ups are ON
	SET_DATA(0xff);

	CE_LOW();
	RD_LOW();

	RD_DLY();
	b = GET_DATA();

	RD_HIGH();
	CE_HIGH();

	return b;
}

void cartReadBytes(uint16_t startaddr, uint16_t length, uint8_t *dst)
{
	while (length--) {
		*dst = cartRead(startaddr);
		dst++;
		startaddr++;
	}
}
