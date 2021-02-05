
#define USBCOMM_EN_STDOUT	1

void usbcomm_init(uint16_t (*ll_tx)(const uint8_t *data, uint16_t len), uint8_t flags);
void usbcomm_doTasks(void);

/* Inject a byte in the receive buffer
 * (data from the host) */
void usbcomm_addbyte(uint8_t b);

/* Add a byte to the output buffer */
void usbcomm_txbyte(uint8_t b);
void usbcomm_txbytes(uint8_t *data, int len);
void usbcomm_drain();

/* Check if there is received data pending */
uint8_t usbcomm_hasData(void);

/* Read one byte from the receive buffer */
uint8_t usbcomm_rxbyte(void);
