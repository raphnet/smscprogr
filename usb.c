/*	smsprogr : Programmer for SMS and GG cartridges.
 *	Copyright (C) 2020-2021  Raphael Assenat <raph@raphnet.net>
 *
 *  Based on:
 *
 *	gc_n64_usb : Gamecube or N64 controller to USB firmware
 *	Copyright (C) 2007-2013  Raphael Assenat <raph@raphnet.net>
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
#include <stdlib.h>
#include <string.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>

#include <util/delay.h>

#include "usb.h"

#define STATE_POWERED		0
#define STATE_DEFAULT		1
#define STATE_ADDRESS		2
#define STATE_CONFIGURED	3

static volatile uint8_t g_usb_suspend;
static uint8_t g_device_state = STATE_DEFAULT;
static uint8_t g_current_config;

// pointer to data for outgoing (IN endpoint) data
static const void *interrupt_data[NUM_USB_ENDPOINTS];
static volatile int interrupt_data_len[NUM_USB_ENDPOINTS];

#define CONTROL_WRITE_BUFSIZE	64
static struct usb_request control_write_rq;
static volatile uint16_t control_write_len;
static volatile uint8_t control_write_in_progress;
static uint8_t control_write_buf[CONTROL_WRITE_BUFSIZE];

static const struct usb_parameters *g_params;

static void initControlWrite(const struct usb_request *rq)
{
	memcpy(&control_write_rq, rq, sizeof(struct usb_request));
	control_write_len = 0;
	control_write_in_progress = 1;
}

static int wcslen(const wchar_t *str)
{
	int i=0;
	while (*str) {
		str++;
		i++;
	}
	return i;
}

static char allocEndpoint(uint8_t number, uint8_t type, uint8_t epsize, uint8_t ints)
{
	UENUM = number;
	UECONX = 1<<EPEN; // activate endpoint
	UECFG0X = type;
	UEIENX = ints;
	UECFG1X = (epsize<<EPSIZE0) | (1<<ALLOC);
	UEINTX = 0;

	if (!(UESTA0X & (1<<CFGOK))) {
		//printf_P("CFG EP%d fail\r\n", number);
		return -1;
	}

	return 0;
}

static uint8_t getEndpointSize(uint8_t epnum)
{
	uint8_t eps = g_params->epconfigs[epnum].size;


	if (eps == 1)
		return 16;
	if (eps == 2)
		return 32;
	if (eps == 3)
		return 64;

	return 8;
}

static char setupEndpoints(void)
{
	int i;
	int r = 0;
	const struct usb_ep_cfg *epcfg = g_params->epconfigs;

	for (i=0; i<NUM_USB_ENDPOINTS; i++)
	{
		interrupt_data[i] = NULL;
		interrupt_data_len[i] = -1;

		if (epcfg[i].enabled) {
			uint8_t ints;

			if (i==0) {
				ints = (1<<RXSTPE) | (1<<RXOUTE);
			} else if ((epcfg[i].type & 1) == EP_TYPE_IN) {
				ints = (1<<TXINE);
			} else {
				ints = (1 << RXOUTE);
			}

			r += allocEndpoint(i, epcfg[i].type, epcfg[i].size, ints);
		}
	}

	if (r) {
		return -1;
	}

	return 0;
}

// Requires UENUM already set
static uint16_t getEPlen(void)
{
#ifdef UEBCHX
	return UEBCLX | (UEBCHX << 8);
#else
	return UEBCLX;
#endif
}

// Requires UENUM already set
// writes up to n bytes
static uint16_t readEP2buf_n(void *dstbuf, int n)
{
	uint16_t len;
	int i;
	uint8_t *dst = dstbuf;
#ifdef UEBCHX
	len = UEBCLX | (UEBCHX << 8);
#else
	len = UEBCLX;
#endif

	for (i=0; i<len && i<n; i++) {
		*dst = UEDATX;
		dst++;
	}

	return i;
}

// Requires UENUM already set
static uint16_t readEP2buf(uint8_t *dst)
{
	uint16_t len;
	int i;
#ifdef UEBCHX
	len = UEBCLX | (UEBCHX << 8);
#else
	len = UEBCLX;
#endif

	for (i=0; i<len; i++) {
		*dst = UEDATX;
		dst++;
	}

	return len;
}

static void buf2EP(uint8_t epnum, const void *src, uint16_t len, uint16_t max_len, uint8_t progmem)
{
	int i;


	UENUM = epnum;  // select endpoint

	if (len > max_len) {
		len = max_len;
	}

	if (progmem) {
		const unsigned char *s = src;
		for (i=0; i<len; i++) {
			UEDATX = pgm_read_byte(s);
			s++;
		}
	} else {
		const unsigned char *s = src;
		for (i=0; i<len; i++) {
			UEDATX = *s;
			s++;
		}
	}
}

/**
 */
