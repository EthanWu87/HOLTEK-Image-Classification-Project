/*********************************************************************************************************//**
 * @file    ht32f493x5_int.c
 * @version $Rev:: 143         $
 * @date    $Date:: 2025-05-08 #$
 * @brief   main interrupt service routines.
 *************************************************************************************************************
 * @attention
 *
 * Firmware Disclaimer Information
 *
 * 1. The customer hereby acknowledges and agrees that the program technical documentation, including the
 *    code, which is supplied by Holtek Semiconductor Inc., (hereinafter referred to as "HOLTEK") is the
 *    proprietary and confidential intellectual property of HOLTEK, and is protected by copyright law and
 *    other intellectual property laws.
 *
 * 2. The customer hereby acknowledges and agrees that the program technical documentation, including the
 *    code, is confidential information belonging to HOLTEK, and must not be disclosed to any third parties
 *    other than HOLTEK and the customer.
 *
 * 3. The program technical documentation, including the code, is provided "as is" and for customer reference
 *    only. After delivery by HOLTEK, the customer shall use the program technical documentation, including
 *    the code, at their own risk. HOLTEK disclaims any expressed, implied or statutory warranties, including
 *    the warranties of merchantability, satisfactory quality and fitness for a particular purpose.
 *
 * <h2><center>Copyright (C) Holtek Semiconductor Inc. All rights reserved</center></h2>
 ************************************************************************************************************/

/* includes ------------------------------------------------------------------*/
#include "ht32f493x5_board.h"
#include "ht32f493x5_int.h"
#include "hm01b0.h"

__IO uint32_t uwTick = 0; 

/** @addtogroup HT32F493x5_periph_examples
  * @{
  */

/** @addtogroup HT32F493x5_GPIO_led_toggle
  * @{
  */

/**
  * @brief  this function handles nmi exception.
  * @param  none
  * @retval none
  */
void NMI_Handler(void)
{
}

/**
  * @brief  this function handles hard fault exception.
  * @param  none
  * @retval none
  */
void HardFault_Handler(void)
{
  /* go to infinite loop when hard fault exception occurs */
  while(1)
  {
  }
}

/**
  * @brief  this function handles memory manage exception.
  * @param  none
  * @retval none
  */
void MemManage_Handler(void)
{
  /* go to infinite loop when memory manage exception occurs */
  while(1)
  {
  }
}

/**
  * @brief  this function handles bus fault exception.
  * @param  none
  * @retval none
  */
void BusFault_Handler(void)
{
  /* go to infinite loop when bus fault exception occurs */
  while(1)
  {
  }
}

/**
  * @brief  this function handles usage fault exception.
  * @param  none
  * @retval none
  */
void UsageFault_Handler(void)
{
  /* go to infinite loop when usage fault exception occurs */
  while(1)
  {
  }
}

/**
  * @brief  this function handles svcall exception.
  * @param  none
  * @retval none
  */
void SVC_Handler(void)
{
}

/**
  * @brief  this function handles debug monitor exception.
  * @param  none
  * @retval none
  */
void DebugMon_Handler(void)
{
}

/**
  * @brief  this function handles pendsv_handler exception.
  * @param  none
  * @retval none
  */
void PendSV_Handler(void)
{
}

/**
  * @brief  this function handles systick handler.
  * @param  none
  * @retval none
  */
void SysTick_Handler(void)
{
	uwTick++;             
	
	/* toggle led */
  if((uwTick % 200) == 0)
  {
    ht32_led_toggle(LED3);
  }
}

uint32_t SysTick_GetTick(void)
{
	return uwTick;   
}

/**
  * @brief  VSYNC interrupt
  *         PA2 / EXINT2 / rising edge
  *
  *         Full-image behavior:
  *          - reset HREF debug counter
  *          - stop DMA
  *          - reload full frame = 164 * 122 = 20008 bytes
  *          - start DMA
  *
  *         Because HM01B0 gated PCLK is enabled, DMA requests only
  *         occur when PCLK capture events are present.
  * @param  none
  * @retval none
  */
void EXINT2_IRQHandler(void)
{
  if(exint_interrupt_flag_get(HM01B0_VSYNC_EXINT_LINE) != RESET)
  {
		if(g_hm01b0.frame_ready == 0)
    {
      dma_channel_enable(HM01B0_DMA_CHANNEL,FALSE);

      dma_data_number_set(HM01B0_DMA_CHANNEL, HM01B0_IMAGE_SIZE_BYTES);

      dma_channel_enable(HM01B0_DMA_CHANNEL, TRUE);
    }
		
    exint_flag_clear(HM01B0_VSYNC_EXINT_LINE);
  }
}

/**
  * @brief  HREF interrupt
  *         PA4 / EXINT4 / rising edge
  *
  *         Does nothing, because HM01B0 gated PCLK is enabled.
  * @param  none
  * @retval none
  */
void EXINT4_IRQHandler(void)
{
  if(exint_interrupt_flag_get(HM01B0_HREF_EXINT_LINE) != RESET)
  {
		exint_flag_clear(HM01B0_HREF_EXINT_LINE);
  }
}

/**
  * @brief  DMA1 Channel2 full-data-transfer interrupt
  * @param  none
  * @retval none
  */
void DMA1_Channel2_IRQHandler(void)
{
  if(dma_interrupt_flag_get(HM01B0_DMA_FDT_FLAG) != RESET)
  {

    /* Stop DMA immediately after full frame */
    dma_channel_enable(HM01B0_DMA_CHANNEL, FALSE);

    g_hm01b0.frame_ready = 1;

    dma_flag_clear(HM01B0_DMA_FDT_FLAG);
  }
}
/**
  * @}
  */

/**
  * @}
  */
