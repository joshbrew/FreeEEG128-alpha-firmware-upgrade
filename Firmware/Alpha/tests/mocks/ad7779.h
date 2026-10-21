#ifndef MOCK_AD7779_H
#define MOCK_AD7779_H
#include <stdint.h>
#define AD7779_REG_SRC_N_MSB 96
#define AD7779_REG_SRC_N_LSB 97
#define AD7779_REG_SRC_IF_MSB 98
#define AD7779_REG_SRC_IF_LSB 99
#define AD7779_REG_SRC_UPDATE 100
#define AD7779_SRC_LOAD_SOURCE 128
#define AD7779_SRC_LOAD_UPDATE 1
#define AD7779_REG_CH_CONFIG(x) (x)
#define AD7779_REG_GENERAL_USER_CONFIG_2 18
#define AD7771_FILTER_MODE 64
typedef int ad7779_ch;
typedef int ad7779_gain;
typedef struct { uint8_t regs[128]; } ad7779_dev;
int ad7779_spi_int_reg_read(ad7779_dev *dev, uint8_t reg, uint8_t *value);
int ad7779_spi_int_reg_write_mask(ad7779_dev *dev, uint8_t reg, uint8_t mask, uint8_t value);
int ad7779_spi_int_reg_write(ad7779_dev *dev, uint8_t reg, uint8_t value);
int ad7779_set_dec_rate(ad7779_dev *dev, uint16_t integer, uint16_t fractional);
int ad7779_set_gain(ad7779_dev *dev, ad7779_ch channel, ad7779_gain gain);
#endif
