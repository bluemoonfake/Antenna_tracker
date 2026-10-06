#ifndef STM32H7XX_HAL_CONF_H
#define STM32H7XX_HAL_CONF_H

#define HAL_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_EXTI_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_I2C_MODULE_ENABLED
#define HAL_PCD_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_SPI_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED

#define HSE_VALUE                            8000000UL
#define HSE_STARTUP_TIMEOUT                  100UL
#define HSI_VALUE                            64000000UL
#define CSI_VALUE                            4000000UL
#define LSI_VALUE                            32000UL
#define LSE_VALUE                            32768UL
#define LSE_STARTUP_TIMEOUT                  5000UL
#define EXTERNAL_CLOCK_VALUE                 12288000UL
#define VDD_VALUE                            3300UL
#define TICK_INT_PRIORITY                    15UL
#define USE_RTOS                             0U
#define USE_SD_TRANSCEIVER                   0U
#define USE_HAL_ADC_REGISTER_CALLBACKS       0U
#define USE_HAL_CEC_REGISTER_CALLBACKS       0U
#define USE_HAL_COMP_REGISTER_CALLBACKS      0U
#define USE_HAL_CORDIC_REGISTER_CALLBACKS    0U
#define USE_HAL_CRYP_REGISTER_CALLBACKS      0U
#define USE_HAL_DAC_REGISTER_CALLBACKS       0U
#define USE_HAL_DCMI_REGISTER_CALLBACKS      0U
#define USE_HAL_DFSDM_REGISTER_CALLBACKS     0U
#define USE_HAL_DMA2D_REGISTER_CALLBACKS     0U
#define USE_HAL_DSI_REGISTER_CALLBACKS       0U
#define USE_HAL_ETH_REGISTER_CALLBACKS       0U
#define USE_HAL_FDCAN_REGISTER_CALLBACKS     0U
#define USE_HAL_FMAC_REGISTER_CALLBACKS      0U
#define USE_HAL_GFXMMU_REGISTER_CALLBACKS    0U
#define USE_HAL_HASH_REGISTER_CALLBACKS      0U
#define USE_HAL_HCD_REGISTER_CALLBACKS       0U
#define USE_HAL_HRTIM_REGISTER_CALLBACKS     0U
#define USE_HAL_I2C_REGISTER_CALLBACKS       0U
#define USE_HAL_I2S_REGISTER_CALLBACKS       0U
#define USE_HAL_IRDA_REGISTER_CALLBACKS      0U
#define USE_HAL_JPEG_REGISTER_CALLBACKS      0U
#define USE_HAL_LPTIM_REGISTER_CALLBACKS     0U
#define USE_HAL_LTDC_REGISTER_CALLBACKS      0U
#define USE_HAL_MDIOS_REGISTER_CALLBACKS     0U
#define USE_HAL_MDMA_REGISTER_CALLBACKS      0U
#define USE_HAL_MMC_REGISTER_CALLBACKS       0U
#define USE_HAL_NAND_REGISTER_CALLBACKS      0U
#define USE_HAL_NOR_REGISTER_CALLBACKS       0U
#define USE_HAL_OPAMP_REGISTER_CALLBACKS     0U
#define USE_HAL_OSPI_REGISTER_CALLBACKS      0U
#define USE_HAL_OTFDEC_REGISTER_CALLBACKS    0U
#define USE_HAL_PCD_REGISTER_CALLBACKS       0U
#define USE_HAL_QSPI_REGISTER_CALLBACKS      0U
#define USE_HAL_RNG_REGISTER_CALLBACKS       0U
#define USE_HAL_RTC_REGISTER_CALLBACKS       0U
#define USE_HAL_SAI_REGISTER_CALLBACKS       0U
#define USE_HAL_SD_REGISTER_CALLBACKS        0U
#define USE_HAL_SDRAM_REGISTER_CALLBACKS     0U
#define USE_HAL_SMARTCARD_REGISTER_CALLBACKS 0U
#define USE_HAL_SMBUS_REGISTER_CALLBACKS     0U
#define USE_HAL_SPDIFRX_REGISTER_CALLBACKS   0U
#define USE_HAL_SPI_REGISTER_CALLBACKS       0U
#define USE_HAL_SWPMI_REGISTER_CALLBACKS     0U
#define USE_HAL_TIM_REGISTER_CALLBACKS       0U
#define USE_HAL_UART_REGISTER_CALLBACKS      0U
#define USE_HAL_USART_REGISTER_CALLBACKS     0U
#define USE_HAL_WWDG_REGISTER_CALLBACKS      0U

#include "stm32h7xx_hal_rcc.h"
#include "stm32h7xx_hal_gpio.h"
#include "stm32h7xx_hal_dma.h"
#include "stm32h7xx_hal_cortex.h"
#include "stm32h7xx_hal_flash.h"
#include "stm32h7xx_hal_exti.h"
#include "stm32h7xx_hal_i2c.h"
#include "stm32h7xx_hal_pcd.h"
#include "stm32h7xx_hal_pwr.h"
#include "stm32h7xx_hal_spi.h"
#include "stm32h7xx_hal_tim.h"
#include "stm32h7xx_hal_uart.h"

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line);
#define assert_param(expression)                                                                   \
  ((expression) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))
#else
#define assert_param(expression) ((void)0U)
#endif

#endif
