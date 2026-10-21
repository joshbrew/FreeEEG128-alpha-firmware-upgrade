#ifndef MOCK_MAIN_H
#define MOCK_MAIN_H
#include <stdint.h>
#define DMA_BUFFER
#define ADC1_DRDY_Pin 1
#define HAL_OK 0
#define HAL_ERROR 1
#define HAL_SAI_STATE_READY 0
#define HAL_SPI_STATE_READY 0
#define HAL_SPI_ERROR_NONE 0
#define GPIO_PIN_RESET 0
#define GPIO_PIN_SET 1
#define __NOP() ((void)0)
typedef struct { int state; int error; } SPI_HandleTypeDef;
typedef struct { int unused; } GPIO_TypeDef;
extern GPIO_TypeDef test_gpio;
int HAL_SPI_Abort(SPI_HandleTypeDef *spi);
int HAL_SPI_GetState(SPI_HandleTypeDef *spi);
int HAL_SPI_GetError(SPI_HandleTypeDef *spi);
int HAL_SPI_TransmitReceive(SPI_HandleTypeDef *spi, uint8_t *tx, uint8_t *rx, uint16_t bytes, uint32_t timeout);
int HAL_SPI_TransmitReceive_DMA(SPI_HandleTypeDef *spi, uint8_t *tx, uint8_t *rx, uint16_t bytes);
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, int value);
#define HAL_UART_STATE_READY 0
typedef struct { int state; } SAI_HandleTypeDef;
typedef struct { int state; } UART_HandleTypeDef;
int HAL_SAI_DMAStop(SAI_HandleTypeDef *sai);
int HAL_SAI_GetState(SAI_HandleTypeDef *sai);
uint32_t HAL_SAI_GetError(SAI_HandleTypeDef *sai);
int HAL_SAI_Receive_DMA(SAI_HandleTypeDef *sai, uint8_t *buffer, uint16_t words);
#define HAL_SAI_ERROR_AFSDET 0x4u
#define HAL_SAI_ERROR_LFSDET 0x8u
#define SAI_IT_AFSDET 0x20u
#define SAI_IT_LFSDET 0x40u
#define SAI_FLAG_AFSDET 0x20u
#define SAI_FLAG_LFSDET 0x40u
#define __HAL_SAI_DISABLE_IT(sai, it) ((void)(sai), (void)(it))
#define __HAL_SAI_CLEAR_FLAG(sai, flag) ((void)(sai), (void)(flag))
int HAL_UART_GetState(UART_HandleTypeDef *uart);
int HAL_UART_Transmit_DMA(UART_HandleTypeDef *uart, uint8_t *buffer, uint16_t size);
uint32_t HAL_GetTick(void);
extern uint32_t SystemCoreClock;
uint32_t HAL_RCC_GetHCLKFreq(void);
uint32_t HAL_GetREVID(void);
void HAL_Delay(uint32_t delay);
#define __DMB() ((void)0)
extern uint32_t test_primask;
void test_set_primask(uint32_t mask);
#define __get_PRIMASK() test_primask
#define __disable_irq() (test_primask = 1)
#define __set_PRIMASK(x) test_set_primask(x)
#define NVIC_SystemReset() ((void)0)
#endif

#define SPI5_NSS_GPIO_Port (&test_gpio)
#define SPI5_NSS_Pin 1

#define SPI2_NSS_GPIO_Port (&test_gpio)
#define SPI2_NSS_Pin 2

#define SPI4_NSS_GPIO_Port (&test_gpio)
#define SPI4_NSS_Pin 3

#define SPI3_NSS3_GPIO_Port (&test_gpio)
#define SPI3_NSS3_Pin 4

#define SPI5_NSS2_GPIO_Port (&test_gpio)
#define SPI5_NSS2_Pin 5

#define SPI2_NSS4_GPIO_Port (&test_gpio)
#define SPI2_NSS4_Pin 6

#define SPI4_NSS2_GPIO_Port (&test_gpio)
#define SPI4_NSS2_Pin 7

#define SPI3_NSS4_GPIO_Port (&test_gpio)
#define SPI3_NSS4_Pin 8

#define SPI5_NSS3_GPIO_Port (&test_gpio)
#define SPI5_NSS3_Pin 9

#define SPI2_NSS3_GPIO_Port (&test_gpio)
#define SPI2_NSS3_Pin 10

#define SPI4_NSS3_GPIO_Port (&test_gpio)
#define SPI4_NSS3_Pin 11

#define SPI3_NSS_GPIO_Port (&test_gpio)
#define SPI3_NSS_Pin 12

#define SPI5_NSS4_GPIO_Port (&test_gpio)
#define SPI5_NSS4_Pin 13

#define SPI2_NSS2_GPIO_Port (&test_gpio)
#define SPI2_NSS2_Pin 14

#define SPI4_NSS4_GPIO_Port (&test_gpio)
#define SPI4_NSS4_Pin 15

#define SPI3_NSS2_GPIO_Port (&test_gpio)
#define SPI3_NSS2_Pin 16

#define ADC1_SYNC_RESET_GPIO_Port (&test_gpio)
#define ADC1_SYNC_RESET_Pin 17
