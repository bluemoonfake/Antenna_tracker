#include "FreeRTOS.h"
#include "task.h"

#include "board.h"
#include "stm32h7xx_hal.h"

#if !TRACKER_BOOT_ONLY
extern PCD_HandleTypeDef g_usb_pcd;
#endif
extern void xPortSysTickHandler(void);

void NMI_Handler(void) {}

void HardFault_Handler(void)
{
  board_fatal();
}

void MemManage_Handler(void)
{
  board_fatal();
}

void BusFault_Handler(void)
{
  board_fatal();
}

void UsageFault_Handler(void)
{
  board_fatal();
}

void DebugMon_Handler(void) {}

void SysTick_Handler(void)
{
  if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
  {
    xPortSysTickHandler();
  } else
  {
    HAL_IncTick();
  }
}

#if !TRACKER_BOOT_ONLY
void USART1_IRQHandler(void)
{
  HAL_UART_IRQHandler(board_fc_uart_handle());
}

void USART3_IRQHandler(void)
{
#if !TRACKER_BRIDGE_ONLY
  HAL_UART_IRQHandler(board_gps_uart_handle());
#endif
}

void OTG_FS_IRQHandler(void)
{
  HAL_PCD_IRQHandler(&g_usb_pcd);
}
#endif