static void longDescriptorHelper(const uint8_t *data, uint16_t len, uint16_t rq_len, uint8_t progmem)
{
	uint16_t todo = rq_len > len ? len : rq_len;
	uint16_t pos = 0;
	uint8_t epsize = getEndpointSize(0);

	while(1)
	{
		if (todo > epsize) {
			buf2EP(0, data+pos, epsize, epsize, progmem);
			UEINTX &= ~(1<<TXINI);
			pos += epsize;
			todo -= epsize;
			while (!(UEINTX & (1<<TXINI)));
		}
		else {
			buf2EP(0, data+pos, todo,
					todo,
					progmem);
			UEINTX &= ~(1<<TXINI);
			while (!(UEINTX & (1<<TXINI)));
			break;
		}
	}
}

static void setupCbAnswer(const void *src, uint16_t len, uint8_t is_pgmspace)
{
	buf2EP(0, src, len, len, is_pgmspace);
}

static void handleSetupPacket(struct usb_request *rq)
{
	char unhandled = 0;
	char res;

	if (USB_RQT_IS_HOST_TO_DEVICE(rq->bmRequestType))
	{
		switch (rq->bmRequestType & USB_RQT_RECIPIENT_MASK)
		{
			case USB_RQT_RECIPIENT_DEVICE:
				switch (rq->bRequest)
				{
					case USB_RQ_SET_ADDRESS:
						UDADDR = rq->wValue;
						while (!(UEINTX & (1<<TXINI)));
						UEINTX &= ~(1<<TXINI);
						while (!(UEINTX & (1<<TXINI)));
						UDADDR |= (1<<ADDEN);
						if (!rq->wValue) {
							g_device_state = STATE_DEFAULT;
						} else {
							g_device_state = STATE_ADDRESS;
						}
						break;

					case USB_RQ_SET_CONFIGURATION:
						g_current_config = rq->wValue;
						if (!g_current_config) {
							g_device_state = STATE_ADDRESS;
						} else {
							g_device_state = STATE_CONFIGURED;
						}
						while (!(UEINTX & (1<<TXINI)));
						UEINTX &= ~(1<<TXINI);
						break;

					default:
						unhandled = 1;
				}
				break; // USB_RQT_RECIPIENT_DEVICE

			case USB_RQT_RECIPIENT_INTERFACE:
				switch(rq->bmRequestType & (USB_RQT_TYPE_MASK))
				{
					case USB_RQT_CLASS:
						switch(rq->bRequest)
						{
								// Request we "support" by just acknowledging
							case CDC_CLSRQ_SEND_BREAK:
							case HID_CLSRQ_SET_IDLE:
								while (!(UEINTX & (1<<TXINI)));
								UEINTX &= ~(1<<TXINI);
								break;

								// Requets we "support" by just receiving what's sent
							case CSC_CLSRQ_SET_LINE_CODING:
							case CDC_CLSRQ_SET_CONTROL_LINE_STATE:
							case HID_CLSRQ_SET_REPORT:
								while (!(UEINTX & (1<<TXINI)));
								UEINTX &= ~(1<<TXINI);
								initControlWrite(rq);
								break;
							default:
								unhandled = 1;
						}
						break;
					default:
						unhandled = 1;
				}

				break;

			case USB_RQT_RECIPIENT_ENDPOINT:
			case USB_RQT_RECIPIENT_OTHER:
			default:
				break;
		}
	}

	// Request where we send data to the host. Handlers
	// simply load the endpoint buffer and transmission
	// is handled automatically.
	if (USB_RQT_IS_DEVICE_TO_HOST(rq->bmRequestType))
	{
		switch (rq->bmRequestType & USB_RQT_RECIPIENT_MASK)
		{
			case USB_RQT_RECIPIENT_DEVICE:
				switch (rq->bRequest)
				{
					case USB_RQ_GET_STATUS:
						{
							unsigned char status[2] = { 0x00, 0x00 };
							// status[0] & 0x01 : Self powered
							// status[1] & 0x02 : Remote wakeup
							buf2EP(0, status, 2, rq->wLength, 0);
						}
						break;

					case USB_RQ_GET_CONFIGURATION:
						{
							if (g_device_state != STATE_CONFIGURED) {
								unsigned char zero = 0;
								buf2EP(0, &zero, 1, rq->wLength, 0);
							} else {
								buf2EP(0, &g_current_config, 1, rq->wLength, 0);
							}
						}
						break;

					case USB_RQ_GET_DESCRIPTOR:
						switch (rq->wValue >> 8)
						{
							case DEVICE_DESCRIPTOR:
								buf2EP(0, (unsigned char*)g_params->devdesc,
										sizeof(struct usb_device_descriptor), rq->wLength,
										g_params->flags & USB_PARAM_FLAG_DEVDESC_PROGMEM);
								break;
							case CONFIGURATION_DESCRIPTOR:
								// Check index if more than 1 config
								longDescriptorHelper(g_params->configdesc,
													g_params->configdesc_ttllen,
													rq->wLength,
													g_params->flags & USB_PARAM_FLAG_CONFDESC_PROGMEM);


								break;
							case STRING_DESCRIPTOR:
								{
									int id, len, slen;
									struct usb_string_descriptor_header hdr;

									id = (rq->wValue & 0xff);
									if (id > 0 && id <= g_params->num_strings)
									{
										id -= 1; // Our string table is zero-based

										len = rq->wLength;
										slen = wcslen(g_params->strings[id]) << 1;

										hdr.bLength = sizeof(hdr) + slen;
										hdr.bDescriptorType = STRING_DESCRIPTOR;

										buf2EP(0, (unsigned char*)&hdr, 2, len, 0);
										len -= 2;
										buf2EP(0, (unsigned char*)g_params->strings[id], slen, len, 0);
									}
									else if (id == 0) // Table of supported languages (string id 0)
									{
										unsigned char languages[4] = {
											4, STRING_DESCRIPTOR, 0x09, 0x10 // English (Canadian)
										};
										buf2EP(0, languages, 4, rq->wLength, 0);
									}
									else
									{
										//printf_P(PSTR("Unknown string id\r\n"));
									}
								}
								break;

							case DEVICE_QUALIFIER_DESCRIPTOR:
								// Full speed devices must respond with a request error.
								unhandled = 1;
								break;


							default:
								//printf_P(PSTR("Unhandled descriptor 0x%02x\n"), rq->wValue>>8);
								unhandled = 1;
						}
						break;

					default:
						unhandled = 1;
				}
				break;

			case USB_RQT_RECIPIENT_INTERFACE:
				switch(rq->bmRequestType & (USB_RQT_TYPE_MASK))
				{
					case USB_RQT_STANDARD:
						switch (rq->bRequest)
						{
							case USB_RQ_GET_STATUS:
								{ // 9.4.5 Get Status, Figure 9-5. Reserved (0)
									unsigned char status[2] = { 0x00, 0x00 };
									buf2EP(0, status, 2, rq->wLength, 0);
								}
								break;

							case USB_RQ_GET_DESCRIPTOR:
								switch (rq->wValue >> 8)
								{
									case REPORT_DESCRIPTOR:
										{
											// HID 1.1 : 7.1.1 Get_Descriptor request. wIndex is the interface number.
											//
											if (rq->wIndex > g_params->n_hid_interfaces) {
												unhandled = 1;
												break;
											}

											longDescriptorHelper(g_params->hid_params[rq->wIndex].reportdesc,
																g_params->hid_params[rq->wIndex].reportdesc_len,
																rq->wLength,
																g_params->flags & USB_PARAM_FLAG_REPORTDESC_PROGMEM);

										}
										break;

									default:
										unhandled = 1;
								}
								break;

							default:
								unhandled = 1;
						}
						break;

					case USB_RQT_CLASS:
						switch (rq->bRequest)
						{
							case HID_CLSRQ_GET_REPORT:
								{
									// HID 1.1 : 7.2.1 Get_Report request. wIndex is the interface number.
									if (rq->wIndex > g_params->n_hid_interfaces)
										break;

									if (g_params->hid_params[rq->wIndex].getReport) {
										const unsigned char *data;
										uint16_t len;
										len = g_params->hid_params[rq->wIndex].getReport(rq, &data);
										if (len) {
											buf2EP(0, data, len, rq->wLength, 0);
										}
									} else {
										// Treat as not-supported (i.e. STALL endpoint)
										unhandled = 1;
									}
								}
								break;
							default:
								unhandled = 1;
						}
						break;

					default:
						unhandled = 1;
				}
				break;

			case USB_RQT_RECIPIENT_ENDPOINT:
				switch (rq->bRequest)
				{
					case USB_RQ_GET_STATUS:
						{ // 9.4.5 Get Status, Figure 0-6
							unsigned char status[2] = { 0x00, 0x00 };
							// status[0] & 0x01 : Halt
							buf2EP(0, status, 2, rq->wLength, 0);
						}
						break;
					default:
						unhandled = 1;
				}
				break;

			case USB_RQT_RECIPIENT_OTHER:
			default:
				unhandled = 1;
		}

		if (!unhandled)
		{
			// Handle transmission now
			UEINTX &= ~(1<<TXINI);
			while (1)
			{
				if (UEINTX & (1<<TXINI)) {
					UEINTX &= ~(1<<TXINI);
				}

				if (UEINTX & (1<<RXOUTI)) {
					break;
				}
			}

			UEINTX &= ~(1<<RXOUTI); // ACK
		}
	} // IS DEVICE-TO-HOST

	if (unhandled) {
		if (g_params->setupCb)
		{
			res = g_params->setupCb(rq, setupCbAnswer);

			if (res) {

				// -1 used to signal an out packet incoming
				if (res == -1) {
					initControlWrite(rq);
				}
				else
				{
					// Handle transmission now
					UEINTX &= ~(1<<TXINI);
					while (1)
					{
						if (UEINTX & (1<<TXINI)) {
							UEINTX &= ~(1<<TXINI);
						}

						if (UEINTX & (1<<RXOUTI)) {
							break;
						}
					}
					UEINTX &= ~(1<<RXOUTI); // ACK
				}

				// handled
				return;
			}
		}
		UECONX |= (1<<STALLRQ);
	}

}

