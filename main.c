/*	smsprogr : Programmer for SMS and GG cartridges.
 *	Copyright (C) 2020-2021  Raphael Assenat <raph@raphnet.net>
 *
 *  Based on:
 *
 *  ppusbcomm : USB to Parallel port transfer
 *	Copyright (C) 2015  Raphael Assenat <raph@raphnet.net>
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
#include <util/delay.h>

#include "util.h"
#include "usb.h"
#include "bootloader.h"
#include "usbcomm.h"

#include "usbstrings.h"
#include "menu.h"

#define MAX_READ_ERRORS	30

#define CS_INTERFACE	0x24

struct class_specific_descriptor {
	struct usb_cdc_functional_descriptor_header header;
	struct usb_cdc_acm_functional_descriptor acm;
	struct usb_cdc_union_functional_descriptor unio;
	struct usb_cdc_call_management_functional_descriptor call;
};

struct cfg1 {
	struct usb_configuration_descriptor configdesc;

	struct usb_interface_descriptor interface_comm;
	struct usb_endpoint_descriptor ep1_in;
	struct class_specific_descriptor cls;

	struct usb_interface_descriptor interface_data;
	struct usb_endpoint_descriptor ep2_in;
	struct usb_endpoint_descriptor ep3_out;

};

static const struct cfg1 cfg1 PROGMEM = {
	.configdesc = {
		.bLength = sizeof(struct usb_configuration_descriptor),
		.bDescriptorType = CONFIGURATION_DESCRIPTOR,
		.wTotalLength = sizeof(cfg1), // includes all descriptors returned together
		.bNumInterfaces = 2,
		.bConfigurationValue = 1,
		.bmAttributes = CFG_DESC_ATTR_RESERVED, // set Self-powred and remote-wakeup here if needed.
		.bMaxPower = 25, // for 50mA
	},

	.interface_comm = {
		.bLength = sizeof(struct usb_interface_descriptor),
		.bDescriptorType = INTERFACE_DESCRIPTOR,
		.bInterfaceNumber = 0,
		.bAlternateSetting = 0,
		.bNumEndpoints = 1,
		.bInterfaceClass = USB_DEVICE_CLASS_CDC,
		.bInterfaceSubClass = CDC_SUBCLASS_ABSTRACT_CONTROL,
		.bInterfaceProtocol = 0x01, // PCCA-101
		.iInterface = 0,
	},

	.ep1_in = {
		.bLength = sizeof(struct usb_endpoint_descriptor),
		.bDescriptorType = ENDPOINT_DESCRIPTOR,
		.bEndpointAddress = USB_RQT_DEVICE_TO_HOST | 1, // 0x81
		.bmAttributes = TRANSFER_TYPE_INT,
		.wMaxPacketsize = 8,
		.bInterval = LS_FS_INTERVAL_MS(10),
	},

	.interface_data = {
		.bLength = sizeof(struct usb_interface_descriptor),
		.bDescriptorType = INTERFACE_DESCRIPTOR,
		.bInterfaceNumber = 1,
		.bAlternateSetting = 0,
		.bNumEndpoints = 2,
		.bInterfaceClass = 0x0a, // data
		.bInterfaceSubClass = 0,
		.bInterfaceProtocol = 0,
		.iInterface = 0,
	},

	.ep2_in = {
		.bLength = sizeof(struct usb_endpoint_descriptor),
		.bDescriptorType = ENDPOINT_DESCRIPTOR,
		.bEndpointAddress = USB_RQT_DEVICE_TO_HOST | 2,
		.bmAttributes = TRANSFER_TYPE_BULK,
		.wMaxPacketsize = 64,
		.bInterval = LS_FS_INTERVAL_MS(10),
	},

	.ep3_out = {
		.bLength = sizeof(struct usb_endpoint_descriptor),
		.bDescriptorType = ENDPOINT_DESCRIPTOR,
		.bEndpointAddress = USB_RQT_HOST_TO_DEVICE | 3,
		.bmAttributes = TRANSFER_TYPE_BULK,
		.wMaxPacketsize = 8,
		.bInterval = LS_FS_INTERVAL_MS(10),
	},

	.cls = {
		.header = {
			.bFunctionLength = sizeof(struct usb_cdc_functional_descriptor_header),
			.bDescriptorType = CS_INTERFACE,
			.bDescriptorSubType = 0, // header
			.bcdCDC = 0x0110,
		},

		.acm = {
			.bFunctionLength = sizeof(struct usb_cdc_acm_functional_descriptor),
			.bDescriptorType = CS_INTERFACE,
			.bDescriptorSubType = 0x02, // abstract cntrol management
			.bmCapabilities = 0,
		},

		.unio = {
			.bFunctionLength = sizeof(struct usb_cdc_union_functional_descriptor),
			.bDescriptorType = CS_INTERFACE,
			.bDescriptorSubType = 0x06, // union functional descriptor
			.bControlInterface = 0, // communication class interface
			.bSubordinateInterface0 = 1, // data class interface
		},

		.call = {
			.bFunctionLength = sizeof(struct usb_cdc_call_management_functional_descriptor),
			.bDescriptorType = CS_INTERFACE,
			.bDescriptorSubType = 0x01, // call management
			.bmCapabilities = 0,
			.bDataInterface = 1,
		}
	}

};

const struct usb_device_descriptor device_descriptor_cdcacm PROGMEM = {
	.bLength = sizeof(struct usb_device_descriptor),
	.bDescriptorType = DEVICE_DESCRIPTOR,
	.bcdUSB = 0x0101,
	.bDeviceClass = USB_DEVICE_CLASS_CDC,
	.bDeviceSubClass = 0,
	.bDeviceProtocol = 0,
	.bMaxPacketSize = 64,
	.idVendor = 0x289B,
	.idProduct = 0x0600,
	.bcdDevice = VERSIONBCD, // 1.1.1
	.bNumConfigurations = 1,
	.iManufacturer = 1,
	.iProduct = 2,
	.iSerialNumber = 3,
};


/** **/

