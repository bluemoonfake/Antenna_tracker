#include "usbd_conf.h"

#include "usbd_cdc.h"
#include "usbd_core.h"
#include "usb_device.h"

PCD_HandleTypeDef g_usb_pcd;

void HAL_PCD_MspInit(PCD_HandleTypeDef *pcd)
{
  GPIO_InitTypeDef gpio = {0};
  if (pcd->Instance != USB2_OTG_FS)
    return;

  __HAL_RCC_GPIOA_CLK_ENABLE();
  gpio.Pin = GPIO_PIN_11 | GPIO_PIN_12;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio.Alternate = GPIO_AF10_OTG2_FS;
  HAL_GPIO_Init(GPIOA, &gpio);

  __HAL_RCC_USB2_OTG_FS_CLK_ENABLE();
  HAL_PWREx_EnableUSBVoltageDetector();
  HAL_NVIC_SetPriority(OTG_FS_IRQn, 6, 0);
  HAL_NVIC_EnableIRQ(OTG_FS_IRQn);
}

void HAL_PCD_MspDeInit(PCD_HandleTypeDef *pcd)
{
  if (pcd->Instance == USB2_OTG_FS)
  {
    HAL_NVIC_DisableIRQ(OTG_FS_IRQn);
    __HAL_RCC_USB2_OTG_FS_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_11 | GPIO_PIN_12);
  }
}

void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef *pcd)
{
  (void)USBD_LL_SetupStage(pcd->pData, (uint8_t *)pcd->Setup);
}

void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *pcd, uint8_t endpoint)
{
  (void)USBD_LL_DataOutStage(pcd->pData, endpoint, pcd->OUT_ep[endpoint].xfer_buff);
}

void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef *pcd, uint8_t endpoint)
{
  (void)USBD_LL_DataInStage(pcd->pData, endpoint, pcd->IN_ep[endpoint].xfer_buff);
}

void HAL_PCD_SOFCallback(PCD_HandleTypeDef *pcd)
{
  (void)USBD_LL_SOF(pcd->pData);
}

void HAL_PCD_ResetCallback(PCD_HandleTypeDef *pcd)
{
  usb_device_reset_from_isr();
  (void)USBD_LL_Reset(pcd->pData);
  (void)USBD_LL_SetSpeed(pcd->pData, USBD_SPEED_FULL);
}

void HAL_PCD_SuspendCallback(PCD_HandleTypeDef *pcd)
{
  (void)USBD_LL_Suspend(pcd->pData);
}

void HAL_PCD_ResumeCallback(PCD_HandleTypeDef *pcd)
{
  (void)USBD_LL_Resume(pcd->pData);
}

void HAL_PCD_ISOOUTIncompleteCallback(PCD_HandleTypeDef *pcd, uint8_t endpoint)
{
  (void)USBD_LL_IsoOUTIncomplete(pcd->pData, endpoint);
}

void HAL_PCD_ISOINIncompleteCallback(PCD_HandleTypeDef *pcd, uint8_t endpoint)
{
  (void)USBD_LL_IsoINIncomplete(pcd->pData, endpoint);
}

void HAL_PCD_ConnectCallback(PCD_HandleTypeDef *pcd)
{
  (void)USBD_LL_DevConnected(pcd->pData);
}

void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef *pcd)
{
  usb_device_reset_from_isr();
  (void)USBD_LL_DevDisconnected(pcd->pData);
}