static void handleDataPacket(const struct usb_request *rq, uint8_t *dat, uint16_t len)
{
	if ((rq->bmRequestType & (USB_RQT_TYPE_MASK)) == USB_RQT_CLASS) {

		// TODO : Cechk for HID_CLSRQ_SET_REPORT in rq->bRequest

		// HID 1.1 : 7.2.2 Set_Report request. wIndex is the interface number.

		if (rq->wIndex > g_params->n_hid_interfaces)
			return;

		if (g_params->hid_params[rq->wIndex].setReport) {
			if (g_params->hid_params[rq->wIndex].setReport(rq, dat, len)) {
				UECONX |= (1<<STALLRQ);
			} else {
				// xmit status
				UEINTX &= ~(1<<TXINI);
			}
			return;
		}
	}

	if ((rq->bmRequestType & (USB_RQT_TYPE_MASK)) == USB_RQT_VENDOR) {

		if (g_params->handleDataPacket) {
			if (g_params->handleDataPacket(rq, dat, len)) {
				UECONX |= (1<<STALLRQ);
			} else {
				// xmit status
				UEINTX &= ~(1<<TXINI);
				while (!(UEINTX & (1<<TXINI)));
			}
			return;
		}
	}
}

// Device interrupt
ISR(USB_GEN_vect)
{
	uint8_t i;
	i = UDINT;

	if (i & (1<<SUSPI)) {
		UDINT &= ~(1<<SUSPI);
		g_usb_suspend = 1;
		UDIEN |= (1<<WAKEUPE);
		// CPU could now be put in low power mode. Later,
		// WAKEUPI would wake it up.
	}

	// this interrupt is to wakeup the cpu from sleep mode.
	if (i & (1<<WAKEUPI)) {
		UDINT &= ~(1<<WAKEUPE);
		if (g_usb_suspend) {
			g_usb_suspend = 0;
			UDIEN &= ~(1<<WAKEUPE); // woke up. Not needed anymore.
		}
	}

	if (i & (1<<EORSTI)) {
		g_usb_suspend = 0;
		setupEndpoints();
		UDINT &= ~(1<<EORSTI);
	}

	if (i & (1<<SOFI)) {
		UDINT &= ~(1<<SOFI);
	}

	if (i & (1<<EORSMI)) {
		UDINT &= ~(1<<EORSMI);
	}

	if (i & (1<<UPRSMI)) {
		UDINT &= ~(1<<UPRSMI);
	}
}

