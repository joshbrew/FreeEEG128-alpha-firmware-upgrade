#ifndef ADS131_BOARD_H
#define ADS131_BOARD_H
#include <stdint.h>
#define EEG_ADCS 16u
#define EEG_CHANNELS 128u
#define EEG_FRAME_BYTES 393u
#define ADS_FRAME_BYTES 30u /* response + eight 24-bit channels + output CRC */
#define ADS_MODE 2u
#define ADS_CLOCK 3u
#define ADS_GAIN1 4u
#define ADS_GAIN2 5u
int ads_init(void);
int ads_read(unsigned adc, unsigned reg, uint16_t *value);
int ads_write(unsigned adc, unsigned reg, uint16_t value);
int ads_rate(uint32_t hz);
uint16_t ads_clock_value(uint32_t hz);
int ads_gain(unsigned channel, unsigned gain);
void ads_sync(void);
/* Four independent SPI buses, four rounds in original channel order. */
int ads_begin_round(unsigned round);
int ads_round_ready(void); /* 1 complete, 0 busy, -1 SPI fault */
int ads_finish_round(unsigned round, uint8_t *channels);
void ads_abort(void);
uint16_t ads_crc(const uint8_t *bytes, unsigned length);
#endif
