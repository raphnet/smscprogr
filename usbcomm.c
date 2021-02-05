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
#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include "usbcomm.h"

/* When defined, has putchar inject a \r character before
 * every \n character */
#define PUTCHAR_SENDS_CRLF

#define RXBUF_SIZE	140
static uint8_t rxbuf[RXBUF_SIZE];
static volatile uint8_t rxbuf_head = 0;
static volatile uint8_t rxbuf_tail = 0;

#define TXBUF_SIZE	63
static uint8_t txbuf[TXBUF_SIZE];
static uint8_t txbuf_pos = 0;

static uint16_t (*fn_sendBytes)(const uint8_t *data, uint16_t l) = 0;
static uint8_t s_flags;

void usbcomm_addbyte(uint8_t b)
{
	rxbuf[rxbuf_head] = b;
	rxbuf_head++;

	if (rxbuf_head >= RXBUF_SIZE) {
		rxbuf_head = 0;
	}
}

static int usbcomm_putchar(char c, FILE *stream)
{
#ifdef PUTCHAR_SENDS_CRLF
	if (c == '\n') {
		usbcomm_txbyte('\r');
	}
#endif
	usbcomm_txbyte(c);
	return 1;
}

static int usbcomm_nullputchar(char c, FILE *stream)
{
	return 1;
}

static FILE mystdout = FDEV_SETUP_STREAM(usbcomm_putchar, NULL, _FDEV_SETUP_WRITE);
static FILE nullstdout = FDEV_SETUP_STREAM(usbcomm_nullputchar, NULL, _FDEV_SETUP_WRITE);

void usbcomm_init(uint16_t (*ll_tx)(const uint8_t *data, uint16_t len), uint8_t flags)
{
	fn_sendBytes = ll_tx;
	s_flags = flags;

	if (s_flags & USBCOMM_EN_STDOUT) {
		stdout = &mystdout;
	} else {
		stdout = &nullstdout;
	}
}

uint8_t usbcomm_hasData(void)
{
	uint8_t b, sreg;

	sreg = SREG;
	cli();
	b = rxbuf_head != rxbuf_tail;
	SREG = sreg;

	return b;
}

uint8_t usbcomm_rxbyte(void)
{
	uint8_t v = rxbuf[rxbuf_tail];

	rxbuf_tail++;
	if (rxbuf_tail >= RXBUF_SIZE) {
		rxbuf_tail = 0;
	}

	return v;
}

void usbcomm_drain()
{
	do {
		usbcomm_doTasks();
	} while (txbuf_pos);
}

void usbcomm_txbyte(uint8_t b)
{
	if (txbuf_pos >= TXBUF_SIZE) {
		usbcomm_drain();
	}

	txbuf[txbuf_pos] = b;
	txbuf_pos++;
}

void usbcomm_txbytes(uint8_t *data, int len)
{
	while (len--) {
		usbcomm_txbyte(*data);
		data++;
	}
}

void usbcomm_doTasks(void)
{
	uint16_t sent;

	if (txbuf_pos > 0) {
		sent = fn_sendBytes(txbuf, txbuf_pos);
		if (sent) {
			// everything always gets sent for now.
			txbuf_pos = 0;
		}
	}
}

