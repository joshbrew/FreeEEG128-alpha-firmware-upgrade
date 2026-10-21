#ifndef MOCK_CDC_H
#define MOCK_CDC_H
#include <stdint.h>
#define USBD_OK 0
#define USBD_BUSY 1
#define USBD_STATE_CONFIGURED 3
typedef struct { void *pClassData; int dev_state; } USBD_HandleTypeDef;
typedef struct { int TxState; } USBD_CDC_HandleTypeDef;
uint8_t CDC_Transmit_HS(uint8_t *buffer, uint16_t length);
#endif