// Endpoint interrupt
ISR(USB_COM_vect)
{
	uint8_t ueint;
	uint8_t i;
	uint8_t ep;

	ueint = UEINT;

	/* EP0 handling */
	if (ueint & (1<<EPINT0)) {
		UENUM = 0;
		i = UEINTX;

		if (i & (1<<RXSTPI)) {
			struct usb_request rq;
			//readEP2buf(g_ep0_buf);
			readEP2buf_n(&rq, sizeof(struct usb_request));
			UEINTX &= ~(1<<RXSTPI);

			handleSetupPacket(&rq);
		}

		if (i & (1<<RXOUTI)) {
			uint16_t len;

			len = getEPlen();

			if (control_write_in_progress) {
				if (control_write_len + len < CONTROL_WRITE_BUFSIZE) {
					readEP2buf(control_write_buf + control_write_len);
					UEINTX &= ~(1<<RXOUTI);

					control_write_len += len;

					// Status phase now
					if (control_write_len >= control_write_rq.wLength) {
						UEINTX &= ~(1<<TXINI);
						while (!(UEINTX&(1<<TXINI)));
						handleDataPacket(&control_write_rq, control_write_buf, control_write_len);
						return;
					}
				}
			}

			UEINTX &= ~(1<<RXOUTI);
		}
	}

	/* Other endpoint handling */
	for (ep=1; ep < NUM_USB_ENDPOINTS; ep++)
	{
		if (g_params->epconfigs[ep].enabled) {
			if (ueint & (1 << ep)) {
				UENUM = ep;
				i = UEINTX;

				// Interrupt In and Bulk In endpoints will have this interrupt
				if (i & (1<<TXINI)) {
					if (interrupt_data_len[ep] < 0) {
						// If there's not already data waiting to be
						// sent, disable the interrupt.
						UEIENX &= ~(1<<TXINE);
					} else {
						UEINTX &= ~(1<<TXINI);
						buf2EP(ep,
								interrupt_data[ep],
								interrupt_data_len[ep],
								interrupt_data_len[ep],
								0);

						interrupt_data[ep] = NULL;
						interrupt_data_len[ep] = -1;
						UEINTX &= ~(1<<FIFOCON);
					}
				}

				// Interrupt Out and Bulk Out endpoints will get this
				if (i & (1<<RXOUTI)) {
					uint8_t count;
					// First, acknowledge the interrupt
					UEINTX &= ~(1<<RXOUTI);

					if (g_params->epconfigs[ep].onByteReceived) {
						// get the byte count
						count = UEBCLX;
						while (count--) {
							g_params->epconfigs[ep].onByteReceived(UEDATX);
						}
					}

					// free the bank
					UEINTX &= ~(1<<FIFOCON);
//					printf("RX\r\n");
				}
			}
		}
	}

}

