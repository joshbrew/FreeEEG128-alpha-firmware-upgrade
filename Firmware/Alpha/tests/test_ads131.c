#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef _MSC_VER
#define __attribute__(x)
#endif
#include "../Src/ads131_board.c"
SPI_HandleTypeDef hspi2, hspi3, hspi4, hspi5;
GPIO_TypeDef test_gpio;
uint32_t SystemCoreClock = 80000000, test_primask;
uint32_t runtime_micros(void) { static uint32_t now; return now++; }
void test_set_primask(uint32_t mask) { test_primask = mask; }
static uint16_t registers[16][63], pending[16];
static unsigned selected = 99, fail_transfer, corrupt_crc, dma_calls;
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, int value)
{
    (void)port;
    if (pin > 16) return;
    if (!value) { assert(selected == 99 || selected / 4 == (pin - 1) / 4); selected = pin - 1; }
    else if (selected == pin - 1) selected = 99;
}
void HAL_Delay(uint32_t ms) { (void)ms; }
int HAL_SPI_TransmitReceive(SPI_HandleTypeDef *spi, uint8_t *out, uint8_t *in, uint16_t bytes, uint32_t timeout)
{
    unsigned adc = selected, i;
    uint16_t opcode, crc;
    assert(bytes == 30 && timeout == 20 && adc < 16 && buses[adc % 4] == spi);
    if (fail_transfer) return HAL_ERROR;
    memset(in, 0, bytes);
    in[0] = (uint8_t)(pending[adc] >> 8); in[1] = (uint8_t)pending[adc]; pending[adc] = 0;
    for (i = 0; i < 24; ++i) in[3 + i] = (uint8_t)(adc * 24 + i);
    opcode = (uint16_t)((uint16_t)out[0] << 8 | out[1]);
    if ((opcode & 0xe000) == 0xa000) pending[adc] = registers[adc][(opcode >> 7) & 63];
    if ((opcode & 0xe000) == 0x6000) registers[adc][(opcode >> 7) & 63] = (uint16_t)((uint16_t)out[3] << 8 | out[4]);
    crc = ads_crc(in, 27); in[27] = (uint8_t)(crc >> 8); in[28] = (uint8_t)crc;
    if (corrupt_crc) in[5] ^= 1;
    return HAL_OK;
}
int HAL_SPI_TransmitReceive_DMA(SPI_HandleTypeDef *spi, uint8_t *out, uint8_t *in, uint16_t bytes)
{
    int result = HAL_SPI_TransmitReceive(spi, out, in, bytes, 20);
    ++dma_calls;
    /* DMA runs on all four buses concurrently; remember selected ADC per frame. */
    selected = 99; spi->state = 1;
    return result;
}
int HAL_SPI_Abort(SPI_HandleTypeDef *spi) { spi->state = 0; return HAL_OK; }
int HAL_SPI_GetState(SPI_HandleTypeDef *spi) { return spi->state; }
int HAL_SPI_GetError(SPI_HandleTypeDef *spi) { return spi->error; }
int main(void)
{
    unsigned adc, i, round;
    uint16_t value;
    uint8_t channels[384];
    for (adc = 0; adc < 16; ++adc) registers[adc][0] = 0x2800;
    assert(ads_crc((const uint8_t *)"123456789", 9) == 0x29b1);
    assert(ads_init());
    for (adc = 0; adc < 16; ++adc) assert(registers[adc][2] == 0x110);
    for (i = 0; i < 4; ++i) {
        assert(ads_rate(250u << i));
        for (adc = 0; adc < 16; ++adc) assert(registers[adc][3] == (0xff42 | ((7 - i) << 2)));
    }
    for (i = 0; i < 128; ++i) assert(ads_gain(i, 1u << (i % 8)));
    for (adc = 0; adc < 16; ++adc) {
        assert(ads_read(adc, 4, &value) && value == 0x3210);
        assert(ads_read(adc, 5, &value) && value == 0x7654);
    }
    assert(!ads_gain(128, 8) && !ads_gain(127, 3) && !ads_rate(512));
    for (round = 0; round < 4; ++round) {
        assert(ads_begin_round(round)); assert(!ads_round_ready());
        for (i = 0; i < 4; ++i) buses[i]->state = 0;
        assert(ads_round_ready() == 1 && ads_finish_round(round, channels));
    }
    assert(dma_calls == 16);
    for (i = 0; i < sizeof(channels); ++i) assert(channels[i] == (uint8_t)i);
    corrupt_crc = 1; assert(!ads_read(15, 4, &value));
    assert(ads_begin_round(0));
    for (i = 0; i < 4; ++i) buses[i]->state = 0;
    assert(!ads_finish_round(0, channels));
    corrupt_crc = 0; fail_transfer = 1;
    assert(!ads_read(0, 4, &value) && !ads_begin_round(0) && active == 0);
    puts("ADS131M08: 16 selects, delayed register replies, rates, 128 gains, CRC, parallel rounds and failures passed.");
    return 0;
}
