#ifndef CONTROL_PROTOCOL_H
#define CONTROL_PROTOCOL_H
#include <stdint.h>
#define CONTROL_LINE_SIZE 80
typedef enum {
    CONTROL_NONE, CONTROL_ERROR, CONTROL_HELP, CONTROL_INFO, CONTROL_STATUS,
    CONTROL_STOP, CONTROL_START, CONTROL_RATE, CONTROL_GAIN, CONTROL_REBOOT, CONTROL_BOOTLOADER,
    CONTROL_DIAG, CONTROL_SETTINGS, CONTROL_SAVE, CONTROL_LOAD, CONTROL_FACTORY, CONTROL_PRESET, CONTROL_CHECK,
    CONTROL_SPIREAD, /* diagnostic: ADS131M08 16-bit register read */
    CONTROL_SPIWRITE /* diagnostic: reserved; raw writes are rejected */
} control_kind;
typedef struct { control_kind kind; uint32_t first, second, third; } control_command;
typedef struct { char line[CONTROL_LINE_SIZE]; unsigned used; int overflow; } control_parser;
/* Lines must start with !. Ignore legacy host commands and unprefixed traffic. */
control_command control_feed(control_parser *parser, uint8_t byte);
#endif