char usb_interruptReady(int ep)
{
	return interrupt_data_len[ep] == -1;
}

void usb_interruptSend(int ep, const void *data, int len)
{
	uint8_t sreg = SREG;

	while (interrupt_data_len[ep] != -1) { }

	cli();

	interrupt_data[ep] = data;
	interrupt_data_len[ep] = len;

	UENUM = ep;
	UEIENX |= (1<<TXINE);

	SREG = sreg;
}

void usb_shutdown(void)
{
	UDCON |= (1<<DETACH);

	// Disable interrupts
	UDIEN = 0;


	USBCON &= ~(1<<USBE);
	USBCON |= (1<<FRZCLK); // initial value
#ifdef UHWCON
	UHWCON &= ~(1<<UVREGE); // Disable USB pad regulator
#endif
}

#define STATE_WAIT_VBUS	0
#define STATE_ATTACHED	1
static unsigned char usb_state;

void usb_doTasks(void)
{
	switch (usb_state)
	{
		default:
			usb_state = STATE_WAIT_VBUS;
		case STATE_WAIT_VBUS:
#ifdef USBSTA
			if (USBSTA & (1<<VBUS)) {
#endif
				//printf_P(PSTR("ATTACH\r\n"));
				UDCON &= ~(1<<DETACH); // clear DETACH bit
				usb_state = STATE_ATTACHED;
#ifdef USBSTA
			}
#endif
			break;
		case STATE_ATTACHED:
			break;
	}
}