USBD_StatusTypeDef USBD_LL_Init(USBD_HandleTypeDef *device)
{
  g_usb_pcd.Instance = USB2_OTG_FS;
  g_usb_pcd.Init.dev_endpoints = 8U;
  g_usb_pcd.Init.use_dedicated_ep1 = 0U;
  g_usb_pcd.Init.dma_enable = 0U;
  g_usb_pcd.Init.low_power_enable = 0U;
  g_usb_pcd.Init.lpm_enable = 0U;
  g_usb_pcd.Init.phy_itface = PCD_PHY_EMBEDDED;
  g_usb_pcd.Init.Sof_enable = 0U;
  g_usb_pcd.Init.speed = PCD_SPEED_FULL;
  g_usb_pcd.Init.vbus_sensing_enable = 0U;
  g_usb_pcd.pData = device;
  device->pData = &g_usb_pcd;

  if (HAL_PCD_Init(&g_usb_pcd) != HAL_OK)
    return USBD_FAIL;
  if (HAL_PCDEx_SetRxFiFo(&g_usb_pcd, 0x80U) != HAL_OK ||
      HAL_PCDEx_SetTxFiFo(&g_usb_pcd, 0U, 0x40U) != HAL_OK ||
      HAL_PCDEx_SetTxFiFo(&g_usb_pcd, 1U, 0x80U) != HAL_OK)
  {
    return USBD_FAIL;
  }
  return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_DeInit(USBD_HandleTypeDef *device)
{
  return HAL_PCD_DeInit(device->pData) == HAL_OK ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_Start(USBD_HandleTypeDef *device)
{
  return HAL_PCD_Start(device->pData) == HAL_OK ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_Stop(USBD_HandleTypeDef *device)
{
  return HAL_PCD_Stop(device->pData) == HAL_OK ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_OpenEP(USBD_HandleTypeDef *device, uint8_t address, uint8_t type,
                                  uint16_t max_packet)
{
  return HAL_PCD_EP_Open(device->pData, address, max_packet, type) == HAL_OK ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_CloseEP(USBD_HandleTypeDef *device, uint8_t address)
{
  return HAL_PCD_EP_Close(device->pData, address) == HAL_OK ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_FlushEP(USBD_HandleTypeDef *device, uint8_t address)
{
  return HAL_PCD_EP_Flush(device->pData, address) == HAL_OK ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_StallEP(USBD_HandleTypeDef *device, uint8_t address)
{
  return HAL_PCD_EP_SetStall(device->pData, address) == HAL_OK ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_ClearStallEP(USBD_HandleTypeDef *device, uint8_t address)
{
  return HAL_PCD_EP_ClrStall(device->pData, address) == HAL_OK ? USBD_OK : USBD_FAIL;
}

uint8_t USBD_LL_IsStallEP(USBD_HandleTypeDef *device, uint8_t address)
{
  PCD_HandleTypeDef *pcd = device->pData;
  return (address & 0x80U) != 0U ? pcd->IN_ep[address & 0x7fU].is_stall
                                 : pcd->OUT_ep[address & 0x7fU].is_stall;
}

USBD_StatusTypeDef USBD_LL_SetUSBAddress(USBD_HandleTypeDef *device, uint8_t address)
{
  return HAL_PCD_SetAddress(device->pData, address) == HAL_OK ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef *device, uint8_t address, uint8_t *data,
                                    uint32_t length)
{
  device->ep_in[address & 0x7fU].total_length = length;
  return HAL_PCD_EP_Transmit(device->pData, address, data, length) == HAL_OK ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef *device, uint8_t address,
                                          uint8_t *data, uint32_t length)
{
  return HAL_PCD_EP_Receive(device->pData, address, data, length) == HAL_OK ? USBD_OK : USBD_FAIL;
}

uint32_t USBD_LL_GetRxDataSize(USBD_HandleTypeDef *device, uint8_t address)
{
  return HAL_PCD_EP_GetRxCount(device->pData, address);
}

void *USBD_static_malloc(uint32_t size)
{
  static uint32_t memory[(sizeof(USBD_CDC_HandleTypeDef) / sizeof(uint32_t)) + 1U];
  (void)size;
  return memory;
}

void USBD_static_free(void *pointer)
{
  (void)pointer;
}

void USBD_LL_Delay(uint32_t delay_ms)
{
  HAL_Delay(delay_ms);
}
