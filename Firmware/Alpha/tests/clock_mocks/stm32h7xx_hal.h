#ifndef CLOCK_MOCK_HAL_H
#define CLOCK_MOCK_HAL_H
#include <stdint.h>
#define HAL_OK 0
#define HAL_ERROR 1
#define REV_ID_V 0x2003u
#define RCC_SYSCLK_DIV1 1u
#define RCC_SYSCLK_DIV2 2u
#define RCC_HCLK_DIV1 1u
#define RCC_HCLK_DIV2 2u
#define RCC_HCLK_DIV4 4u
#define RCC_HCLK_DIV8 8u
#define PWR_REGULATOR_VOLTAGE_SCALE0 0u
#define PWR_REGULATOR_VOLTAGE_SCALE1 1u
#define PWR_REGULATOR_VOLTAGE_SCALE2 2u
#define PWR_REGULATOR_VOLTAGE_SCALE3 3u
#define RCC_PLL1VCOWIDE 1u
#define RCC_PLL1VCOMEDIUM 0u
#define FLASH_LATENCY_0 0u
#define FLASH_LATENCY_1 1u
#define RCC_OSCILLATORTYPE_HSI48 1u
#define RCC_OSCILLATORTYPE_HSI 2u
#define RCC_OSCILLATORTYPE_HSE 4u
#define RCC_HSI_DIV1 1u
#define RCC_HSICALIBRATION_DEFAULT 64u
#define RCC_HSI48_ON 1u
#define RCC_HSE_OFF 0u
#define RCC_HSE_BYPASS 2u
#define RCC_PLL_ON 1u
#define RCC_PLL1VCIRANGE_3 3u
#define RCC_PLL2VCIRANGE_3 3u
#define RCC_PLL2VCOWIDE 1u
#define RCC_PLLCKSELR_PLLSRC 3u
#define RCC_PLLCKSELR_PLLSRC_HSE 3u
#define RCC_PLLCKSELR_PLLSRC_HSI 2u
#define RCC_PLLCKSELR_PLLSRC_CSI 1u
#define RCC_PLLSOURCE_HSE RCC_PLLCKSELR_PLLSRC_HSE
#define RCC_PLLSOURCE_HSI RCC_PLLCKSELR_PLLSRC_HSI
typedef struct {
    uint32_t PLLState, PLLSource, PLLM, PLLN, PLLP, PLLQ, PLLR, PLLRGE, PLLVCOSEL, PLLFRACN;
} PLL_Config;
typedef struct {
    uint32_t OscillatorType, HSIState, HSICalibrationValue, HSI48State, HSEState;
    PLL_Config PLL;
} RCC_OscInitTypeDef;
typedef struct {
    uint32_t PLL2M, PLL2N, PLL2P, PLL2Q, PLL2R, PLL2RGE, PLL2VCOSEL, PLL2FRACN;
} PLL2_Config;
typedef struct { PLL2_Config PLL2; } RCC_PeriphCLKInitTypeDef;
typedef struct { uint32_t PLLCKSELR; } Mock_RCC;
extern Mock_RCC mock_rcc;
#define RCC (&mock_rcc)
uint32_t HAL_GetREVID(void);
int HAL_RCC_OscConfig(RCC_OscInitTypeDef *config);
#endif
