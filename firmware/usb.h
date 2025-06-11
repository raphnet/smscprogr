#ifndef _usb_h__
#define _usb_h__

#include <stdint.h>
#include <stdlib.h>
#include <avr/pgmspace.h>

/**** Standard USB defines ****/

#define DEVICE_DESCRIPTOR			0x01
#define CONFIGURATION_DESCRIPTOR	0x02
#define STRING_DESCRIPTOR			0x03
#define INTERFACE_DESCRIPTOR		0x04
#define ENDPOINT_DESCRIPTOR			0x05
#define DEVICE_QUALIFIER_DESCRIPTOR	0x06

// USB HID 1.11 section 7.1.1
#define HID_DESCRIPTOR				0x21
#define REPORT_DESCRIPTOR			0x22
#define PHYSICAL_DESCRIPTOR			0x23

#define USB_RQT_IS_HOST_TO_DEVICE(r)	(((r) & 0x80) == 0)
#define USB_RQT_IS_DEVICE_TO_HOST(r)	(((r) & 0x80) == 0x80)
#define USB_RQT_HOST_TO_DEVICE		0x00
#define USB_RQT_DEVICE_TO_HOST		0x80
#define USB_RQT_STANDARD			(0x00 << 5)
#define USB_RQT_CLASS				(0x01 << 5)
#define USB_RQT_VENDOR				(0x02 << 5)
#define USB_RQT_TYPE_MASK			(0x03 << 5)
#define USB_RQT_RECIPIENT_DEVICE	0x00
#define USB_RQT_RECIPIENT_INTERFACE	0x01
#define USB_RQT_RECIPIENT_ENDPOINT	0x02
#define USB_RQT_RECIPIENT_OTHER		0x03
#define USB_RQT_RECIPIENT_MASK		0x1F

#define USB_RQ_GET_STATUS			0x00
#define USB_RQ_CLEAR_FEATURE		0x01
#define USB_RQ_SET_FEATURE			0x03
#define USB_RQ_SET_ADDRESS			0x05
#define USB_RQ_GET_DESCRIPTOR		0x06
#define USB_RQ_SET_DESCRIPTOR		0x07
#define USB_RQ_GET_CONFIGURATION	0x08
#define USB_RQ_SET_CONFIGURATION	0x09
#define USB_RQ_GET_INTERFACE		0x0A
#define USB_RQ_SET_INTERFACE		0x0B

/* If the one you need is missing, add it. For values,
 * see: http://www.usb.org/developers/defined_class
 */
#define USB_DEVICE_CLASS_AUDIO		0x01
#define USB_DEVICE_CLASS_CDC		0x02
#define USB_DEVICE_CLASS_HID		0x03
#define USB_DEVICE_CLASS_MASS_STORAGE	0x04
#define USB_DEVICE_CLASS_HUB		0x05
#define USB_DEVICE_CLASS_VENDOR		0xFF

struct usb_request {
	uint8_t bmRequestType;
	uint8_t bRequest;
	uint16_t wValue;
	uint16_t wIndex;
	uint16_t wLength;
};

struct usb_device_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType; // DEVICE_DESCRIPTOR
	uint16_t bcdUSB;
	uint8_t bDeviceClass; // class code
	uint8_t bDeviceSubClass;
	uint8_t bDeviceProtocol;
	uint8_t bMaxPacketSize; // Max packet size for endpoint zero
	uint16_t idVendor;
	uint16_t idProduct;
	uint16_t bcdDevice;
	uint8_t iManufacturer;
	uint8_t iProduct;
	uint8_t iSerialNumber;
	uint8_t bNumConfigurations;
};

#define CFG_DESC_ATTR_RESERVED		0x80
#define CFG_DESC_ATTR_SELF_POWERED	0x40
#define CFG_DESC_ATTR_REMOTE_WAKEUP	0x20
struct usb_configuration_descriptor {
	uint8_t bLength; // sizeof(struct usb_configuration_descriptor)
	uint8_t bDescriptorType; // CONFIGURATION_DESCRIPTOR
	uint16_t wTotalLength; // for all descriptors in this configuration. (Cfg, interface, endpoint, class)
	uint8_t bNumInterfaces;
	uint8_t bConfigurationValue; // for SET_CONFIGURATION argument(must be >= 1)
	uint8_t iConfiguration; // String descriptor index
	uint8_t bmAttributes; // D7 (set to one), D6 Selft-powred, D5 remote wakeup
	uint8_t bMaxPower; // in 2mA units
};

struct usb_interface_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType; // INTERFACE_DESCRIPTOR
	uint8_t bInterfaceNumber;
	uint8_t bAlternateSetting;
	uint8_t bNumEndpoints;
	uint8_t bInterfaceClass;
	uint8_t bInterfaceSubClass;
	uint8_t bInterfaceProtocol;
	uint8_t iInterface; // String descriptor index
};

#define TRANSFER_TYPE_CONTROL		0x0
#define TRANSFER_TYPE_ISOCHRONOUS	0x1
#define TRANSFER_TYPE_BULK			0x2
#define TRANSFER_TYPE_INT			0x3
#define LS_FS_INTERVAL_MS(v)	((v))		// At low/full speed, 1 ms units
#define HS_INTERVAL_US(v)		((v)/125)	// At high speed, .125us units
struct usb_endpoint_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bEndpointAddress;
	uint8_t bmAttributes;
	uint16_t wMaxPacketsize;
	uint8_t bInterval;
};

struct usb_string_descriptor_header {
	uint8_t bLength;
	uint8_t bDescriptorType;
};

