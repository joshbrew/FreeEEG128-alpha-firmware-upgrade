#ifndef FREEEEG128_H
#define FREEEEG128_H
#include <stdint.h>
/* USB IRQ only enqueues bytes. All parsing, SPI and replies run in main. */
void FreeEEG128_Receive(const uint8_t *data, uint32_t length);
void FreeEEG128_DataReady(uint16_t pin);
void FreeEEG128_Run(void) __attribute__((noreturn));
#endif
