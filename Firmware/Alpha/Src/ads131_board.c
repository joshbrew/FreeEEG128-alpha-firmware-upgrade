#include "ads131_board.h"
#include "main.h"
#include "runtime.h"
#include <string.h>

extern SPI_HandleTypeDef hspi2, hspi3, hspi4, hspi5;
static SPI_HandleTypeDef *const buses[4] = {&hspi5, &hspi2, &hspi4, &hspi3};
/* Preserve the upstream devices[0..15] ordering, including SPI2/SPI3's
   nonsequential select wiring. Each ADC contributes eight adjacent channels. */
static GPIO_TypeDef *const ports[16] = {
    SPI5_NSS_GPIO_Port, SPI2_NSS_GPIO_Port, SPI4_NSS_GPIO_Port, SPI3_NSS3_GPIO_Port,
    SPI5_NSS2_GPIO_Port, SPI2_NSS4_GPIO_Port, SPI4_NSS2_GPIO_Port, SPI3_NSS4_GPIO_Port,
    SPI5_NSS3_GPIO_Port, SPI2_NSS3_GPIO_Port, SPI4_NSS3_GPIO_Port, SPI3_NSS_GPIO_Port,
    SPI5_NSS4_GPIO_Port, SPI2_NSS2_GPIO_Port, SPI4_NSS4_GPIO_Port, SPI3_NSS2_GPIO_Port
};
static const uint16_t pins[16] = {
    SPI5_NSS_Pin, SPI2_NSS_Pin, SPI4_NSS_Pin, SPI3_NSS3_Pin,
    SPI5_NSS2_Pin, SPI2_NSS4_Pin, SPI4_NSS2_Pin, SPI3_NSS4_Pin,
    SPI5_NSS3_Pin, SPI2_NSS3_Pin, SPI4_NSS3_Pin, SPI3_NSS_Pin,
    SPI5_NSS4_Pin, SPI2_NSS2_Pin, SPI4_NSS4_Pin, SPI3_NSS2_Pin
};
DMA_BUFFER static uint8_t tx[4][32] __attribute__((aligned(32)));
DMA_BUFFER static uint8_t rx[4][32] __attribute__((aligned(32)));
static unsigned active;

uint16_t ads_crc(const uint8_t *data, unsigned length)
{
    uint16_t crc = 0xffff;
    unsigned bit;
    while (length--) {
        crc ^= (uint16_t)*data++ << 8;
        for (bit = 0; bit < 8; ++bit)
            crc = (uint16_t)((crc << 1) ^ ((crc & 0x8000u) ? 0x1021u : 0u));
    }
    return crc;
}

/* Always transfer complete 10-word frames. RREG responses arrive in the next
   frame, not in the command frame. Never use the legacy shared DMA scratch. */
static int exchange(unsigned adc, uint16_t opcode, uint16_t data, uint16_t *response)
{
    uint8_t out[ADS_FRAME_BYTES] = {0}, in[ADS_FRAME_BYTES];
    int ok;
    if (adc >= EEG_ADCS) return 0;
    out[0] = (uint8_t)(opcode >> 8); out[1] = (uint8_t)opcode;
    out[3] = (uint8_t)(data >> 8); out[4] = (uint8_t)data;
    HAL_GPIO_WritePin(ports[adc], pins[adc], GPIO_PIN_RESET);
    ok = HAL_SPI_TransmitReceive(buses[adc % 4], out, in, sizeof(out), 20) == HAL_OK;
    HAL_GPIO_WritePin(ports[adc], pins[adc], GPIO_PIN_SET);
    if (!ok || ads_crc(in, 27) != ((uint16_t)in[27] << 8 | in[28])) return 0;
    if (response) *response = (uint16_t)((uint16_t)in[0] << 8 | in[1]);
    return 1;
}

int ads_read(unsigned adc, unsigned reg, uint16_t *value)
{
    if (reg > 0x3e || !value) return 0;
    return exchange(adc, (uint16_t)(0xa000u | reg << 7), 0, 0) && exchange(adc, 0, 0, value);
}
int ads_write(unsigned adc, unsigned reg, uint16_t value)
{
    uint16_t actual;
    return reg >= ADS_MODE && reg <= 0x3e &&
           exchange(adc, (uint16_t)(0x6000u | reg << 7), value, 0) &&
           ads_read(adc, reg, &actual) && (actual & (reg == ADS_MODE ? 0xfbffu : 0xffffu)) == (value & (reg == ADS_MODE ? 0xfbffu : 0xffffu));
}

