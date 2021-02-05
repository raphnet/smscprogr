#include <stdlib.h> // for wchar_t
#include "usbstrings.h"

const wchar_t *g_usb_strings[] = {
	[0] = L"raphnet.", 	// 1 : Vendor
	[1] = L"SMSCPROGRv0",	// 2: Product
	[2] = L"123456",	// 3 : Serial
};

