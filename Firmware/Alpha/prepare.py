"""Materialize the pinned 80 MHz source archive and apply the FreeEEG128 patches.

Generated sources go in Firmware/build/source. Edit Alpha sources, not that directory.
No network downloads or third party Python packages are required.
"""
from pathlib import Path, PurePosixPath
import hashlib
import re
import shutil
import tarfile

HERE = Path(__file__).resolve().parent
ARCHIVE = HERE.parent / 'Base' / 'STM32H743ZI_ads131_80MHz_makefile.tar.gz'
ARCHIVE_SHA256 = 'b288351888cc8145a18c6b43ed905d9a9197daf5e3e17843cde82b197189634a'
SOURCE = HERE.parent / 'build' / 'source'


def replace(text, old, new, count=1):
    if text.count(old) != count:
        raise ValueError(f'Base source changed: expected {count} matches for {old[:90]!r}')
    return text.replace(old, new)


def prepare():
    if hashlib.sha256(ARCHIVE.read_bytes()).hexdigest() != ARCHIVE_SHA256:
        raise ValueError('Base archive SHA256 mismatch; refusing to patch unknown firmware')
    SOURCE.mkdir(parents=True, exist_ok=True)
    with tarfile.open(ARCHIVE) as archive:
        for member in archive.getmembers():
            parts = PurePosixPath(member.name).parts[1:]
            if not member.isfile() or not parts or parts[0] == 'build':
                continue
            if '..' in parts or any(':' in p for p in parts):
                raise ValueError('Unsafe archive member')
            target = SOURCE.joinpath(*parts)
            target.parent.mkdir(parents=True, exist_ok=True)
            data = archive.extractfile(member).read()
            if target.suffix.lower() in ('.c', '.h', '.s', '.ld') or target.name == 'Makefile':
                data = data.replace(b'\r\r\n', b'\n').replace(b'\r\n', b'\n')
            target.write_bytes(data)

    def edit(name, transform):
        path = SOURCE / name
        path.write_text(transform(path.read_text(encoding='utf-8')), encoding='utf-8', newline='\n')

    def function(source, signature):
        start = source.index(signature + '\n{')
        brace = source.index('{', start)
        depth, end = 1, brace + 1
        while depth:
            depth += (source[end] == '{') - (source[end] == '}')
            end += 1
        return source[start:end]

    def main(s):
        notice = s[:s.index('/* Includes')]
        handles = '\n'.join(re.findall(r'^\w+_HandleTypeDef \w+;', s, re.M))
        names = ['MX_GPIO_Init', 'MX_DMA_Init', 'MX_SPI2_Init', 'MX_SPI3_Init',
                 'MX_SPI4_Init', 'MX_SPI5_Init', 'MX_USART1_UART_Init', 'MX_NVIC_Init']
        init = '\n'.join(function(s, 'static void ' + n + '(void)') for n in names)
        clock = function(s, 'void SystemClock_Config(void)')
        clock = replace(clock, 'RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;',
                        'RCC_ClkInitStruct.SYSCLKDivider = FREEEEG128_CORE_DIVIDER;')
        clock = replace(clock, 'RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;',
                        'RCC_ClkInitStruct.AHBCLKDivider = FREEEEG128_AHB_DIVIDER;')
        clock = replace(clock, '__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);',
                        '__HAL_RCC_SYSCFG_CLK_ENABLE();\n  if (HAL_PWREx_ControlVoltageScaling(FREEEEG128_VOLTAGE_SCALE) != HAL_OK) Error_Handler();')
        clock = replace(clock, '&RCC_ClkInitStruct, FLASH_LATENCY_0',
                        '&RCC_ClkInitStruct, FREEEEG128_FLASH_LATENCY')
        # Keep the board's peripheral routing, but share tested startup/fallback
        # logic between the application and the HSI-only recovery loader.
        clock = replace(clock, '  RCC_OscInitTypeDef RCC_OscInitStruct = {0};\n', '')
        begin = clock.index('  /** Macro to configure the PLL clock source')
        end = clock.index('  /** Initializes the CPU, AHB', begin)
        clock = clock[:begin] + '  if (!clock_start_pll()) Error_Handler();\n' + clock[end:]
        begin = clock.index('  PeriphClkInitStruct.PLL2.PLL2M = 1;')
        end = clock.index('  PeriphClkInitStruct.PLL2.PLL2FRACN = 0;', begin)
        end += len('  PeriphClkInitStruct.PLL2.PLL2FRACN = 0;')
        clock = clock[:begin] + '  clock_configure_pll2(&PeriphClkInitStruct);' + clock[end:]
        # All four ADC buses run SPI mode 1 at PLL2Q/4 = 8.192 MHz.
        # GPIO chip selects remain high except for the selected converter.
        result = (notice + '#include "main.h"\n#include "usb_device.h"\n'
                  '#include "freeeeg128.h"\n#include "clock_profile.h"\n#include "clock_start.h"\n' + handles + '\n'
                  + '\n'.join('static void ' + n + '(void);' for n in names)
                  + '\nvoid SystemClock_Config(void);\nint main(void)\n{\n  HAL_Init();\n  SystemClock_Config();\n'
                  + '\n'.join('  ' + n + '();' for n in names) +
                  '\n  MX_USB_DEVICE_Init();\n  FreeEEG128_Run();\n}\n' + clock + '\n' + init +
                  '\nvoid HAL_GPIO_EXTI_Callback(uint16_t pin) { FreeEEG128_DataReady(pin); }\n'
                  'void Error_Handler(void) { __disable_irq(); for (;;) { __NOP(); } }\n')
        return result
    edit('Src/main.c', main)
    edit('Src/stm32h7xx_it.c', lambda s: replace(s, 'void BusFault_Handler(void)',
                                              'void Legacy_BusFault_Handler(void)'))

    def usb(s):
        s = replace(s, '#include "usbd_cdc_if.h"', '#include "usbd_cdc_if.h"\n#include "freeeeg128.h"\n#include <string.h>')
        s = replace(s, '/* USER CODE BEGIN PV */', '/* USER CODE BEGIN PV */\nstatic uint8_t line_coding[7] = {0x00,0xc2,0x01,0x00,0,0,8};\nstatic uint16_t control_lines;')
        s = replace(s, '  case CDC_SET_LINE_CODING:\n\n    break;',
                    '  case CDC_SET_LINE_CODING:\n    if (length == sizeof(line_coding)) memcpy(line_coding, pbuf, sizeof(line_coding));\n    break;')
        s = replace(s, '  case CDC_GET_LINE_CODING:\n\n    break;',
                    '  case CDC_GET_LINE_CODING:\n    if (length == sizeof(line_coding)) memcpy(pbuf, line_coding, sizeof(line_coding));\n    break;')
        s = replace(s, '  case CDC_SET_CONTROL_LINE_STATE:\n\n    break;',
                    '  case CDC_SET_CONTROL_LINE_STATE:\n    control_lines = ((USBD_SetupReqTypedef *)pbuf)->wValue;\n    break;')
        s = replace(s, '  /* USER CODE BEGIN 11 */', '  /* USER CODE BEGIN 11 */\n  FreeEEG128_Receive(Buf, *Len);')
        s = replace(s, '  if (hcdc->TxState != 0){',
                    '  if (hUsbDeviceHS.dev_state != USBD_STATE_CONFIGURED || hcdc == NULL) { __set_PRIMASK(primask); return USBD_FAIL; }\n'
                    '  if (Len > sizeof(UserTxBufferHS)) { __set_PRIMASK(primask); return USBD_FAIL; }\n'
                    '  if (hcdc->TxState != 0){')
        s = replace(s, '  USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceHS.pClassData;',
                    '  uint32_t primask = __get_PRIMASK();\n  __disable_irq();\n'
                    '  USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceHS.pClassData;')
        s = replace(s, '    return USBD_BUSY;', '    __set_PRIMASK(primask);\n    return USBD_BUSY;')
        s = replace(s, '  USBD_CDC_SetTxBuffer(&hUsbDeviceHS, Buf, Len);',
                    '  memcpy(UserTxBufferHS, Buf, Len); /* Own data until USB IN completion. */\n'
                    '  USBD_CDC_SetTxBuffer(&hUsbDeviceHS, UserTxBufferHS, Len);')
        s = replace(s, '  result = USBD_CDC_TransmitPacket(&hUsbDeviceHS);',
                    '  result = USBD_CDC_TransmitPacket(&hUsbDeviceHS);\n  __set_PRIMASK(primask);')
        return s
    edit('Src/usbd_cdc_if.c', usb)
    edit('Src/usbd_desc.c', lambda s: s.replace('"STMicroelectronics"', '"FreeEEG128"').replace('"STM32 Virtual ComPort"', '"FreeEEG128 EEG + Control"'))
    # HSI48 USB clock accuracy is recovered from USB SOF with the H7 CRS peripheral.
    def clock(s):
        s = replace(s, '  HAL_PWREx_EnableUSBVoltageDetector();', '''  HAL_PWREx_EnableUSBVoltageDetector();
  __HAL_RCC_CRS_CLK_ENABLE();
  RCC_CRSInitTypeDef crs = {0};
  crs.Prescaler = RCC_CRS_SYNC_DIV1;
  crs.Source = RCC_CRS_SYNC_SOURCE_USB1; /* USB1 = OTG_HS, the port wired to PB14/PB15. */
  crs.Polarity = RCC_CRS_SYNC_POLARITY_RISING;
  crs.ReloadValue = __HAL_RCC_CRS_RELOADVALUE_CALCULATE(48000000, 1000);
  crs.ErrorLimitValue = RCC_CRS_ERRORLIMIT_DEFAULT;
  crs.HSI48CalibrationValue = RCC_CRS_HSI48CALIBRATION_DEFAULT;
  HAL_RCCEx_CRSConfig(&crs);''')
        return s
    edit('Src/usb_device.c', clock)
    # The generated suspend callback stops the PHY clock, but neither the HAL reset nor the
    # resume path restarts it, so a suspend (e.g. data cable connected after boot) leaves the
    # device unable to answer the next enumeration. The board is self-powered: keep it running.
    def usb_conf(s):
        s = replace(s, '  __HAL_PCD_GATE_PHYCLOCK(hpcd);\n',
                    '  /* PHY clock deliberately not gated: nothing restarts it on reset/resume. */\n')
        s = replace(s, '  /* USER CODE BEGIN 3 */\n',
                    '  /* USER CODE BEGIN 3 */\n  __HAL_PCD_UNGATE_PHYCLOCK(hpcd);\n')
        return s
    edit('Src/usbd_conf.c', usb_conf)
    # DMA scratch memory has no flash image. Without NOLOAD, objcopy fills the entire
    # address gap from internal flash to SRAM, producing a ~640 MB binary.
    edit('STM32H743ZITx_FLASH.ld', lambda s: replace(s, '.dma_buffer :', '.dma_buffer (NOLOAD) :'))
    for directory in ('Inc', 'Src'):
        for source in (HERE / directory).iterdir():
            shutil.copy2(source, SOURCE / directory / source.name)
    print(f'Prepared {SOURCE}')
    return SOURCE


if __name__ == '__main__':
    prepare()