/* HID 1.11 section 4.2 */
#define HID_SUBCLASS_NONE	0
#define	HUD_SUBCLASS_BOOT	1

/* HID 1.11 section 4.3 */
#define HID_PROTOCOL_NONE		0
#define HID_PROTOCOL_KEYBOARD	1
#define HID_PROTOCOL_MOUSE		2

/* Other values defined at HID 1.11, 6.2.1 */
#define HID_COUNTRY_NOT_SUPPORTED	0x00

struct usb_hid_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint16_t bcdHid;
	uint8_t bCountryCode;
	uint8_t bNumDescriptors;

	uint8_t bClassDescriptorType;
	uint16_t wClassDescriptorLength;
	// Note: At least one descriptor (REPORT) is
	// required. Additional descriptors
	// are declared as above (type, length) and
	// appended to the HID descriptor.
};

#define HID_CLSRQ_GET_REPORT	0x01
#define HID_CLSRQ_GET_IDLE		0x02
#define HID_CLSRQ_GET_PROTOCOL	0x03
#define HID_CLSRQ_SET_REPORT	0x09
#define HID_CLSRQ_SET_IDLE		0x0A
#define HID_CLSRQ_SET_PROTOCOL	0x0B

#define HID_REPORT_TYPE_INPUT	0x01
#define HID_REPORT_TYPE_OUTPUT	0x02
#define HID_REPORT_TYPE_FEATURE	0x03

/* CDC */

#define CDC_SUBCLASS_DIRECT_LINE_CONTROL	0x01
#define CDC_SUBCLASS_ABSTRACT_CONTROL		0x02
#define CDC_SUBCLASS_TELEPHONE_CONTROL		0x03

#define CSC_CLSRQ_SET_LINE_CODING			0x20
#define CDC_CLSRQ_SET_CONTROL_LINE_STATE	0x22
#define CDC_CLSRQ_SEND_BREAK				0x23

struct usb_cdc_functional_descriptor_header {
	uint8_t bFunctionLength;
	uint8_t bDescriptorType;
	uint8_t bDescriptorSubType;
	uint16_t bcdCDC;
};

struct usb_cdc_acm_functional_descriptor {
	uint8_t bFunctionLength;
	uint8_t bDescriptorType;
	uint8_t bDescriptorSubType;
	uint8_t bmCapabilities;
};

struct usb_cdc_union_functional_descriptor {
	uint8_t bFunctionLength;
	uint8_t bDescriptorType;
	uint8_t bDescriptorSubType;
	uint8_t bControlInterface;
	uint8_t bSubordinateInterface0;
};

struct usb_cdc_call_management_functional_descriptor {
	uint8_t bFunctionLength;
	uint8_t bDescriptorType;
	uint8_t bDescriptorSubType;
	uint8_t bmCapabilities;
	uint8_t bDataInterface;
};

/**** API ****/

#define USB_PARAM_FLAG_DEVDESC_PROGMEM		1
#define USB_PARAM_FLAG_CONFDESC_PROGMEM		2
#define USB_PARAM_FLAG_REPORTDESC_PROGMEM	4

#define MAX_HID_INTERFACES	2

struct usb_hid_parameters {
	uint16_t reportdesc_len;
	const unsigned char *reportdesc;

	// Warning: Called from interrupt handler. Implement accordingly.
	uint16_t (*getReport)(struct usb_request *rq, const uint8_t **dat);
	uint8_t (*setReport)(const struct usb_request *rq, const uint8_t *dat, uint16_t len);
};

#define EP_TYPE_CTL		(0 << EPTYPE0)
#define EP_TYPE_ISO		(1 << EPTYPE0)
#define EP_TYPE_BULK	(2 << EPTYPE0)
#define EP_TYPE_INT		(3 << EPTYPE0)
#define EP_TYPE_IN		0x01
#define EP_TYPE_OUT		0x00


#define EP_SIZE_8	0
#define EP_SIZE_16	1
#define EP_SIZE_32	2
#define EP_SIZE_64	3

#define NUM_USB_ENDPOINTS 4

struct usb_ep_cfg {
	uint8_t enabled;
	uint8_t type;
	uint8_t size;

	// function pointers for OUT endpoints
	void (*onByteReceived)(uint8_t b);
};

struct usb_parameters {
	uint8_t flags;

	// endpoing configration (for configuring the atmel USB controller).
	// should match the endpoint descriptors...
	struct usb_ep_cfg epconfigs[NUM_USB_ENDPOINTS];

	const struct usb_device_descriptor *devdesc;

	const void *configdesc;
	// length including all descriptors returned together
	// for getdescriptor(config). (i.e. Config descriptor +
	// interface descritor + hid and endpoints)
	uint16_t configdesc_ttllen;

	uint8_t num_strings;
	const wchar_t *const *strings;

	// Called when an unsupported setup packet arrives. Return non-zero when handled.
	uint8_t (*setupCb)(const struct usb_request *rq, void (*answerFunc)(const void *src, uint16_t len, uint8_t is_pgmspace));

	// Called when a vendor data packet arrives. Return non-zero when handled.
	uint8_t (*handleDataPacket)(const struct usb_request *rq, const uint8_t *dat, uint16_t len);


	uint8_t n_hid_interfaces;
	struct usb_hid_parameters hid_params[MAX_HID_INTERFACES];
};

char usb_interruptReady(int ep);
void usb_interruptSend(int ep, const void *data, int len);

void usb_init(const struct usb_parameters *params);
void usb_doTasks(void);
void usb_shutdown(void);


#endif // _usb_h__