static struct usb_parameters usb_params_cdcacm = {
	.flags = USB_PARAM_FLAG_CONFDESC_PROGMEM |
				USB_PARAM_FLAG_DEVDESC_PROGMEM |
					USB_PARAM_FLAG_REPORTDESC_PROGMEM,
	.devdesc = (PGM_VOID_P)&device_descriptor_cdcacm,
	.configdesc = (PGM_VOID_P)&cfg1,
	.configdesc_ttllen = sizeof(cfg1),
	.num_strings = NUM_USB_STRINGS,
	.strings = g_usb_strings,

	.n_hid_interfaces = 0,

	.epconfigs = {
		[0] = { 1, EP_TYPE_CTL, EP_SIZE_64 },
		[1] = { 1, EP_TYPE_INT | EP_TYPE_IN, EP_SIZE_8 },
		[2] = { 1, EP_TYPE_BULK | EP_TYPE_IN, EP_SIZE_64 },
		[3] = { 1, EP_TYPE_BULK | EP_TYPE_OUT, EP_SIZE_8, usbcomm_addbyte },
	},
};


void hwinit(void)
{
	/* PORTB
	 *
	 * 7: D7    Input pull up
	 * 6: D6	Input pull up
	 * 5: D5	Input pull up
	 * 4: D4	Input pull up
	 * 3: D3	Input pull up
	 * 2: D2	Input pull up
	 * 1: D1	Input pull up
	 * 0: D0	Input pull up
	 */
	DDRB = 0x00;
	PORTB = 0xFF;

	/* PORTC
	 *
	 * 7: CPLD7	Output low LE
	 * 6: CPLD8	Output low - used for clocking
	 * 5: nCE	Output high
	 * 4: nWR	Output high
	 * 3: (no such pin)
	 * 2: nRD	Output high
	 * 1: RESET	(N/A: Reset input per fuses) (left "floating")
	 * 0: XTAL2	(N/A: Crystal oscillator) (left "floating")
	 */
	DDRC = 0xFC;
	PORTC = 0x34;

	/* PORTD
	 *
	 * 7: HWB		Input (external pull-up)
	 * 6: CPLD6     Output low	---- not connected - error on PCB ----
	 * 5: CPLD5		Output low  REG1
	 * 4: CPLD4		Output low	REG0
	 * 3: CPLD3		Output low
	 * 2: CPLD2		Output low
	 * 1: CPLD1		Output low
	 * 0: CPLD0		Output low
	 */
	DDRD = 0x7F;
	PORTD = 0x00;

	// System clock. External crystal is 16 Mhz and we want
	// to run at max. speed. (no prescaler)
	CLKPR = 0x80;
	CLKPR = 0x0; // Division factor of 1
	PRR0 = 0;
	PRR1 = 0;
}

static uint16_t cdcacm_sendBytes(const uint8_t *data, uint16_t length)
{
	if (usb_interruptReady(2)) {
		usb_interruptSend(2, data, length);
	//	while (!usb_interruptReady(2));
		return length;
	}
	return 0;
}

#define CMDBUF_SIZE	24

static uint8_t cmdbuf[CMDBUF_SIZE];
static uint16_t cmdbufpos = 0;

int main(void)
{
	uint8_t b;


	hwinit();

	usbcomm_init(cdcacm_sendBytes, USBCOMM_EN_STDOUT);
	usb_init(&usb_params_cdcacm);

	sei();

	printf("Ready!\r\n");

	while (1)
	{
		usb_doTasks();
		usbcomm_doTasks();


		if (usbcomm_hasData())
		{
			b = usbcomm_rxbyte();
			//printf("[%02x] ", b);

			if ((b=='\n')) {
			}
			else if (b=='\r') {
				// store a zero instead to terminate the string
				cmdbuf[cmdbufpos] = 0;

				menu_handleLine(cmdbuf, cmdbufpos);

				// Data has been consumed. Clear buffer.
				cmdbufpos=0;
			}
			else {
				// echo
				putchar(b);

				cmdbuf[cmdbufpos] = b;
				cmdbufpos++;
			}

			if (cmdbufpos >= CMDBUF_SIZE) {
				cmdbufpos = 0;
				printf_P(PSTR("Line too long\r\n"));
			}
		}
	}

	return 0;
}

