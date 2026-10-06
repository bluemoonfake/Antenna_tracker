#ifndef TEST_HAL_H
#define TEST_HAL_H
#include <stdint.h>
#include <stddef.h>
typedef int HAL_StatusTypeDef;
#define HAL_OK    0
#define HAL_ERROR 1
typedef struct
{
  int unused;
} GPIO_TypeDef;
typedef struct
{
  uint32_t AddressingMode;
  uint32_t AutoReloadPreload;
  uint32_t BaudRate;
  uint32_t BaudRatePrescaler;
  uint32_t CLKPhase;
  uint32_t CLKPolarity;
  uint32_t CRCCalculation;
  uint32_t CRCPolynomial;
  uint32_t ClockDivision;
  uint32_t CounterMode;
  uint32_t DataSize;
  uint32_t Direction;
  uint32_t DualAddressMode;
  uint32_t FifoThreshold;
  uint32_t FirstBit;
  uint32_t GeneralCallMode;
  uint32_t HwFlowCtl;
  uint32_t IOSwap;
  uint32_t MasterInterDataIdleness;
  uint32_t MasterKeepIOState;
  uint32_t MasterReceiverAutoSusp;
  uint32_t MasterSSIdleness;
  uint32_t Mode;
  uint32_t NSS;
  uint32_t NSSPMode;
  uint32_t NSSPolarity;
  uint32_t NoStretchMode;
  uint32_t OneBitSampling;
  uint32_t OverSampling;
  uint32_t OwnAddress1;
  uint32_t OwnAddress2;
  uint32_t OwnAddress2Masks;
  uint32_t Parity;
  uint32_t Period;
  uint32_t Prescaler;
  uint32_t RepetitionCounter;
  uint32_t RxCRCInitializationPattern;
  uint32_t StopBits;
  uint32_t TIMode;
  uint32_t Timing;
  uint32_t TxCRCInitializationPattern;
  uint32_t WordLength;
} TestInit;
typedef struct
{
  void *Instance;
  TestInit Init;
  struct
  {
    uint32_t AdvFeatureInit;
  } AdvancedInit;
} UART_HandleTypeDef;
typedef struct
{
  void *Instance;
  TestInit Init;
  struct
  {
    uint32_t AdvFeatureInit;
  } AdvancedInit;
} I2C_HandleTypeDef;
typedef struct
{
  void *Instance;
  TestInit Init;
  struct
  {
    uint32_t AdvFeatureInit;
  } AdvancedInit;
} SPI_HandleTypeDef;
typedef struct
{
  void *Instance;
  TestInit Init;
  struct
  {
    uint32_t AdvFeatureInit;
  } AdvancedInit;
} TIM_HandleTypeDef;
typedef struct
{
  uint32_t Pin;
  uint32_t Mode;
  uint32_t Pull;
  uint32_t Speed;
  uint32_t Alternate;
} GPIO_InitTypeDef;
typedef struct
{
  uint32_t OCMode;
  uint32_t Pulse;
  uint32_t OCPolarity;
  uint32_t OCNPolarity;
  uint32_t OCFastMode;
  uint32_t OCIdleState;
  uint32_t OCNIdleState;
} TIM_OC_InitTypeDef;
#define GPIO_AF1_TIM1                           1U
#define GPIO_AF4_I2C1                           2U
#define GPIO_AF4_I2C2                           3U
#define GPIO_AF5_SPI2                           4U
#define GPIO_AF7_USART1                         5U
#define GPIO_AF7_USART3                         6U
#define GPIO_MODE_AF_OD                         7U
#define GPIO_MODE_AF_PP                         8U
#define GPIO_MODE_OUTPUT_PP                     9U
#define GPIO_NOPULL                             10U
#define GPIO_PIN_10                             11U
#define GPIO_PIN_11                             12U
#define GPIO_PIN_13                             13U
#define GPIO_PIN_14                             14U
#define GPIO_PIN_15                             15U
#define GPIO_PIN_2                              16U
#define GPIO_PIN_3                              17U
#define GPIO_PIN_4                              18U
#define GPIO_PIN_5                              19U
#define GPIO_PIN_6                              20U
#define GPIO_PIN_7                              21U
#define GPIO_PIN_8                              22U
#define GPIO_PIN_9                              23U
#define GPIO_PIN_RESET                          24U
#define GPIO_PIN_SET                            25U
#define GPIO_PULLUP                             26U
#define GPIO_SPEED_FREQ_HIGH                    27U
#define I2C_ADDRESSINGMODE_7BIT                 28U
#define I2C_ANALOGFILTER_ENABLE                 29U
#define I2C_DUALADDRESS_DISABLE                 30U
#define I2C_GENERALCALL_DISABLE                 31U
#define I2C_MEMADD_SIZE_8BIT                    33U
#define I2C_NOSTRETCH_DISABLE                   34U
#define I2C_OA2_NOMASK                          35U
#define SPI_BAUDRATEPRESCALER_128               37U
#define SPI_CRCCALCULATION_DISABLE              38U
#define SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN 39U
#define SPI_DATASIZE_8BIT                       40U
#define SPI_DIRECTION_2LINES                    41U
#define SPI_FIFO_THRESHOLD_01DATA               42U
#define SPI_FIRSTBIT_MSB                        43U
#define SPI_IO_SWAP_DISABLE                     45U
#define SPI_MASTER_INTERDATA_IDLENESS_00CYCLE   46U
#define SPI_MASTER_KEEP_IO_STATE_ENABLE         47U
#define SPI_MASTER_RX_AUTOSUSP_DISABLE          48U
#define SPI_MASTER_SS_IDLENESS_00CYCLE          49U
#define SPI_MODE_MASTER                         50U
#define SPI_NSS_POLARITY_LOW                    51U
#define SPI_NSS_PULSE_DISABLE                   52U
#define SPI_NSS_SOFT                            53U
#define SPI_PHASE_2EDGE                         54U
#define SPI_POLARITY_HIGH                       55U
#define SPI_TIMODE_DISABLE                      57U
#define TIM_AUTORELOAD_PRELOAD_DISABLE          58U
#define TIM_CHANNEL_3                           59U
#define TIM_CHANNEL_4                           60U
#define TIM_CLOCKDIVISION_DIV1                  61U
#define TIM_COUNTERMODE_UP                      62U
#define TIM_OCFAST_DISABLE                      64U
#define TIM_OCIDLESTATE_RESET                   65U
#define TIM_OCMODE_PWM1                         66U
#define TIM_OCNIDLESTATE_RESET                  67U
#define TIM_OCNPOLARITY_HIGH                    68U
#define TIM_OCPOLARITY_HIGH                     69U
#define UART_ADVFEATURE_NO_INIT                 71U
#define UART_HWCONTROL_NONE                     72U
#define UART_MODE_TX_RX                         74U
#define UART_ONE_BIT_SAMPLE_DISABLE             75U
#define UART_OVERSAMPLING_16                    76U
#define UART_PARITY_NONE                        77U
#define UART_STOPBITS_1                         79U
#define UART_WORDLENGTH_8B                      80U
#define USART1_IRQn                             81U
#define USART3_IRQn                             82U
#define USART1                                  ((void *)(uintptr_t)1)
#define USART3                                  ((void *)(uintptr_t)2)
#define I2C1                                    ((void *)(uintptr_t)3)
#define I2C2                                    ((void *)(uintptr_t)4)
#define SPI2                                    ((void *)(uintptr_t)5)
#define TIM1                                    ((void *)(uintptr_t)6)
#define GPIOA                                   ((void *)(uintptr_t)7)
#define GPIOB                                   ((void *)(uintptr_t)8)
#define GPIOC                                   ((void *)(uintptr_t)9)
#define GPIOD                                   ((void *)(uintptr_t)10)
#define GPIOE                                   ((void *)(uintptr_t)11)
#define __HAL_RCC_GPIOA_CLK_ENABLE()            ((void)0)
#define __HAL_RCC_GPIOB_CLK_ENABLE()            ((void)0)
#define __HAL_RCC_GPIOC_CLK_ENABLE()            ((void)0)
#define __HAL_RCC_GPIOD_CLK_ENABLE()            ((void)0)
#define __HAL_RCC_GPIOE_CLK_ENABLE()            ((void)0)
#define __HAL_RCC_I2C1_CLK_ENABLE()             ((void)0)
#define __HAL_RCC_I2C2_CLK_ENABLE()             ((void)0)
#define __HAL_RCC_SPI2_CLK_ENABLE()             ((void)0)
#define __HAL_RCC_TIM1_CLK_ENABLE()             ((void)0)
#define __HAL_RCC_USART1_CLK_ENABLE()           ((void)0)
#define __HAL_RCC_USART3_CLK_ENABLE()           ((void)0)
#define __HAL_TIM_SET_COMPARE(...)              ((void)0)
#define __disable_irq()                         ((void)0)
#define __NOP()                                 ((void)0)
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *, uint8_t *, uint16_t);
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *, uint8_t *, uint16_t, uint32_t);
uint32_t HAL_GetTick(void);
int HAL_GPIO_Init();
int HAL_GPIO_WritePin();
int HAL_I2CEx_ConfigAnalogFilter();
int HAL_I2C_Init();
int HAL_I2C_Mem_Read();
int HAL_I2C_Mem_Write();
int HAL_NVIC_EnableIRQ();
int HAL_NVIC_SetPriority();
int HAL_SPI_Init();
int HAL_SPI_Receive();
int HAL_SPI_Transmit();
int HAL_TIM_PWM_ConfigChannel();
int HAL_TIM_PWM_Init();
int HAL_TIM_PWM_Start();
int HAL_TIM_PWM_Stop();
int HAL_UART_Init();
#endif