uint16_t ads_clock_value(uint32_t hz)
{
    /* HR fMOD=8.192MHz/2; external reference matches this PCB's REF2025. */
    unsigned osr = hz == 250 ? 7 : hz == 500 ? 6 : hz == 1000 ? 5 : 4;
    return (uint16_t)(0xff00u | 0x40u | (osr << 2) | 2u);
}
void ads_sync(void)
{
    /* A 2 us pulse (1..2047 CLKIN periods) synchronizes without resetting
       registers. Mask IRQs so a long USB interrupt cannot stretch it into RESET. */
    uint32_t primask = __get_PRIMASK(), start;
    __disable_irq();
    HAL_GPIO_WritePin(ADC1_SYNC_RESET_GPIO_Port, ADC1_SYNC_RESET_Pin, GPIO_PIN_RESET);
    start = runtime_micros();
    while ((uint32_t)(runtime_micros() - start) < 2u) { __NOP(); }
    HAL_GPIO_WritePin(ADC1_SYNC_RESET_GPIO_Port, ADC1_SYNC_RESET_Pin, GPIO_PIN_SET);
    __set_PRIMASK(primask);
}
int ads_rate(uint32_t hz)
{
    unsigned adc;
    int ok = 1;
    if (hz != 250 && hz != 500 && hz != 1000 && hz != 2000) return 0;
    for (adc = 0; adc < EEG_ADCS; ++adc)
        if (!ads_write(adc, ADS_CLOCK, ads_clock_value(hz))) ok = 0;
    ads_sync();
    return ok;
}
int ads_gain(unsigned channel, unsigned gain)
{
    unsigned code = 0, shift = (channel % 4) * 4, reg = ADS_GAIN1 + (channel % 8) / 4;
    uint16_t value;
    if (channel >= EEG_CHANNELS || !gain || gain > 128 || (gain & (gain - 1))) return 0;
    while ((1u << code) != gain) ++code;
    if (!ads_read(channel / 8, reg, &value)) return 0;
    return ads_write(channel / 8, reg, (uint16_t)((value & ~(15u << shift)) | code << shift));
}
int ads_init(void)
{
    unsigned adc;
    uint16_t id;
    int ok = 1;
    for (adc = 0; adc < EEG_ADCS; ++adc) HAL_GPIO_WritePin(ports[adc], pins[adc], GPIO_PIN_SET);
    HAL_GPIO_WritePin(ADC1_SYNC_RESET_GPIO_Port, ADC1_SYNC_RESET_Pin, GPIO_PIN_RESET);
    HAL_Delay(2);
    HAL_GPIO_WritePin(ADC1_SYNC_RESET_GPIO_Port, ADC1_SYNC_RESET_Pin, GPIO_PIN_SET);
    HAL_Delay(10);
    for (adc = 0; adc < EEG_ADCS; ++adc) {
        /* ID CHANCNT[11:8]=8. Fixed 24-bit words, CCITT output CRC, level DRDY. CHECK drains ADC1
           during rate measurement; unread FIFOs can change DRDY behavior. */
        if (!ads_read(adc, 0, &id) || (id & 0x0f00u) != 0x0800u ||
            !ads_write(adc, ADS_MODE, 0x0110u)) ok = 0;
    }
    return ok;
}

void ads_abort(void)
{
    unsigned bus, adc;
    for (bus = 0; bus < 4; ++bus) if (active & (1u << bus)) HAL_SPI_Abort(buses[bus]);
    active = 0;
    for (adc = 0; adc < EEG_ADCS; ++adc) HAL_GPIO_WritePin(ports[adc], pins[adc], GPIO_PIN_SET);
}
int ads_begin_round(unsigned round)
{
    unsigned bus;
    if (round >= 4 || active) return 0;
    for (bus = 0; bus < 4; ++bus) {
        unsigned adc = round * 4 + bus;
        memset(tx[bus], 0, sizeof(tx[bus]));
        HAL_GPIO_WritePin(ports[adc], pins[adc], GPIO_PIN_RESET);
        active |= 1u << bus;
        if (HAL_SPI_TransmitReceive_DMA(buses[bus], tx[bus], rx[bus], ADS_FRAME_BYTES) != HAL_OK) {
            ads_abort(); return 0;
        }
    }
    return 1;
}
int ads_round_ready(void)
{
    unsigned bus;
    for (bus = 0; bus < 4; ++bus) {
        if (HAL_SPI_GetError(buses[bus]) != HAL_SPI_ERROR_NONE) return -1;
        if (HAL_SPI_GetState(buses[bus]) != HAL_SPI_STATE_READY) return 0;
    }
    return 1;
}
int ads_finish_round(unsigned round, uint8_t *channels)
{
    unsigned bus;
    int ok = 1;
    if (round >= 4 || active != 15 || ads_round_ready() != 1) return 0;
    __DMB();
    for (bus = 0; bus < 4; ++bus) {
        unsigned adc = round * 4 + bus;
        HAL_GPIO_WritePin(ports[adc], pins[adc], GPIO_PIN_SET);
        if (ads_crc(rx[bus], 27) != ((uint16_t)rx[bus][27] << 8 | rx[bus][28])) ok = 0;
        memcpy(channels + adc * 24, rx[bus] + 3, 24);
    }
    active = 0;
    return ok;
}