#if defined(__AVR_ATmega32U2__)

/* Atmega32u2 datasheet 8.11.6, PLLCSR.
 * But register summary says PLLP0... */
#ifndef PINDIV
#define PINDIV	2
#endif
static void pll_init(void)
{
#if F_EXTERNAL==8000000L
	PLLCSR = 0;
#elif F_EXTERNAL==16000000L
	PLLCSR = (1<<PINDIV);
#else
	#error Unsupported clock frequency
#endif
	PLLCSR |= (1<<PLLE);
	while (!(PLLCSR&(1<<PLOCK))) {
		// wait for PLL lock
	}
}
#else
static void pll_init(void)
{
#if F_EXTERNAL==8000000L
	// The PLL generates a clock that is 24x a nominal 2MHz input.
	// Hence, we need to divide by 4 the external 8MHz crystal
	// frequency.
	PLLCSR = (1<<PLLP1)|(1<<PLLP0);
#elif F_EXTERNAL==16000000L
	// The PLL generates a clock that is 24x a nominal 2MHz input.
	// Hence, we need to divide by 8 the external 16MHz crystal
	// frequency.
	PLLCSR = (1<<PLLP2)|(1<<PLLP0);
#else
	#error Unsupported clock frequency
#endif

	PLLCSR |= (1<<PLLE);
	while (!(PLLCSR&(1<<PLOCK))) {
		// wait for PLL lock
	}
}
#endif

void usb_init(const struct usb_parameters *params)
{
	// Initialize the registers to the default values
	// from the datasheet. The bootloader that sometimes
	// runs before we get here (when doing updates) leaves
	// different values...
#ifdef UHWCON
	UHWCON = 0x80;
#endif
	USBCON = 0x20;
	UDCON = 0x01;
	UDIEN = 0x00;
	UDADDR = 0x00;

	g_params = params;

	// Set some initial values
	USBCON &= ~(1<<USBE);
	USBCON |= (1<<FRZCLK); // initial value
#ifdef UHWCON
	UHWCON |= (1<<UVREGE); // Enable USB pad regulator
	UHWCON &= ~(1<<UIDE);
	UHWCON |= (1<UIMOD);
#endif

#ifdef UPOE
	UPOE = 0; // Disable direct drive of USB pins
#endif
#ifdef REGCR
	REGCR = 0; // Enable the regulator
#endif

	pll_init();

	USBCON |= (1<<USBE);
	USBCON &= ~(1<<FRZCLK); // Unfreeze clock
#ifdef OTGPADE
	USBCON |= (1<<OTGPADE);
#endif

#ifdef LSM
	// Select full speed mode
	UDCON &= (1<<LSM);
#endif

	setupEndpoints();

	UDINT &= ~(1<<SUSPI);
	UDIEN = (1<<SUSPE) | (1<<EORSTE) |/* (1<<SOFE) |*/ (1<<WAKEUPE) | (1<<EORSME) | (1<<UPRSME);
}
